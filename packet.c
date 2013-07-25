#include "packet.h"

#include <stdlib.h>
#include <errno.h>

#include "error.h"

bool send_offer(struct dhcp_msg *m, struct dhcp_lease *l) {
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

//	int err = sendto(w->fd,
//		send_buffer, send_len,
//		MSG_DONTWAIT, (struct sockaddr *)&broadcast, sizeof broadcast);

//	if (err < 0) {
//		dhcpd_error(errno, 1, "Could not send DHCPOFFER");
		return false;
//	}

	free(buf);

	return true;
}
