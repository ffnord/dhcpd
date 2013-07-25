#pragma once

#include "dhcp.h"

bool send_offer(struct dhcp_msg *m, struct dhcp_lease *l);
bool send_ack(struct dhcp_msg *m, struct dhcp_lease *l, struct in_addr *r);
bool send_nak(struct dhcp_msg *m);
