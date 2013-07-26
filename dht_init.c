#include "dht.h"

struct in6_addr * if_nametolinklocal(char* interface){
  struct ifaddrs *ifaddr, *ifa;
  int family, s;

  if (getifaddrs(&ifaddr) == -1) {
    dhcpd_error(1,0,"Error can't determine ipv6 link local for interface %s",interface);
  }

  // TODO check if interface is still there
  int ifindex = if_nametoindex(interface);

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL)
      continue;
    if(strcmp(ifa->ifa_name,interface) == 0) {
      if(ifa->ifa_addr->sa_family == AF_INET6){
        if(ifa->ifa_addr != NULL){
          struct sockaddr_in6 *addr = (struct sockaddr_in6*) ifa->ifa_addr;
          if(IN6_IS_ADDR_LINKLOCAL(&(addr->sin6_addr))){
            puts(ifa->ifa_name);
            struct in6_addr *ip6addr = (struct in6_addr*) malloc(sizeof(struct in6_addr));
            memcpy(ip6addr,&addr->sin6_addr,sizeof(struct in6_addr));
            freeifaddrs(ifaddr);
            return ip6addr; // TODO Fix types
          }
        }
      }
    }
  }
  return NULL;
}

/**
 * DHT Initialisation and Network Setup
 */
int dht_init(unsigned int hash_width, unsigned int port){
  // Initialise DHT structure
  dist_hash_table.hash_width = hash_width;
  dist_hash_table.neighbors  = NULL;
  dist_hash_table.data       = NULL;

  // Setup Network ------------------------------------------------------------

  // Determine multicast address
  inet_pton(AF_INET6, "ff12::0043", &dht_multicast_address);

  // Create socket
  if((dht_socket = socket(AF_INET6,SOCK_DGRAM,0)) < 0){
      dhcpd_error(1,0,"Could not create socket");
  }

  unsigned int ifindex = if_nametoindex(interface); 
  if(setsockopt(dht_socket, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex,sizeof(ifindex))){
    dhcpd_error(1,0,"setsockopt: unable to set SO_REUSEADDR");
  }

  // Determine our address
  struct in6_addr *linklocal = if_nametolinklocal(interface);
  dht_unicast_address = *linklocal;
  free(linklocal);

  // Define multicast dht
  dht_multicast_sockaddr.sin6_family = AF_INET6;
  dht_multicast_sockaddr.sin6_port   = htons(4343);
  dht_multicast_sockaddr.sin6_addr   = dht_multicast_address;

  // Define address to bind against
	struct sockaddr_in6 bind_addr = {
		.sin6_family   = AF_INET6,
		.sin6_port     = htons(4343),
    .sin6_scope_id = ifindex,
    .sin6_addr     = IN6ADDR_ANY_INIT
	};

  // Set ReUSE
  int one = 1;
  if(setsockopt(dht_socket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
    dhcpd_error(1,0,"setsockopt: unable to set SO_REUSEADDR");
  }
  
  // Bind socket to address
  if(bind(dht_socket, (const struct sockaddr *)&bind_addr, sizeof(struct sockaddr_in6)) < 0) {
    dhcpd_error(1,0,"Can't bind to socket");
  }

  // Setup Multicast Resource
  struct ipv6_mreq mreq;
  mreq.ipv6mr_multiaddr = dht_multicast_address;
  mreq.ipv6mr_interface = ifindex;

  // Join Multicast group
  if(setsockopt(dht_socket, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq))){
    dhcpd_error(1,0,"setsockopt: unable to set IPV6_JOIN_GROUP");
  }
  

  if (setsockopt(dht_socket, IPPROTO_IPV6, IPV6_RECVPKTINFO, &one, sizeof(one))) {
    dhcpd_error(1,0,"setsockopt: unable to set IPV6_RECVPKTINFO");
  }
  // Setup Event Loop ---------------------------------------------------------

  // Select the default event loop
  struct ev_loop *loop = EV_DEFAULT;

  // Register callback on event loop
  ev_io_init(&dht_multicast_watcher, dht_cb, dht_socket, EV_READ);
	ev_io_start(loop, &dht_multicast_watcher);

  // Setup timer
  ev_timer_init(&dht_multicast_timer,&dht_multicast_timer_cb, 0.1 ,0.);
  ev_timer_start(loop,&dht_multicast_timer);

  puts("Joined Multicast");

  dht_boot_wait = 0;

  // Register callback to event loop
  //ev_io_init(&dht_watcher, dht_cb, dht_socket, EV_READ);
  //ev_io_start(loop, &dht_watcher);


  ev_run(loop,0);
  return -1;
}
