/* (c) 2013 Fritz Conrad Grimpen */

#define DHCP_DHCPD

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unistd.h>

#ifdef __linux__
#include <cap-ng.h>
#endif

#include <pwd.h>
#include <grp.h>

#include <ev.h>

#include "array.h"
#include "dhcp.h"
#include "argv.h"
#include "error.h"
#include "config.h"
#include "iplist.h"
#include "packet.h"
#include "pool.h"

#ifndef RECV_BUF_LEN
#define RECV_BUF_LEN 4096
#endif

#ifndef SEND_BUF_LEN
#define SEND_BUF_LEN 4096
#endif

#define VERSION "0.1"

uint8_t recv_buffer[RECV_BUF_LEN];
uint8_t send_buffer[SEND_BUF_LEN];

struct config cfg = CONFIG_EMPTY;

struct pool *pool;

bool debug = false;

static const char BROKEN_SOFTWARE_NOTIFICATION[] =
"#################################### ALERT ####################################\n"
"  BROKEN SOFTWARE NOTIFICATION - SOMETHING SENDS INVALID DHCP MESSAGES IN YOUR\n"
"                                    NETWORK\n";
static const char USAGE[] =
"%s [-h[elp]] [-v[ersion]] [-d[ebug]] [-user UID] [-group GID]\n"
"\t[-interface IF] [-db FILE]\n"
"\t[-new] [-allocate] [-iprange IP IP] [-router IP]... [-nameserver IP]...\n";


#define MAC_ADDRSTRLEN 18

/**
 * Handle DHCPDISCOVER request and reply to that
 */
static void discover_cb(EV_P_ ev_io *w, struct dhcp_msg *msg)
{
	(void)EV_A;

	/* XXX: Take address from pool
	 * 			If it is empty, ask for new address
	 *			If not, send offer
	 *				and publish lease
	 *				and rebalance pool if below threshhold?
	 */

	struct dhcp_lease lease = DHCP_LEASE_EMPTY;

	struct pool_entry *entry;

	entry = pool_get(pool);

	/* If our pool is empty we'll ask the network to offer an address to us.
	 * In the meantime, we won't respond to the client.
	 */
	if (entry == NULL) {
		// XXX: Ask for address
		return;
	}

	lease = (struct dhcp_lease){
		.routers = cfg.routers,
		.routers_cnt = cfg.routers_cnt,
		.nameservers = cfg.nameservers,
		.nameservers_cnt = cfg.nameservers_cnt,
		.leasetime = cfg.leasetime,
		.prefixlen = cfg.prefixlen,
		.address = entry->address
	};

	free(entry);

	send_offer(w->fd, msg, &lease);

	// XXX: Send (lease.address, msg->chaddr, lease.leasetime) to DHT
}

/**
 * Handle to DHCPREQUEST request and reply to that, and allocate lease if
 * enabled
 */
static void request_cb(EV_P_ ev_io *w, struct dhcp_msg *msg)
{
	(void)EV_A;

	/* XXX: Fetch lease with callback
	 *
	 * struct:
	 * 		- request_id
	 * 		- dhcp_msg
	 * 		- callback_ptr
	 * 		- timeout
	 *
	 * timeout -> NAK
	 *
	 * Callback: Check whether lease matches client
	 *           If yes, ACK
	 *           If no, NAK
	 */

	struct in_addr *requested_server;
	struct in_addr *requested_addr;
	uint8_t *options;
	struct dhcp_opt current_opt;

	requested_addr = NULL;
	requested_server = (struct in_addr *)DHCP_MSG_F_SIADDR(msg->data);
	options = DHCP_MSG_F_OPTIONS(msg->data);

	while (dhcp_opt_next(&options, &current_opt, msg->end))
		switch (current_opt.code)
		{
			case DHCP_OPT_REQIPADDR:
				requested_addr = (struct in_addr *)current_opt.data;
				break;
			case DHCP_OPT_SERVERID:
				requested_server = (struct in_addr *)current_opt.data;
				break;
		}

	if (requested_server->s_addr != msg->sid->sin_addr.s_addr)
		return;

	struct dhcp_lease lease = DHCP_LEASE_EMPTY;

	lease.address = *requested_addr;

	if (0) {
		// NACK
		send_nak(w->fd, msg);
	} else {
		// ACK
		send_ack(w->fd, msg, &lease);
	}
}

/**
 * Handle DHCPRELEASE request and release the lease if it was allocated
 */
static void release_cb(EV_P_ ev_io *w, struct dhcp_msg *msg)
{
	(void)EV_A;
	(void)w;
	(void)msg;

	/* XXX: Send message to manager */
}

/**
 * Handle DHCPDECLINE request and do nothing at the moment
 */
static void decline_cb(EV_P_ ev_io *w, struct dhcp_msg *msg)
{
	(void)EV_A;
	(void)w;
	(void)msg;

	/* XXX: Send message to manager */
}

/**
 * Handle DHCPINFORM request and reply with the correct information
 */
static void inform_cb(EV_P_ ev_io *w, struct dhcp_msg *msg)
{
	(void)EV_A;
	(void)w;
	(void)msg;

	/* XXX: http://tools.ietf.org/html/draft-ietf-dhc-dhcpinform-clarify-04
	 */
}

/**
 * Handle libev IO event to socket and call the correct message type
 * handler.
 */
static void req_cb(EV_P_ ev_io *w, int revents)
{
	(void)revents;

	/* Initialize address struct passed to recvfrom */
	struct sockaddr_in srcaddr = {
		.sin_addr = {INADDR_ANY}
	};
	socklen_t srcaddrlen = AF_INET;

	/* Receive data from socket */
	ssize_t recvd = recvfrom(
		w->fd,
		recv_buffer,
		RECV_BUF_LEN,
		MSG_DONTWAIT,
		(struct sockaddr * restrict)&srcaddr, &srcaddrlen);

	/* Detect errors */
	if (recvd < 0)
		return;
	/* Detect too small messages */
	if (recvd < DHCP_MSG_HDRLEN)
		return;
	/* Check magic value */
	uint8_t *magic = DHCP_MSG_F_MAGIC(recv_buffer);
	if (!DHCP_MSG_MAGIC_CHECK(magic))
		return;

	/* Extract message type from options */
	uint8_t *options = DHCP_MSG_F_OPTIONS(recv_buffer);
	struct dhcp_opt current_option;

	enum dhcp_msg_type msg_type = 0;

	while (dhcp_opt_next(&options, &current_option, (uint8_t*)(recv_buffer + recvd)))
		if (current_option.code == 53)
			msg_type = (enum dhcp_msg_type)current_option.data[0];

	struct dhcp_msg msg = {
		.data = recv_buffer,
		.end = recv_buffer + recvd,
		.length = recvd,
		.type = msg_type,
		.ciaddr.s_addr = ntohl(*DHCP_MSG_F_CIADDR(recv_buffer)),
		.yiaddr.s_addr = ntohl(*DHCP_MSG_F_YIADDR(recv_buffer)),
		.siaddr.s_addr = ntohl(*DHCP_MSG_F_SIADDR(recv_buffer)),
		.giaddr.s_addr = ntohl(*DHCP_MSG_F_GIADDR(recv_buffer)),
		.source = (struct sockaddr *)&srcaddr,
		.sid = (struct sockaddr_in *)&server_id
	};

	memcpy(&msg.chaddr, DHCP_MSG_F_CHADDR(recv_buffer), sizeof(msg.chaddr));

	switch (msg_type)
	{
		case DHCPDISCOVER:
			discover_cb(EV_A_ w, &msg);
			break;

		case DHCPREQUEST:
			request_cb(EV_A_ w, &msg);
			break;

		case DHCPRELEASE:
			release_cb(EV_A_ w, &msg);
			break;

		case DHCPDECLINE:
			decline_cb(EV_A_ w, &msg);
			break;

		case DHCPINFORM:
			inform_cb(EV_A_ w, &msg);
			break;

		default:
			fprintf(stderr, BROKEN_SOFTWARE_NOTIFICATION);
			break;
	}
}

int main(int argc, char **argv)
{
	struct argv argv_cfg = ARGV_EMPTY;

	if (!argv_parse(argc, argv, &argv_cfg))
	{
		if (argv_cfg.argerror == -1)
			dhcpd_error(1, 0, "Unexpected argument list end");
		else
			dhcpd_error(1, 0, "Unexpected argument %s", argv_cfg.argv[argv_cfg.argerror]);
		exit(1);
	}

	if (argv_cfg.version)
	{
		printf("dhcpd " VERSION " - (c) 2013 Fritz Conrad Grimpen\n");
		exit(0);
	}

	if (argv_cfg.help || argv_cfg.interface == NULL)
	{
		printf(USAGE, argv_cfg.arg0);
		exit(0);
	}

	if (!config_fill(&cfg, &argv_cfg))
		dhcpd_error(1, 0, cfg.error);

	if (argv_cfg.user != NULL)
	{
#ifdef __linux__
		uid_t uid = 0;
		gid_t gid = 0;

		struct passwd *pwent;

		pwent = getpwnam(argv_cfg.user);
		if (pwent == NULL)
		{
			pwent = getpwuid(atoi(argv_cfg.user));
			if (pwent == NULL)
				dhcpd_error(1, errno, "Could not find user identified by \"%s\"", argv_cfg.user);
		}

		uid = pwent->pw_uid;
		gid = pwent->pw_gid;

		if (argv_cfg.group != NULL)
		{
			struct group *grent;

			grent = getgrnam(argv_cfg.group);
			if (grent == NULL)
			{
				grent = getgrgid(atoi(argv_cfg.group));
				if (grent == NULL)
					dhcpd_error(1, errno, "Could not find user identified by \"%s\"", argv_cfg.group);
			}

			gid = grent->gr_gid;
		}

		capng_clear(CAPNG_SELECT_BOTH);
		capng_updatev(CAPNG_ADD, CAPNG_EFFECTIVE|CAPNG_PERMITTED,
			CAP_NET_BIND_SERVICE, CAP_NET_BROADCAST, CAP_NET_RAW, -1);
		if (capng_change_id(uid, gid, CAPNG_DROP_SUPP_GRP | CAPNG_CLEAR_BOUNDING))
			dhcpd_error(1, 0, "Could not change UID and drop capabilities");
#else
		dhcpd_error(1, 0, "Can only drop privileges on Linux");
#endif
	}

	/* Prepare dummy IP Pool */
	pool = pool_create(8);

	struct pool_entry entry;
	uint32_t ip;

	IPRANGE_FOREACH(cfg.iprange[0], cfg.iprange[1], ip) {
		entry.address.s_addr = htonl(ip);

		pool_add(pool, &entry);
	}

	/* Set client IP address */
	broadcast.sin_port = htons(68);
	/* Clear IO buffers */
	memset(send_buffer, 0, ARRAY_LEN(send_buffer));
	memset(recv_buffer, 0, ARRAY_LEN(recv_buffer));

	if (if_nametoindex(argv_cfg.interface) == 0)
		dhcpd_error(1, errno, argv_cfg.interface);

	if (argv_cfg.debug)
		debug = true;

	int sock;
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		dhcpd_error(1, errno, "Could not create socket");

	struct sockaddr_in bind_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(67),
		.sin_addr = {INADDR_ANY}
	};

	struct ifaddrs *ifaddrs, *ifa;

	if (getifaddrs(&ifaddrs) == -1)
		dhcpd_error(1, errno, "Could not get interface information");

	for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (ifa->ifa_addr == NULL)
			continue;

		if (ifa->ifa_addr->sa_family == AF_INET &&
				strcmp(ifa->ifa_name, argv_cfg.interface) == 0)
		{
			struct sockaddr_in *ifa_addr_in = (struct sockaddr_in *)ifa->ifa_addr;
			server_id = *ifa_addr_in;
			break;
		}
	}

	freeifaddrs(ifaddrs);

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) != 0)
		dhcpd_error(1, errno, "Could not set socket to reuse address");

	if (bind(sock, (const struct sockaddr *)&bind_addr, sizeof(struct sockaddr_in)) < 0)
		dhcpd_error(1, errno, "Could not bind to 0.0.0.0:67");

	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (int[]){1}, sizeof(int)) != 0)
		dhcpd_error(1, errno, "Could not set broadcast socket option");
#ifdef __linux__
	if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, argv_cfg.interface, strlen(argv_cfg.interface)) != 0)
		dhcpd_error(1, errno, "Could not bind to device %s", argv_cfg.interface);
#endif

	struct ev_loop *loop = EV_DEFAULT;

	ev_io read_watch;

	ev_io_init(&read_watch, req_cb, sock, EV_READ);
	ev_io_start(loop, &read_watch);

	ev_run(loop, 0);

	config_free(&cfg);
	argv_free(&argv_cfg);

	exit(0);
}

