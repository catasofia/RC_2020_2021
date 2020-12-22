#include "packet-format.h"
#include <limits.h>
#include <netdb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


int missed_ack = 0;
int base = 0;
int packets_sent = 0;
int nextToReceive = 1;
int lastPacket = -1;

int sendingSegments(int sockfd, data_pkt_t data_pkt, size_t data_len, struct sockaddr_in srv_addr);
void resendPackets(int window, FILE *file, data_pkt_t data_pkt, int sockfd, struct sockaddr_in srv_addr);


int main(int argc, char *argv[]) {
  char *file_name = argv[1];
  char *host = argv[2];
  int port = atoi(argv[3]);
  int window = atoi(argv[4]);

  if(window > MAX_WINDOW_SIZE){
    perror("window size > 32");
    exit(-1);
  }

  FILE *file = fopen(file_name, "r");
  if (!file) {
    perror("fopen");
    exit(-1);
  }

  // Prepare server host address.
  struct hostent *he;
  if (!(he = gethostbyname(host))) {
    perror("gethostbyname");
    exit(-1);
  }

  struct sockaddr_in srv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = *((struct in_addr *)he->h_addr),
  };

  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;

  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  uint32_t seq_num = 0;
  data_pkt_t data_pkt;
  size_t data_len;
  
  do { // Generate segments from file, until the the end of the file.

    for(int i = 0; i < window && !feof(file) ; i++){
      if(fseek(file, packets_sent * MAX_SIZE, SEEK_SET) != 0)
        printf("fseek error\n"); 
      
      // Prepare data segment.
      data_pkt.seq_num = htonl(seq_num++);
      
      // Load data from file.
      data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);
      
      // Send segment.
      if(sendingSegments(sockfd, data_pkt, data_len, srv_addr)){
        packets_sent++;
      }
    } 

    ack_pkt_t ack_pkt;
    int received_bytes;
    while(1){ 
      received_bytes = recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&srv_addr, &(socklen_t){sizeof(srv_addr)});

      if(received_bytes > 0){  //received a ack
        if(ntohl(ack_pkt.seq_num) == nextToReceive){ //receive the expected
          missed_ack = 0;
          base++;
          nextToReceive++;
        
          printf("received acknowledgment %d // ack number = %u\n", ntohl(ack_pkt.seq_num), ntohl(ack_pkt.selective_acks));

          if(lastPacket != -1 && (lastPacket + 1) == ntohl(ack_pkt.seq_num)) break;

          if(base == packets_sent) break;
          
          else if (!(feof(file))){
            data_pkt.seq_num = htonl(seq_num++);
            data_len = fread(&data_pkt.data, 1, sizeof(data_pkt.data), file);
            
            if(sendingSegments(sockfd, data_pkt, data_len, srv_addr)){
              packets_sent++;
            }
          }
        }
        else if((ntohl(ack_pkt.seq_num) > nextToReceive) && ntohl(ack_pkt.selective_acks) == 0){
          printf("received acknowledgment %d // ack number = %u\n", ntohl(ack_pkt.seq_num), ntohl(ack_pkt.selective_acks));
          seq_num = nextToReceive;
          nextToReceive = ntohl(ack_pkt.seq_num);
    
          if(fseek(file, seq_num*MAX_SIZE, SEEK_SET) != 0)
            printf("fseek error\n"); 

          if(lastPacket != -1 && (lastPacket + 1) == ntohl(ack_pkt.seq_num)) break;

          for (int i = 0; i < window && !feof(file); i++){
            data_pkt.seq_num = htonl(seq_num);
            data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);
            if(sendingSegments(sockfd, data_pkt, data_len, srv_addr))
              seq_num++;
          }
        }
      }
      
      else{
        missed_ack++;
        printf("Failed attempts: %d. Missing %d to reach the maximum.\n", missed_ack, MAX_RETRIES - missed_ack);

        if(missed_ack == MAX_RETRIES){
          perror("3 consecutive timeouts.\n");
          exit(EXIT_FAILURE);
        }
        else if(ack_pkt.selective_acks == 0){  //stop and wait and go back n or selective repeat if everything was lost;
          //needs to send again, failed to receive the acks
          if(fseek(file, base * MAX_SIZE, SEEK_SET) != 0) //sets the file position where it needs to start sending data
            printf("fseek error\n"); 
          resendPackets(window, file, data_pkt, sockfd, srv_addr);
        }

        else{     // selective repeat
          uint32_t num = ntohl(ack_pkt.selective_acks);

          if(fseek(file, ntohl(ack_pkt.seq_num)*MAX_SIZE, SEEK_SET) != 0)   //sets the position to send the beggining of the receiver window; 
            printf("fseek error\n"); 

          data_pkt.seq_num = ack_pkt.seq_num;
          data_len = fread(&data_pkt.data, 1, sizeof(data_pkt.data), file);

          sendingSegments(sockfd, data_pkt, data_len, srv_addr);
          

          int limit = ntohl(ack_pkt.seq_num) + window;
          for(int i = ntohl(ack_pkt.seq_num); i < limit && !feof(file); i++){
            if(lastPacket == ntohl(ack_pkt.seq_num))
              break;

            if (num & (1 << (i - 1))) continue;   //bit set
            
            else{//bit not set
              if(fseek(file, i * MAX_SIZE, SEEK_SET) != 0)
                printf("fseek error\n"); 

              data_pkt.seq_num = htonl(i);
              data_len = fread(&data_pkt.data, 1, sizeof(data_pkt.data), file);
              sendingSegments(sockfd, data_pkt, data_len, srv_addr);
            }
          }
        }
      }
      if(lastPacket != -1 && (lastPacket + 1) == ntohl(ack_pkt.seq_num)) break;
    }
  } while (base == packets_sent && !(feof(file) && data_len < sizeof(data_pkt.data)));

  // Clean up and exit.
  close(sockfd);
  fclose(file);

  return 0;
}

int sendingSegments(int sockfd, data_pkt_t data_pkt, size_t data_len, struct sockaddr_in srv_addr){
  
  ssize_t sent_len =
      sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,
            (struct sockaddr *)&srv_addr, sizeof(srv_addr));
  
  printf("Sending segment %d, size %ld.\n", ntohl(data_pkt.seq_num),
        offsetof(data_pkt_t, data) + data_len);

  if (sent_len != offsetof(data_pkt_t, data) + data_len) {
        fprintf(stderr, "Truncated packet.\n");
        exit(-1);
  }
  if(data_len < sizeof(data_pkt.data))
    lastPacket = ntohl(data_pkt.seq_num);
  
  return 1;
}


void resendPackets(int window, FILE *file, data_pkt_t data_pkt, int sockfd, struct sockaddr_in srv_addr){
  size_t data_len;
  int x = base;

  for(int i = 0; i < window && !feof(file); i++){
    data_pkt.seq_num = htonl(x);
    data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);
    if(sendingSegments(sockfd, data_pkt, data_len, srv_addr))
      x++;
  }
}