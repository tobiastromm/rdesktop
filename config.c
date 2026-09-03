/* -*- c-basic-offset: 8 -*- */
#include "config.h"
#include <errno.h>

extern RDP_VERSION g_rdp_version;
extern char g_hostname[16];
extern int g_tcp_port_rdp;
extern char *g_username;

static RD_BOOL
rdesktop_config_parse_port(const char *value, uint16 *port)
{
	char *end;
	long parsed;

	errno = 0;
	parsed = strtol(value, &end, 10);
	if (errno != 0 || end == value || *end != '\0' || parsed < 1 || parsed > 65535)
		return False;
	*port = (uint16) parsed;
	return True;
}

void
rdesktop_config_init_defaults(RDESKTOP_CONFIG *config)
{
	memset(config, 0, sizeof(*config));
	config->rdp_version = RDP_V5;
	config->port = TCP_PORT_RDP;
}

RD_BOOL
rdesktop_config_validate(const RDESKTOP_CONFIG *config)
{
	return config != NULL &&
		(config->rdp_version == RDP_V4 || config->rdp_version == RDP_V5) &&
		config->port != 0;
}

RD_BOOL
rdesktop_config_parse_server_endpoint(RDESKTOP_CONFIG *config)
{
	char *separator;
#ifdef IPv6
	char *cursor;
	int colons = 0;

	for (cursor = config->server; *cursor != '\0'; cursor++)
		if (*cursor == ':')
			colons++;
	if (colons >= 2)
	{
		separator = strchr(config->server, ']');
		if (config->server[0] != '[' || separator == NULL)
			return True;
		if (separator[1] == ':' && separator[2] != '\0' &&
		    !rdesktop_config_parse_port(separator + 2, &config->port))
			return False;
		if (separator[1] != '\0' && separator[1] != ':')
			return False;
		*separator = '\0';
		memmove(config->server, config->server + 1, strlen(config->server));
		return config->server[0] != '\0';
	}
#endif
	separator = strchr(config->server, ':');
	if (separator != NULL)
	{
		if (!rdesktop_config_parse_port(separator + 1, &config->port))
			return False;
		*separator = '\0';
	}
	return config->server[0] != '\0';
}

void
rdesktop_config_apply_to_legacy(const RDESKTOP_CONFIG *config, char *domain,
				size_t domain_size)
{
	g_rdp_version = config->rdp_version;
	STRNCPY(g_hostname, config->hostname, sizeof(g_hostname));
	g_tcp_port_rdp = config->port;
	xfree(g_username);
	g_username = NULL;
	if (config->username_set)
	{
		g_username = (char *) xmalloc(strlen(config->username) + 1);
		strcpy(g_username, config->username);
	}
	STRNCPY(domain, config->domain, domain_size);
}
