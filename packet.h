#pragma once

#include <netinet/in.h>

#include <ev.h>

#include "dhcp.h"

extern struct sockaddr_in server_id;
extern struct sockaddr_in broadcast;

bool send_offer(EV_P_ ev_io *w, struct dhcp_msg *m, struct dhcp_lease *l);
bool send_ack(EV_P_ ev_io *w, struct dhcp_msg *m, struct dhcp_lease *l, struct in_addr *r);
bool send_nak(EV_P_ ev_io *w, struct dhcp_msg *m);
