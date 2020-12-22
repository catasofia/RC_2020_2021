/******************************************************************************\
* Distance vector routing protocol with reverse path poisoning.                *
\******************************************************************************/

#include <stdlib.h>

#include "routing-simulator.h"

// Message format to send between nodes.
typedef struct {
  //list of new costs that is sended to neighbours;
  cost_t newCost[MAX_NODES];
  //list of hops to each node
  node_t next_hop[MAX_NODES];
} message_t;

// State format.
typedef struct {
  //matrix of costs for bellman ford;
  cost_t costMatrix[MAX_NODES][MAX_NODES];
  //list of hops to each node
  node_t next_hop[MAX_NODES];
} state_t;


int bellman_ford(state_t* state){
  
  /* BELLMAN-FORD EQUATION
  dx(y) = min{cv(x,v) + dv(y)}
  x = current_node
  y = node
  v = neighbour n
  */

  int update = 0;
  
  for(node_t node = get_first_node(); node <= get_last_node(); node++){
    
    if(node != get_current_node()){
    
      cost_t cost = COST_INFINITY;
      node_t next = -1;
      
      //neighbours n of current node
      for(node_t n = get_first_node(); n <= get_last_node(); n++){
        if(n != get_current_node()){
          if(COST_ADD(get_link_cost(n), state->costMatrix[n][node]) < cost){
            //min between cost from current to neighbour n plus cost from neighbour n to destination node 
            //and previous cost
            cost = COST_ADD(get_link_cost(n), state->costMatrix[n][node]);
            next = n;
          }
        }
      }

      //after checking all neighbours
      //needs to update the state? Found a different cost for dx(y)?
      if(cost != state->costMatrix[get_current_node()][node]){
        state->costMatrix[get_current_node()][node] = cost;
        state->next_hop[node] = next;
        //sets new route via next hop
        set_route(node, next, cost);
        update = 1;
      }
    }
  }
  return update;
}


// Notify a node that a neighboring link has changed cost.
void notify_link_change(node_t neighbor, cost_t new_cost) {
  
  if(!get_state()){
    state_t *state_aux = (state_t*)malloc(sizeof(state_t));
    for(int i = 0; i < MAX_NODES; i++){
      for(int j = 0; j < MAX_NODES; j++){
        if(i == j) //diagonal
          state_aux->costMatrix[i][j] = 0;
        else 
          state_aux->costMatrix[i][j] = COST_INFINITY; 
      }
      state_aux->next_hop[i] = -1;
    }
    set_state(state_aux);
  }
  
  state_t *state = (state_t *) get_state();
  
  if(bellman_ford(state)){
    //send message to neighbours
    for(node_t node = get_first_node(); node <= get_last_node(); node++){
      //only sends to neighbours, not me
      if(node != get_current_node() && get_link_cost(node) < COST_INFINITY){
        message_t *message = (message_t*)malloc(sizeof(message_t));
        for(int i = 0; i < MAX_NODES; i++){
          message->newCost[i] = state->costMatrix[get_current_node()][i];
          message->next_hop[i] = state->next_hop[i];
        }
        send_message(node, message);
      }
    }
  }
}

// Receive a message sent by a neighboring node.
void notify_receive_message(node_t sender, void *message) {
  
  message_t *mes = (message_t*) message;
  
  if(!get_state()){
    state_t *state_aux = (state_t*)malloc(sizeof(state_t));
    for(int i = 0; i < MAX_NODES; i++){
      for(int j = 0; j < MAX_NODES; j++){
        if(i == j) //diagonal
          state_aux->costMatrix[i][j] = 0;
        else 
          state_aux->costMatrix[i][j] = COST_INFINITY; 
      }
      state_aux->next_hop[i] = -1;
    }
    set_state(state_aux);
  }
  
  state_t *state = (state_t *) get_state();

  //updates according to message received
  for(int i = 0; i < MAX_NODES; i++){
    if(get_current_node() != mes->next_hop[i]){
      state->costMatrix[sender][i] = mes->newCost[i];
      state->next_hop[i] = mes->next_hop[i];
    }
    else{
      state->costMatrix[sender][i] = COST_INFINITY;
      state->next_hop[i] = -1;
    }
  }

  if(bellman_ford(state)){
    //send message to neighbours
    for(node_t node = get_first_node(); node <= get_last_node(); node++){
      //only sends to neighbours, not me
      if(node != get_current_node() && get_link_cost(node) < COST_INFINITY){
        message_t *message = (message_t*)malloc(sizeof(message_t));
        for(int i = 0; i < MAX_NODES; i++){
          message->newCost[i] = state->costMatrix[get_current_node()][i];
          message->next_hop[i] = state->next_hop[i];
        }
        send_message(node, message);
      }
    }
  }
}