#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ev.h>

#include <netinet/in.h>

#include "array.h"

/* A DHCP message consists of a fixed-length header and a variable-length
 * option part:
 *
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |     op (1)    |   htype (1)   |   hlen (1)    |   hops (1)    |
 * +---------------+---------------+---------------+---------------+
 * |                            xid (4)                            |
 * +-------------------------------+-------------------------------+
 * |           secs (2)            |           flags (2)           |
 * +-------------------------------+-------------------------------+
 * |                          ciaddr  (4)                          |
 * +---------------------------------------------------------------+
 * |                          yiaddr  (4)                          |
 * +---------------------------------------------------------------+
 * |                          siaddr  (4)                          |
 * +---------------------------------------------------------------+
 * |                          giaddr  (4)                          |
 * +---------------------------------------------------------------+
 * |                                                               |
 * |                          chaddr  (16)                         |
 * |                                                               |
 * |                                                               |
 * +---------------------------------------------------------------+
 * |                                                               |
 * |                          sname   (64)                         |
 * +---------------------------------------------------------------+
 * |                                                               |
 * |                          file    (128)                        |
 * +---------------------------------------------------------------+
 * |                                                               |
 * |                          options (variable)                   |
 * +---------------------------------------------------------------+
 */

#define DHCP_MSG_F_OP(m)      ((uint8_t*)((m)+0))
#define DHCP_MSG_F_HTYPE(m)   ((uint8_t*)((m)+1))
#define DHCP_MSG_F_HLEN(m)    ((uint8_t*)((m)+2))
#define DHCP_MSG_F_HOPS(m)    ((uint8_t*)((m)+3))
#define DHCP_MSG_F_XID(m)     ((uint32_t*)((m)+4))
#define DHCP_MSG_F_SECS(m)    ((uint16_t*)((m)+8))
#define DHCP_MSG_F_FLAGS(m)   ((uint16_t*)((m)+10))
#define DHCP_MSG_F_CIADDR(m)  ((uint32_t*)((m)+12))
#define DHCP_MSG_F_YIADDR(m)  ((uint32_t*)((m)+16))
#define DHCP_MSG_F_SIADDR(m)  ((uint32_t*)((m)+20))
#define DHCP_MSG_F_GIADDR(m)  ((uint32_t*)((m)+24))
#define DHCP_MSG_F_CHADDR(m)  ((char*)((m)+28))
#define DHCP_MSG_F_MAGIC(m)   ((uint8_t*)((m)+236))
#define DHCP_MSG_F_OPTIONS(m) ((uint8_t*)((m)+240))

#define DHCP_MSG_LEN    (576)
#define DHCP_MSG_HDRLEN (240)
#define DHCP_MSG_MAGIC  ((uint8_t[]){ 99, 130, 83, 99 })

#define DHCP_MSG_MAGIC_CHECK(m) (m[0] == 99 && m[1] == 130 && m[2] == 83 && m[3] == 99)

#define DHCP_OPT_F_CODE(o) ((uint8_t*)(o))
#define DHCP_OPT_F_LEN(o)  ((uint8_t*)((o)+1))
#define DHCP_OPT_F_DATA(o) ((char*)((o)+2))

#define DHCP_OPT_LEN(o) (((uint8_t*)(o))[0] != 255 && ((uint8_t*)(o))[0] != 0 ? ((uint8_t*)(o))[1] + 2 : 1)
#define DHCP_OPT_NEXT(o) (((uint8_t*)(o))+DHCP_OPT_LEN(o))
#define DHCP_OPT_CONT(o, l) {\
		(l) += DHCP_OPT_LEN(o);\
		(o) = DHCP_OPT_NEXT(o);\
	}
#define DHCP_OPT(m) {\
		.code = *DHCP_OPT_F_CODE(m),\
		.len = *DHCP_OPT_F_LEN(m),\
		.data = ((*DHCP_OPT_F_LEN(m)) > 0 ? DHCP_OPT_F_DATA(m) : NULL)\
	}

enum dhcp_opt_type
{
	DHCP_OPT_STUB = 0,
	DHCP_OPT_NETMASK = 1,
	DHCP_OPT_ROUTER = 3,
	DHCP_OPT_DNS = 6,
	DHCP_OPT_REQIPADDR = 50,
	DHCP_OPT_LEASETIME = 51,
	DHCP_OPT_MSGTYPE = 53,
	DHCP_OPT_SERVERID = 54,
	DHCP_OPT_END = 255
};

enum dhcp_msg_type
{
	DHCPDISCOVER = 1,
	DHCPOFFER = 2,
	DHCPREQUEST = 3,
	DHCPDECLINE = 4,
	DHCPACK = 5,
	DHCPNAK = 6,
	DHCPRELEASE = 7,
	DHCPINFORM = 8
};

struct dhcp_opt
{
	uint8_t code;
	uint8_t len;
	char *data;
};

struct dhcp_msg
{
	uint8_t *data;
	uint8_t *end;
	size_t length;

	enum dhcp_msg_type type;
	struct in_addr ciaddr;
	struct in_addr yiaddr;
	struct in_addr siaddr;
	struct in_addr giaddr;
	uint8_t chaddr[16];

	struct sockaddr *source;
	struct sockaddr_in *sid;
};

struct dhcp_lease
{
	struct in_addr address;

	struct in_addr *routers;
	size_t routers_cnt;

	struct in_addr *nameservers;
	size_t nameservers_cnt;

	ev_tstamp leasetime;
	uint8_t prefixlen;
};

#define DHCP_LEASE_EMPTY {\
		.address = {INADDR_ANY},\
		.routers = NULL,\
		.routers_cnt = 0,\
		.nameservers = NULL,\
		.nameservers_cnt = 0,\
		.leasetime = 0,\
		.prefixlen = 0\
	}

/**
 * Prepare new DHCP message from a specified DHCP message
 *
 * @param[out] reply Buffer which holds the new DHCP message
 * @param[in] original Buffer which holds the old DHCP message
 */
static inline void dhcp_msg_prepare(uint8_t *reply, uint8_t *original)
{
	*DHCP_MSG_F_XID(reply) = *DHCP_MSG_F_XID(original);
	*DHCP_MSG_F_HTYPE(reply) = *DHCP_MSG_F_HTYPE(original);
	*DHCP_MSG_F_HLEN(reply) = *DHCP_MSG_F_HLEN(original);
	*DHCP_MSG_F_OP(reply) = (*DHCP_MSG_F_OP(reply) == 2 ? 1 : 2);
	ARRAY_COPY(DHCP_MSG_F_MAGIC(reply), DHCP_MSG_MAGIC, 4);
	ARRAY_COPY(DHCP_MSG_F_CHADDR(reply), DHCP_MSG_F_CHADDR(original), 16);
}

/**
 * Generate full response from a specified DHCP message
 *
 * @param[out] reply Buffer which holds the new DHCP message
 * @param[out] options Pointer which will point to next message option
 * @param[out] send_len Size of the message after filling information
 * @param[in] msg DHCP message
 * @param[in] type Type of the new DHCP message
 */
static inline void dhcp_msg_reply(uint8_t *reply, uint8_t **options,
	size_t *send_len, struct dhcp_msg *msg, enum dhcp_msg_type type)
{
	*send_len = DHCP_MSG_HDRLEN;
	memset(reply, 0, DHCP_MSG_LEN);
	dhcp_msg_prepare(reply, msg->data);

	ARRAY_COPY(DHCP_MSG_F_SIADDR(reply), &msg->sid->sin_addr, 4);

	*options = DHCP_MSG_F_OPTIONS(reply);

	(*options)[0] = DHCP_OPT_MSGTYPE;
	(*options)[1] = 1;
	(*options)[2] = type;
	DHCP_OPT_CONT(*options, *send_len);
}

#define dhcp_opt_insert_val(buf, buf_len, send_len, opt, type, vtype, value) \
	do { vtype v = value; dhcp_opt_insert(buf, buf_len, send_len, opt, type, sizeof(vtype), (uint8_t *)(&(v))); } while(0)

static inline bool dhcp_opt_insert(uint8_t *buf, size_t buf_len, size_t *send_len, uint8_t **opt, enum dhcp_msg_type type, size_t data_len, uint8_t *data)
{
	if(!buf || !*buf || !opt | !*opt) {
		return false;
	}

	if( (size_t)(*opt - buf) != *send_len ) {
		return false;
	}

	if( (size_t)(*opt - buf) > buf_len ) {
		return false;
	}

	size_t raw_data_len = data_len + 2;

	switch(type) {
		case 0x00:	//Padding
		case 0xFF:	//End of packet
			raw_data_len = 1;
			if(0 != data_len) {
				return false;
			}

			(*opt)[0] = type;
			(*opt)++;
			*send_len++;
			return true;

		case 0x01:	//Subnet Mask
		case 0x02:	//Time Offset
		case 0x10:	//Swap Server IP
		case 0x18:	//PMTUD Timeout
		case 0x1c:	//Broadcast IP
		case 0x20:	//Router Solicitation Address
		case 0x23:	//ARP Cache Timeout
		case 0x26:	//TCP Keepalive Interval
		case 0x32:	//Requested IP Address
		case 0x33:	//IP Address Lease Time
		case 0x3A:	//Renewal (T1) Time
		case 0x3B:	//Rebind (T2) Time
			if(4 != data_len) {
				return false;
			}
			break;

		case 0x03:	//Routers
		case 0x04:	//Timeservers
		case 0x05:	//Nameservers
		case 0x06:	//DNS Servers
		case 0x07:	//Log Servers
		case 0x08:	//Cookie Servers
		case 0x09:	//LPR Servers
		case 0x0A:	//Impress Servers
		case 0x0B:	//Resource Location Servers
		case 0x29:	//Network Information Servers
		case 0x2A:	//Network Time Protocol Servers
		case 0x2C:	//NetBIOS over TCP/IP Name Servers
		case 0x2D:	//NetBIOS over TCP/IP Datagramm Distribution Server
		case 0x30:	//X Windows System Font Server
		case 0x31:	//X Window System Display Manager
		case 0x41:	//Network Information Service+ Servers
		case 0x45:	//SMTP Servers
		case 0x46:	//POP3 Servers
		case 0x47:	//NNTP Servers
		case 0x48:	//HTTP Servers
		case 0x49:	//Finger Servers
		case 0x4A:	//IRC Servers
		case 0x4B:	//StreetTalk Servers
		case 0x4C:	//StreetTalk Directory Assistance Servers
			if((0 == data_len) || (0 != data_len % 4)) {
				return false;
			}
			break;

		case 0x0C:	//Hostname Option
		case 0x0E:	//Merit Dump Filename
		case 0x0F:	//Domain Name Option
		case 0x11:	//Root Path
		case 0x12:	//Extensions Path
		case 0x28:	//Network Information Service Domain
		case 0x2B:	//Vendor Specific Information
		case 0x2F:	//NetBIOS over TCP/IP Scope
		case 0x37:	//Parameter Request List
		case 0x38:	//Message
		case 0x3C:	//Class Identifier
		case 0x3D:	//Client Identifier
		case 0x40:	//Network Information Service+ Domain
		case 0x42:	//TFTP Servername
		case 0x43:	//Boot Filename
		case 0x4F:	//LDAP Servers
		case 0x64:	//PCode
		case 0x65:	//TCode
		case 0x78:	//SIP Server
			if(0 == data_len) {
				return false;
			}
			break;

		case 0x0D:	//Boot File Size (No. of 512 Octet Blocks)
		case 0x16:	//Maximum Datagramm Reassembly Size
		case 0x1A:	//Interface MTU
		case 0x39:	//Maximum DHCP Message Size
			if(2 != data_len) {
				return false;
			}
			break;

		case 0x13:	//IP Forwarding
		case 0x14:	//Non-Local Source-Routing
		case 0x17:	//IP Default TTL
		case 0x1B:	//All Subnets local
		case 0x1D:	//Subnet Mask Discovery
		case 0x1E:	//Subnet Mask Supplier
		case 0x1F:	//Router Discovery
		case 0x22:	//Trailer Encapsulation
		case 0x24:	//Ethernet Encapsulation
		case 0x25:	//TCP Default TTL
		case 0x27:	//TCP Keepalive Garbage
		case 0x2E:	//NetBIOS over TCP/IP Node Type
		case 0x34:	//Option Override
		case 0x35:	//DHCP Message Type
		case 0x36:	//DHCP Server ID
			if(1 != data_len) {
				return false;
			}
			break;

		case 0x15:	//NLSR Policy
		case 0x21:	//Static Routes
			if((0 == data_len) || (0 != data_len % 8)) {
				return false;
			}
			break;

		case 0x19:	//PMTUD Plateau Table
			if((0 == data_len) || (0 != data_len % 2)) {
				return false;
			}
			break;

		case 0x44:	//Mobile Home Agent
			if(0 != data_len % 4) {
				return false;
			}
			break;

		case 0x79:	//Classless Static Routes
			//Enforces RFC 3396
			if( 5 > data_len) {
				return false;
			}
			break;

		case 0x50:	//Rapid Commit
			if(0 != data_len) {
				return false;
			}
			break;

		default:
			//No special restrictions
	}

	if(data_len > 255) {
		return false;
	}

	if(*send_len + raw_data_len > buf_len) {
		return false;
	}

	(*opt)[0] = type;
	(*opt)[1] = data_len;

	if(data_len) {
		memcpy(*opt + 2, data, data_len);
	}

	DHCP_OPT_CONT(*opt, *send_len);

	return true;
}

static inline bool dhcp_opt_next(uint8_t **cur, struct dhcp_opt *opt, uint8_t *end)
{
	if (*DHCP_OPT_F_CODE(*cur) == DHCP_OPT_END)
		return false;

	if (opt != NULL)
		*opt = (struct dhcp_opt)DHCP_OPT(*cur);

	/* Overflow detection */
	if ((*cur) + DHCP_OPT_LEN(*cur) >= end)
		return false;

	*cur = DHCP_OPT_NEXT(*cur);

	return true;
}

extern uint8_t *dhcp_opt_add_lease(uint8_t *options,
	size_t *send_len,
	struct dhcp_lease *lease);
