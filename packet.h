#pragma once

#include "dhcp.h"

bool send_offer(struct dhcp_msg *m, struct dhcp_lease *l);
