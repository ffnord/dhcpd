#include "dhcp.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

static uint32_t netmask_from_prefixlen(uint8_t prefixlen)
{
	return htonl(0xFFFFFFFFU - (1 << (32 - prefixlen)) + 1);
}

uint8_t *dhcp_opt_add_lease(uint8_t *options, size_t *_send_len, struct dhcp_lease *lease)
{
	size_t send_len = 0;

	if (lease->prefixlen > 0)
	{
		options[0] = DHCP_OPT_NETMASK;
		options[1] = 4;
		ARRAY_COPY((options + 2), (uint8_t*)((uint32_t[]){netmask_from_prefixlen(lease->prefixlen)}), 4);
		DHCP_OPT_CONT(options, send_len);
	}

	if (lease->routers_cnt > 0)
	{
		options[0] = DHCP_OPT_ROUTER;
		options[1] = lease->routers_cnt * 4;
		for (size_t i = 0; i < lease->routers_cnt; ++i)
			*(struct in_addr *)(options + 2 + (i * 4)) = lease->routers[i];
		DHCP_OPT_CONT(options, send_len);
	}

	if (lease->leasetime > 0)
	{
		options[0] = DHCP_OPT_LEASETIME;
		options[1] = 4;
		*(uint32_t*)(options + 2) = htonl(lease->leasetime);
		DHCP_OPT_CONT(options, send_len);
	}

	if (lease->nameservers_cnt > 0)
	{
		options[0] = DHCP_OPT_DNS;
		options[1] = lease->nameservers_cnt * 4;
		for (size_t i = 0; i < lease->nameservers_cnt; ++i)
			*(struct in_addr *)(options + 2 + (i * 4)) = lease->nameservers[i];
		DHCP_OPT_CONT(options, send_len);
	}

	if (_send_len != NULL)
		*_send_len += send_len;

	return options;
}
