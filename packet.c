#include "packet.h"

bool send_offer(struct dhcp_msg *m, struct dhcp_lease *l) {
	uint8_t *buf = malloc(DHCP_MSG_LEN);
	if(!buf) {
		dhcpd_error(ENOMEM, 1, "Could not send DHCPOFFER");
		return false;
	}

	size_t send_len = 0;
	uint8_t *options = NULL;

	dhcp_msg_reply(buf, &options, &send_len, m, DHCPOFFER);

	ARRAY_COPY(DHCP_MSG_F_YIADDR(send_buffer), &lease.address, 4);

	dhcp_opt_insert_val(buf, DHCP_MSG_LEN, &send_len, options, DHCP_OPT_SERVERID, uint32_t, msg->sid->sin_addr);

	options = dhcp_opt_add_lease(options, &send_len, &lease);

	*options = DHCP_OPT_END;
	DHCP_OPT_CONT(options, send_len);

//	if (debug)
//		msg_debug(&((struct dhcp_msg){.data = send_buffer, .length = send_len }), 1);

//	int err = sendto(w->fd,
//		send_buffer, send_len,
//		MSG_DONTWAIT, (struct sockaddr *)&broadcast, sizeof broadcast);

//	if (err < 0)
//		dhcpd_error(errno, 1, "Could not send DHCPOFFER");

	return false;
}
