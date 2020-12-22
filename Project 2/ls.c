/******************************************************************************\
* Link state routing protocol.                                                 *
\******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "routing-simulator.h"

typedef struct {
  cost_t link_cost[MAX_NODES];
  int version;
} link_state_t;

// Message format to send between nodes.
typedef struct {
  link_state_t ls[MAX_NODES];
} message_t;

// State format.
typedef struct {
  link_state_t ls[MAX_NODES];
  //node_t N[MAX_NODES];
  node_t next[MAX_NODES];
} state_t;


int check_not_in_list(node_t node, node_t list[MAX_NODES]){
  for(int i = 0; i < MAX_NODES; i++){
    if (list[i] == node)
      return 0;
  }
  return -1;
}


node_t check_minimum(int cost[MAX_NODES], node_t N[MAX_NODES]){
  node_t min = -1;
  cost_t value = COST_INFINITY;
  for(int i = get_first_node(); i <= get_last_node(); i++){
    if(check_not_in_list(i, N) == -1){
      if(cost[i] < value){
        min = i;
        value = cost[i];
      }
    }
  }
  return min;
}


void dijsktra(state_t *state){
  
  //initialization:
  node_t N[MAX_NODES];
  node_t next[MAX_NODES];
  
  int position = 1;
  int cost[MAX_NODES];

  for(int i = 0; i < MAX_NODES; i++){
    next[i] = -1;
    N[i] = -1;
    if(i != get_current_node())
      cost[i] = get_link_cost(i);
    else
      cost[i] = 0;
  }

  N[0] = get_current_node();
  //next[0] = get_current_node();
  //cost[0] = 0;

  //loop
  while(check_minimum(cost, N) != -1){
    node_t min = check_minimum(cost, N);
    N[position] = min;

    /* if(cost[min] != state->ls[get_current_node()].link_cost[min] && next[min] != -1){
      set_route(min, next[min], cost[min]);
      state->ls[min].version++;
      position++;
    } */
    
    for(node_t v = get_first_node(); v <= get_last_node(); v++){
      if((check_not_in_list(v, N)) && (state->ls[min].link_cost[v] != COST_INFINITY)){
        if(COST_ADD(cost[min], state->ls[min].link_cost[v]) < cost[v]){
          cost[v] = COST_ADD(cost[min], state->ls[min].link_cost[v]);
          next[v] = min;
        }
      }
    }
  }
  for(int i = 0; i < MAX_NODES; i++){
    if(next[i] == -1) break;
    set_route(N[i], next[i], cost[i]); 
  }
}

// Notify a node that a neighboring link has changed cost.
void notify_link_change(node_t neighbor, cost_t new_cost) {
  
  if(get_state() == NULL){
    state_t *state_aux = (state_t*)malloc(sizeof(state_t));
    for(int i = 0; i < MAX_NODES; i++){
      state_aux->ls[i].version = 0;
      for(int j = 0; j < MAX_NODES; j++){
        if(i == j)
          state_aux->ls[i].link_cost[j] = 0;
        else
          state_aux->ls[i].link_cost[j] = COST_INFINITY;
      }
    }
    set_state(state_aux);
  }
  
  state_t *state = (state_t *) get_state();

  dijsktra(state);

  for(node_t node = get_first_node(); node <= get_last_node(); node++){
    //sends to neighbours
    if(node != get_current_node() && get_link_cost(node) < COST_INFINITY){
      message_t *message = (message_t*)malloc(sizeof(message_t));
      for(int i = 0; i < MAX_NODES; i++){
        message->ls[get_current_node()].link_cost[i] = state->ls[get_current_node()].link_cost[i];
      }
      send_message(node, message);
    }
  }
}

// Receive a message sent by a neighboring node.
void notify_receive_message(node_t sender, void *message) {

  message_t *mes = (message_t *) message;
  
  if(get_state() == NULL){
    state_t *state_aux = (state_t*)malloc(sizeof(state_t));
    for(int i = 0; i < MAX_NODES; i++){
      state_aux->ls[i].version = 0;
      for(int j = 0; j < MAX_NODES; j++){
        if(i == j)
          state_aux->ls[i].link_cost[j] = 0;
        else
          state_aux->ls[i].link_cost[j] = COST_INFINITY;
      }
    }
    set_state(state_aux);
  }

  state_t *state = (state_t *) get_state();

  if(mes->ls[sender].version > state->ls[sender].version){
    for(int i = 0; i < MAX_NODES; i++){
      state->ls[sender].link_cost[i] = mes->ls[sender].link_cost[i];
      state->ls[sender].version = mes->ls[sender].version;
    }

    dijsktra(state);

    for(node_t node = get_first_node(); node <= get_last_node(); node++){
      //only sends to neighbours, not me
      if(node != get_current_node() && get_link_cost(node) < COST_INFINITY){
        message_t *message = (message_t*)malloc(sizeof(message_t));
        for(int i = 0; i < MAX_NODES; i++){
          message->ls[get_current_node()].link_cost[i] = state->ls[get_current_node()].link_cost[i];
        }
        send_message(node, message);
      }
    }
  }
}
