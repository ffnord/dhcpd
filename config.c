#include "config.h"

bool config_fill(struct config *cfg, struct argv *argv)
{
	cfg->argv = argv;

	for (size_t i = 0; i < argv->routers_cnt; ++i)
	{
		cfg->routers = realloc(cfg->routers, ++cfg->routers_cnt * sizeof(struct in_addr));
		if (inet_pton(AF_INET, argv->routers[i], &cfg->routers[cfg->routers_cnt-1]) != 1) {
			cfg->error = "Invalid router address";
			config_free(cfg);
			return false;
		}
	}

	for (size_t i = 0; i < argv->nameservers_cnt; ++i)
	{
		cfg->nameservers = realloc(cfg->nameservers, ++cfg->nameservers_cnt * sizeof(struct in_addr));
		if (inet_pton(AF_INET, argv->nameservers[i], &cfg->nameservers[cfg->nameservers_cnt-1]) != 1) {
			cfg->error = "Invalid nameserver address";
			config_free(cfg);
			return false;
		}
	}

	for (size_t i = 0; i < 2; ++i)
		if (argv->iprange[i])
			if (inet_pton(AF_INET, argv->iprange[i], &cfg->iprange[i]) != 1) {
				cfg->error = "Invalid IP range address";
				config_free(cfg);
				return false;
			}

	if (argv->leasetime)
		cfg->leasetime = atoi(argv->leasetime);

	if (argv->prefixlen)
		cfg->prefixlen = atoi(argv->prefixlen);

	return true;
}
