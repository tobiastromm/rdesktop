#define main rdesktop_program_main
#include "../config.h"
#undef main

RDP_VERSION g_rdp_version = RDP_V4;
char g_hostname[16] = "legacy-host";
int g_tcp_port_rdp = TCP_PORT_RDP;

int
main(void)
{
	RDESKTOP_CONFIG value;
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
	rdesktop_config_apply_to_legacy(&value);
	if (g_rdp_version != RDP_V5)
		return 4;
	if (strcmp(g_hostname, "client-host") != 0 || g_tcp_port_rdp != TCP_PORT_RDP)
		return 5;

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
