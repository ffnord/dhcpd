#include "dht.h"


void dht_cb(EV_P_ ev_io *w, int revents) {

  struct iovec buffer_vec = { .iov_base = buffer, .iov_len = sizeof(buffer) };
  char cbuf[1024];

  struct msghdr message;
  message.msg_control = cbuf;
  message.msg_controllen = sizeof(cbuf); 
  message.msg_iov = &buffer_vec;
  message.msg_iovlen = 1;

  struct sockaddr_in6 recvaddr;
  message.msg_name    = &recvaddr;
  message.msg_namelen = sizeof(struct sockaddr_in6);

  ssize_t n = recvmsg(w->fd,&message,0);
  
  const char *end = (char*)message.msg_control + message.msg_controllen;

  struct cmsghdr *cmsg;
  for (cmsg = CMSG_FIRSTHDR(&message); cmsg; cmsg = CMSG_NXTHDR(&message, cmsg)) {
    if ((char*)cmsg + sizeof(*cmsg) > end)
      return;
    if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO) {
      struct in6_pktinfo *pktinfo = (struct in6_pktinfo*)CMSG_DATA(cmsg);
      if ((char*)pktinfo + sizeof(*pktinfo) > end)
        return;

      if (IN6_IS_ADDR_MULTICAST(&pktinfo->ipi6_addr)){
        // Call multicast packet handler
        dht_multicast(loop,w,revents,recvaddr);
      } else if (IN6_IS_ADDR_LINKLOCAL(&pktinfo->ipi6_addr)) {
        // unicast packet handler
        dht_unicast(loop,w,revents,recvaddr);
      }
      
      return;
    }
  }
}

/**
 * Handling of multicast packets
 */
void dht_multicast(EV_P_ ev_io *w, int revents,struct sockaddr_in6 recvaddr){
  // Transform buffer to packed
  struct dht_msg *msg = (struct dht_msg*) buffer;

  char addr[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6,&recvaddr.sin6_addr,addr);
  printf("Multicast from [%s] with type %i \n",addr,msg->type);

  switch(msg->type) {
    case DHT_NEIGHBOR_LOOKUP:
      dht_multicast_neighbor_lookup(msg);
      break;
  }

  switch(dht_current_state) {
    case DHT_STATE_BOOT:
      break;
    case DHT_STATE_INIT:
    case DHT_STATE_RUNNING:
      break;
    default:
      dhcpd_error(0,0,"Received unknown multicast msg");
  }

  // When neighbors are discovered stop sending multicast via timer
  // ev_timer_stop(dht_multicast_timer);
}

void dht_multicast_neighbor_lookup(struct dht_msg* inmsg){
  printf("Neightbor lookup\n");
	struct sockaddr_in6 recvsockaddr = {
		.sin6_family   = AF_INET6,
		.sin6_port     = htons(4343),
	};

  memcpy(&recvsockaddr.sin6_addr,&inmsg->data,sizeof(struct in6_addr));

  // Prepare msg
  int msg_size = sizeof(struct dht_msg) + sizeof(dht_unicast_address);
  struct dht_msg * msg = (struct dht_msg*) malloc(msg_size); 
  msg->type     = DHT_NEIGHBOR_ANNOUNCE;
  memcpy(msg->hash,&dist_hash_table.dht_node_hash,4);
  msg->len      = sizeof(dht_unicast_address);
  memcpy(msg->data,&dht_unicast_address,sizeof(dht_unicast_address));

  if(sendto(dht_socket,(char*) msg,msg_size,0,(struct sockaddr *)&recvsockaddr,sizeof(recvsockaddr)) == -1){
      dhcpd_error(0,1,"sendto: failed to send msg (%m)");
  }
}

void dht_multicast_neighbor_announcement(struct dht_msg* msg){
  printf("Neightbor announcement\n");

  struct in6_addr neighbor_addr;
  memcpy(&neighbor_addr,&msg->data,sizeof(struct in6_addr));

  struct dht_neighbor * neighbor = (struct dht_neighbor*) malloc(sizeof(struct dht_neighbor));

  neighbor->address = neighbor_addr;
  memcpy(&neighbor->hash,msg->hash,4);

  neighbor->next = dist_hash_table.neighbors;

  dist_hash_table.neighbors = neighbor;
  printf("Learned new neighbor\n");

  if(dist_hash_table.neighbors->next == NULL) {
    if(dht_current_state == DHT_STATE_BOOT) {
      printf("Change State to INIT\n");
      dht_current_state = DHT_STATE_INIT;
    }
  }
}

void dht_unicast(EV_P_ ev_io *w, int revents,struct sockaddr_in6 recvaddr){
  struct dht_msg *msg = (struct dht_msg*) buffer;
  switch(msg->type) {
    case DHT_NEIGHBOR_ANNOUNCE:
      dht_multicast_neighbor_announcement(msg);
      break;
  }
}

/** 
 * Multicast Timer 
 *
 * In BOOT state this timer will send periodic (10sec) Neighbor lookups,
 * after a defined time running and not deactivated it will decide
 * that we are alone and initialise the zone setup.
 *
 * TODO Let it send neightbor lookups with a higher period (60s?)??
 */
void dht_multicast_timer_cb(struct ev_loop *loop, ev_timer *w, int revents){
  if ( dht_current_state == DHT_STATE_BOOT ) {

    printf("Searching for neighbors\n");

    // Prepare msg
    struct dht_msg * msg = (struct dht_msg*) malloc(sizeof(struct dht_msg) + sizeof(dht_unicast_address)); 
    msg->type     = DHT_NEIGHBOR_LOOKUP;
    msg->len      = sizeof(dht_unicast_address);
    memcpy(msg->data,&dht_unicast_address,sizeof(dht_unicast_address));

    if(sendto(dht_socket,(char*) msg,sizeof(msg),0,(struct sockaddr *)&dht_multicast_sockaddr,sizeof(dht_multicast_sockaddr)) == -1){
      dhcpd_error(0,1,"sendto: failed to send msg (%m)");
    }

    free(msg);

    // Reset timer when we haven't reach the inital search time
    if(dht_boot_wait < 60) {
      float time = (float) 7.5 + (float) (random() % 50) / 10 ;
      w->repeat = time;
      ev_timer_again (loop,w);
      dht_boot_wait += time;
    } else {
      // We have reached the point where we assume, that we are alone 
      // in the network, setup the zone.
      char* zonemask = (char*){0x0000};
      //dht_zone_setup(zonemask);
      dht_current_state = DHT_STATE_RUNNING;
      ev_timer_stop(loop,w);
    }
  } else {
    // For some reason we have not been deactivated.
    ev_timer_stop(loop,w);
  }
}
