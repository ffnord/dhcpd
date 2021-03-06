#include "packet.h"

#include <stdlib.h>
#include <errno.h>

#include "error.h"

struct sockaddr_in server_id;
struct sockaddr_in broadcast = {
	.sin_family = AF_INET,
	.sin_addr = {INADDR_BROADCAST},
};

bool send_offer(int socket, struct dhcp_msg *m, struct dhcp_lease *l) {
	uint8_t *buf = malloc(DHCP_MSG_LEN);
	if(!buf) {
		dhcpd_error(ENOMEM, 1, "Could not send DHCPOFFER");
		return false;
	}

	size_t send_len = 0;
	uint8_t *options = NULL;

	dhcp_msg_reply(buf, &options, &send_len, m, DHCPOFFER);

	ARRAY_COPY(DHCP_MSG_F_YIADDR(buf), &l->address, 4);

	dhcp_opt_insert_val(buf, DHCP_MSG_LEN, &send_len, &options, DHCP_OPT_SERVERID, uint32_t, m->sid->sin_addr.s_addr);

	options = dhcp_opt_add_lease(options, &send_len, l);

	*options = DHCP_OPT_END;
	DHCP_OPT_CONT(options, send_len);

//	if (debug)
//		msg_debug(&((struct dhcp_msg){.data = send_buffer, .length = send_len }), 1);

	int err = sendto(socket, buf, send_len, MSG_DONTWAIT,
			(struct sockaddr *)&broadcast, sizeof broadcast);

	if (err < 0) {
		dhcpd_error(errno, 1, "Could not send DHCPOFFER");
		return false;
	}

	free(buf);

	return true;
}

bool send_ack(int socket, struct dhcp_msg *m, struct dhcp_lease *l) {
	uint8_t *buf = malloc(DHCP_MSG_LEN);
	if(!buf) {
		dhcpd_error(ENOMEM, 1, "Could not send DHCPOFFER");
		return false;
	}

	size_t send_len = 0;
	uint8_t *options = NULL;

	// ACK
	dhcp_msg_reply(buf, &options, &send_len, m, DHCPACK);

	ARRAY_COPY(DHCP_MSG_F_YIADDR(buf), &l->address, 4);

	dhcp_opt_insert_val(buf, DHCP_MSG_LEN, &send_len, &options, DHCP_OPT_SERVERID, uint32_t, m->sid->sin_addr.s_addr);

	options = dhcp_opt_add_lease(options, &send_len, l);

	*options = DHCP_OPT_END;
	DHCP_OPT_CONT(options, send_len);

//	if (debug)
//		msg_debug(&((struct dhcp_msg){.data = buf, .length = send_len }), 1);
	int err = sendto(socket, buf, send_len, MSG_DONTWAIT,
			(struct sockaddr *)&broadcast, sizeof broadcast);

	if (err < 0) {
		dhcpd_error(0, 1, "Could not send DHCPACK");
		return false;
	}

	free(buf);

	return true;
}

bool send_nak(int socket, struct dhcp_msg *m) {
	uint8_t *buf = malloc(DHCP_MSG_LEN);
	if(!buf) {
		dhcpd_error(ENOMEM, 1, "Could not send DHCPOFFER");
		return false;
	}

	size_t send_len = 0;
	uint8_t *options = NULL;

	dhcp_msg_reply(buf, &options, &send_len, m, DHCPNAK);

	options[0] = DHCP_OPT_END;
	DHCP_OPT_CONT(options, send_len);

//	if (debug)
//		msg_debug(&((struct dhcp_msg){.data = buf, .length = send_len }), 1);
	int err = sendto(socket, buf, send_len, MSG_DONTWAIT,
			(struct sockaddr *)&broadcast, sizeof broadcast);

	if (err < 0) {
		dhcpd_error(0, 1, "Could not send DHCPNAK");
		return false;
	}

	free(buf);

	return true;
}
