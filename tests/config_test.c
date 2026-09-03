#define main rdesktop_program_main
#include "../config.h"
#undef main

RDP_VERSION g_rdp_version = RDP_V4;
char g_hostname[16] = "legacy-host";
int g_tcp_port_rdp = TCP_PORT_RDP;
char *g_username = NULL;

void *
xmalloc(int size)
{
	return malloc((size_t) size);
}

void
xfree(void *memory)
{
	free(memory);
}

int
main(void)
{
	RDESKTOP_CONFIG value;
	char domain[256];
	rdesktop_config_init_defaults(&value);
	if (value.rdp_version != RDP_V5 || value.hostname[0] != '\0' ||
	    !rdesktop_config_validate(&value))
		return 1;
	value.rdp_version = RDP_V4;
	if (!rdesktop_config_validate(&value))
		return 2;
	value.rdp_version = (RDP_VERSION) 0;
	if (rdesktop_config_validate(&value))
		return 3;
	value.rdp_version = RDP_V5;
	STRNCPY(value.hostname, "client-host", sizeof(value.hostname));
	g_rdp_version = RDP_V4;
	rdesktop_config_apply_to_legacy(&value, domain, sizeof(domain));
	if (g_rdp_version != RDP_V5)
		return 4;
	if (strcmp(g_hostname, "client-host") != 0 || g_tcp_port_rdp != TCP_PORT_RDP)
		return 5;
	if (g_username != NULL || domain[0] != '\0')
		return 11;

	STRNCPY(value.username, "alice", sizeof(value.username));
	value.username_set = True;
	STRNCPY(value.domain, "EXAMPLE", sizeof(value.domain));
	rdesktop_config_apply_to_legacy(&value, domain, sizeof(domain));
	if (g_username == NULL || strcmp(g_username, "alice") != 0 ||
	    strcmp(domain, "EXAMPLE") != 0)
		return 12;
	value.domain[0] = '\0';
	rdesktop_config_apply_to_legacy(&value, domain, sizeof(domain));
	if (strcmp(g_username, "alice") != 0 || domain[0] != '\0')
		return 13;
	value.username[0] = '\0';
	value.username_set = True;
	rdesktop_config_apply_to_legacy(&value, domain, sizeof(domain));
	if (g_username == NULL || g_username[0] != '\0')
		return 14;

	rdesktop_config_init_defaults(&value);
	STRNCPY(value.server, "server.example.com", sizeof(value.server));
	if (!rdesktop_config_parse_server_endpoint(&value) ||
	    strcmp(value.server, "server.example.com") != 0 || value.port != TCP_PORT_RDP)
		return 6;

	rdesktop_config_init_defaults(&value);
	STRNCPY(value.server, "192.0.2.10:3390", sizeof(value.server));
	if (!rdesktop_config_parse_server_endpoint(&value) ||
	    strcmp(value.server, "192.0.2.10") != 0 || value.port != 3390)
		return 7;

	rdesktop_config_init_defaults(&value);
	STRNCPY(value.server, "server.example.com:invalid", sizeof(value.server));
	if (rdesktop_config_parse_server_endpoint(&value))
		return 8;

#ifdef IPv6
	rdesktop_config_init_defaults(&value);
	STRNCPY(value.server, "[2001:db8::1]:3391", sizeof(value.server));
	if (!rdesktop_config_parse_server_endpoint(&value) ||
	    strcmp(value.server, "2001:db8::1") != 0 || value.port != 3391)
		return 9;

	rdesktop_config_init_defaults(&value);
	STRNCPY(value.server, "2001:db8::1", sizeof(value.server));
	if (!rdesktop_config_parse_server_endpoint(&value) ||
	    strcmp(value.server, "2001:db8::1") != 0 || value.port != TCP_PORT_RDP)
		return 10;
#endif

	return 0;
}
