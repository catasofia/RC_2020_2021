#include "packet-format.h"
#include <arpa/inet.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>


int base = 0;
int lastPkt = -1;
int allReceived = false;

ack_pkt_t createAck();



int main(int argc, char *argv[]) {
  char *file_name = argv[1];
  int port = atoi(argv[2]);
  int window = atoi(argv[3]);

  if(window > MAX_WINDOW_SIZE){
    perror("window size > 32");
    exit(-1);
  }

  FILE *file = fopen(file_name, "w");
  if (!file) {
    perror("fopen");
    exit(-1);
  }


  // Prepare server socket.
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    perror("socket");
    exit(-1);
  }

  // Allow address reuse so we can rebind to the same port,
  // after restarting the server.

  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) <
      0) {
    perror("setsockopt");
    exit(-1);
  }

  struct sockaddr_in srv_addr = {
      .sin_family = AF_INET,
      .sin_addr.s_addr = htonl(INADDR_ANY),
      .sin_port = htons(port),
  };

  if (bind(sockfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr))) {
    perror("bind");
    exit(-1);
  }

  fprintf(stderr, "Receiving on port: %d\n", port);
  ssize_t len;
  

  if(window == 1){
    do { // Iterate over segments, until last the segment is detected.
      // Receive segment.

      struct sockaddr_in src_addr;
      data_pkt_t data_pkt;
      do{
          len =
            recvfrom(sockfd, &data_pkt, sizeof(data_pkt), 0,
                      (struct sockaddr *)&src_addr, &(socklen_t){sizeof(src_addr)});

          printf("Received segment %d, size %ld.\n", ntohl(data_pkt.seq_num), len);

          if(ntohl(data_pkt.seq_num) == base){   //received the base window, update it
            base++;
            ack_pkt_t ack_pkt = createAck();

            if(sendto(sockfd, &ack_pkt, sizeof(ack_pkt_t), 0, (struct sockaddr *)&src_addr, sizeof(src_addr)) == -1)
              printf("sending error\n");
            break;
          }
          ack_pkt_t ack_pkt = createAck();
          if(sendto(sockfd, &ack_pkt, sizeof(ack_pkt_t), 0, (struct sockaddr *)&src_addr, sizeof(src_addr)) == -1)
            printf("sending error\n");

      } while(ntohl(data_pkt.seq_num) != base);
      fwrite(data_pkt.data, 1, len - offsetof(data_pkt_t, data), file);
    } while (len == sizeof(data_pkt_t));
  }

  else if(window > 1){
    uint32_t selective_acks_aux = 0;
    do{
      struct sockaddr_in src_addr;
      data_pkt_t data_pkt;

      for(int i = 0; i < window; i++){
        len =
            recvfrom(sockfd, &data_pkt, sizeof(data_pkt), 0,
                      (struct sockaddr *)&src_addr, &(socklen_t){sizeof(src_addr)});
        
        printf("Received segment %d, size %ld.\n", ntohl(data_pkt.seq_num), len); 
        ack_pkt_t ack_pkt = createAck();

        if(ntohl(data_pkt.seq_num) == base){   //received the base window

          base++;

          if(selective_acks_aux != 0){
            for(int i = 0; i < window; i++){
              if (ntohl(selective_acks_aux) & (1 << (i - 1))) //bit set
                base++; 
            }
          }

          int position = ntohl(data_pkt.seq_num);
          ack_pkt.seq_num = htonl(base);

          if(fseek(file, position * MAX_SIZE, SEEK_SET) != 0)
            printf("fseek error\n"); 

          fwrite(data_pkt.data, 1, len - offsetof(data_pkt_t, data), file);
          if(sendto(sockfd, &ack_pkt, sizeof(ack_pkt_t), 0, (struct sockaddr *)&src_addr, sizeof(src_addr)) == -1)
            printf("sending error\n");
          
          if(len != sizeof(data_pkt_t)) 
            lastPkt = ntohl(data_pkt.seq_num) + 1;
          
          for(int i = 0; i < window; i++){
            if (selective_acks_aux & (1 << (i - 1))) allReceived = true;   //bit set
            else allReceived = false;
          }

          if(allReceived && lastPkt != -1){
            base = lastPkt;
          }
            
        }

        else if(ntohl(data_pkt.seq_num) <= base + window - 1){  //received not the base, but one in the window
          int set = ntohl(data_pkt.seq_num) - 1;
          int position = ntohl(data_pkt.seq_num);
          
          selective_acks_aux = selective_acks_aux | 1 << set;
          ack_pkt.selective_acks = htonl(selective_acks_aux);
          ack_pkt.seq_num = htonl(base);
          
          if(fseek(file, position * MAX_SIZE, SEEK_SET) != 0)
            printf("fseek error\n"); 
          
          fwrite(data_pkt.data, 1, len - offsetof(data_pkt_t, data), file);

          if(sendto(sockfd, &ack_pkt, sizeof(ack_pkt_t), 0, (struct sockaddr *)&src_addr, sizeof(src_addr)) == -1)
            printf("sending error\n");
          
          if(len != sizeof(data_pkt_t)) lastPkt = ntohl(data_pkt.seq_num);
        }
        
        else{ //outside the window;
          ack_pkt.seq_num = htonl(base);
          if(sendto(sockfd, &ack_pkt, sizeof(ack_pkt_t), 0, (struct sockaddr *)&src_addr, sizeof(src_addr)) == -1)
            printf("sending error\n");

          continue;
        }

        if((lastPkt > -1 && lastPkt == base))       
          break;
      }
    } while(!(lastPkt > -1 && lastPkt == base));
  }
  // Clean up and exit.
  close(sockfd);
  fclose(file);

  return 0;
}

ack_pkt_t createAck(){
  ack_pkt_t ack;
  ack.seq_num = htonl(base);
  ack.selective_acks = 0;
  return ack;
}
