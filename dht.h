#define _GNU_SOURCE

#include <ev.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "error.h"

#ifndef DHCPD_DHT_H
#define DHCPD_DHT_H

struct dht_data {
  char *hash;
  void *data;

  struct dht_data *next;
};

struct dht_neighbor {
  struct in6_addr address;
  char            hash[4];

  struct dht_neighbor *next;
};

struct dht {
  struct dht_neighbor *neighbors;
  struct dht_data *data;   // zone data
  unsigned int hash_width; // width of the space in byte
  char dht_node_hash[4];
  char* zonemask;
};

enum dht_msg_type {
  DHT_NEIGHBOR_LOOKUP = 0,
  DHT_NEIGHBOR_ANNOUNCE = 1,
  DHT_JOIN = 2,
  DHT_PING = 3
};

enum dht_state {
  DHT_STATE_BOOT = 0,
  DHT_STATE_INIT = 1,
  DHT_STATE_RUNNING = 2
};

struct __attribute__((packed)) dht_msg {
  uint8_t  type;
  uint8_t  hash[4];
  uint32_t len;
  uint8_t  data[];
};

extern struct dht dist_hash_table;

extern ev_io dht_watcher;
extern ev_io dht_multicast_watcher;
extern ev_timer dht_multicast_timer;

extern char buffer[100000];

extern enum  dht_state dht_current_state;
extern int   dht_boot_wait;
extern char* interface;
extern int   port;

extern int    dht_socket;
extern struct in6_addr dht_multicast_address;
extern struct sockaddr_in6 dht_multicast_sockaddr;
extern struct in6_addr dht_unicast_address;

int dht_init(unsigned int hash_width,unsigned int port);
int dht_get(char *hash,void **data);
int dht_set(char *hash,void *data);

// Multicast handler
void dht_cb(EV_P_ ev_io *w, int revents);
void dht_multicast_timer_cb(struct ev_loop *loop, ev_timer *w, int revents);



#endif
