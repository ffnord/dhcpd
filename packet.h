#pragma once

#include <netinet/in.h>

#include "dhcp.h"

extern struct sockaddr_in server_id;
extern struct sockaddr_in broadcast;

bool send_offer(int socket, struct dhcp_msg *m, struct dhcp_lease *l);
bool send_ack(int socket, struct dhcp_msg *m, struct dhcp_lease *l);
bool send_nak(int socket, struct dhcp_msg *m);
