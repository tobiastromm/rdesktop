#define main rdesktop_program_main
#include "../config.h"
#undef main

RDP_VERSION g_rdp_version = RDP_V4;
char g_hostname[16] = "legacy-host";
int g_tcp_port_rdp = TCP_PORT_RDP;
char *g_username = NULL;
uint32 g_requested_session_width = 1024;
uint32 g_requested_session_height = 768;
window_size_type_t g_window_size_type = Fixed;
int g_xpos = 0;
int g_ypos = 0;
int g_pos = 0;
int g_dpi = 0;
int g_server_depth = -1;
char g_keymapname[PATH_MAX] = "";

void
logger(log_subject_t subject, log_level_t level, char *format, ...)
{
	(void) subject;
	(void) level;
	(void) format;
}

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
	if (value.width != 1024 || value.height != 768 ||
	    value.geometry_type != Fixed || value.geometry_set ||
	    value.colour_depth != -1 || value.keymap[0] != '\0')
		return 15;
	if (rdesktop_config_parse_geometry(&value, "1280x720") != 0 ||
	    value.width != 1280 || value.height != 720 || value.geometry_type != Fixed)
		return 16;
	rdesktop_config_init_defaults(&value);
	if (rdesktop_config_parse_geometry(&value, "80%") != 0 ||
	    value.width != 80 || value.height != 80 || value.geometry_type != PercentageOfScreen)
		return 17;
	rdesktop_config_init_defaults(&value);
	if (rdesktop_config_parse_geometry(&value, "80%x60%@144-10+20") != 0 ||
	    value.width != 80 || value.height != 60 || value.dpi != 144 ||
	    value.x != -10 || value.y != 20 || value.position != 3)
		return 18;
	rdesktop_config_init_defaults(&value);
	if (rdesktop_config_parse_geometry(&value, "workarea") != 0 ||
	    value.geometry_type != Workarea)
		return 19;
	rdesktop_config_init_defaults(&value);
	if (rdesktop_config_parse_geometry(&value, "800x") == 0)
		return 20;

	value.colour_depth = 8;
	if (!rdesktop_config_validate(&value)) return 21;
	value.colour_depth = 15;
	if (!rdesktop_config_validate(&value)) return 22;
	value.colour_depth = 16;
	if (!rdesktop_config_validate(&value)) return 23;
	value.colour_depth = 24;
	if (!rdesktop_config_validate(&value)) return 24;
	value.colour_depth = 32;
	if (!rdesktop_config_validate(&value)) return 25;
	value.colour_depth = 12;
	if (rdesktop_config_validate(&value)) return 26;

	value.colour_depth = 16;
	STRNCPY(value.keymap, "pt-br", sizeof(value.keymap));
	if (rdesktop_config_parse_geometry(&value, "640x480") != 0)
		return 27;
	rdesktop_config_apply_to_legacy(&value, domain, sizeof(domain));
	if (g_server_depth != 16 || strcmp(g_keymapname, "pt-br") != 0 ||
	    g_requested_session_width != value.width ||
	    g_requested_session_height != value.height)
		return 28;

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
