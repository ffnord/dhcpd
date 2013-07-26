#include "dht.h"

/* Global state */

// get IPv6 link local for an interface

// Zone Setup
void dht_zone_setup(char* zonemask){
  dhcpd_error(0,0,"Setting zonemask to %02x", zonemask[0]);
  dist_hash_table.zonemask = zonemask;
}

// Unicast Socket Callback
//static void dht_cb(EV_P_ ev_io *w, int revents){
//  puts("Got unicast message");
//  ssize_t n = recvfrom(w->fd,buffer,sizeof(buffer),0,NULL,NULL);
//  struct dht_msg* msg = (struct dht_msg*) buffer;
//  printf("Msg(%i)[Type:%i,Hash:,Len:%i]\n",n,msg->type,msg->len);
//
//  switch(dht_current_state){
//    case DHT_STATE_BOOT:
//      // Messages recieved in this state should
//      // be neighbor informations 
//
//      // dht_neighbor_append();
//      // dht_current_state = DHT_STATE_INIT;
//      break;
//    case DHT_STATE_INIT:
//      break;
//    case DHT_STATE_RUNNING:
//      break;
//  }
//}

int dht_get(char *hash,void **data){
  return -1;
}

int dht_set(char *hash,void *data){
  return -1;
}

static inline int max(int a,int b){
  return (a > b ? a : b);
}

int main(int argc,char** args){
  if(argc <= 2) {
    dhcpd_error(1,22,"%s <interface> <port>",args[0]);
    return 0;
  }
  interface = args[1];
  port = atoi(args[2]);
  puts(args[2]);
  printf("Starting DHT on Port %i\n",port);
  dht_init(1,port);
}
