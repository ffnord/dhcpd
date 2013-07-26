#include "dht.h"

struct dht dist_hash_table;

ev_io dht_watcher;
ev_io dht_multicast_watcher;
ev_timer dht_multicast_timer;
int dht_socket;
char buffer[100000];

enum dht_state dht_current_state = DHT_STATE_BOOT;
int  dht_boot_wait;

char* interface;
int   port;

struct in6_addr dht_multicast_address;
struct sockaddr_in6 dht_multicast_sockaddr;
struct in6_addr dht_unicast_address;
