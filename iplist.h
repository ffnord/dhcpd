#pragma once

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct iplist
{
	struct in_addr *ips;
	size_t cnt;
};

#define IPLIST_EMPTY {NULL, 0}
#define IPLIST_RESIZE(l, n) (l) = realloc((l).ips, (l).cnt = (n))
#define IPLIST_INC(l) IPLIST_RESIZE(l, ((l).cnt + 1))
#define IPLIST_EXPAND(l) ((l)->ips), ((l)->cnt)
#define IPLIST_DUMP_LEN(l) ((l)->cnt * INET_ADDRSTRLEN + (l)->cnt)

/**
 * Convert ip list to text presentation
 */
static inline char *iplist_dump(struct in_addr *in, size_t in_cnt, char *out, size_t out_len)
{
	size_t n_len = INET_ADDRSTRLEN * in_cnt + in_cnt;
	if (out == NULL)
		out = malloc(n_len);
	memset(out, 0, n_len);
	if (n_len > out_len)
		return NULL;

	for (size_t i = 0; i < in_cnt; ++i)
	{
		inet_ntop(AF_INET, &in[i], out, INET_ADDRSTRLEN);

		while (*out)
			++out;

		*out = ',';
		++out;
	}

	*(out-1) = 0;

	return out;
}

/**
 * Generate comparision code, which checks whether a is part of the IP range
 * [l, u].
 *
 * IP ranges are full-inclusive ranges, which means, that l and u, the lower
 * and upper boundaries, are part of the range.
 */
#define IPRANGE_IN(l, u, a) (ntohl((l).s_addr) <= ntohl((a).s_addr) &&\
	ntohl((u).s_addr) >= ntohl((a).s_addr))
#define IPRANGE_IN2(r, a) IPRANGE_IN(r[0], r[1], a)

#define IPRANGE_FOREACH(l, u, v) for ((v) = ntohl(l.s_addr); (v) <= ntohl(u.s_addr); ++(v))
