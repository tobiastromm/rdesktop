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
char g_password[64] = "";
RD_BOOL g_fullscreen = False;
RDESKTOP_WINDOW_MODE g_window_mode = RDESKTOP_WINDOW_CLIENT_SIZE;
RD_BOOL g_show_connection_bar = True;
RD_BOOL g_rdpsnd = False;

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

static int
session_fields_equal(const RDESKTOP_CONFIG *left, const RDESKTOP_CONFIG *right)
{
	return strcmp(left->server, right->server) == 0 &&
		left->port == right->port &&
		strcmp(left->username, right->username) == 0 &&
		strcmp(left->domain, right->domain) == 0 &&
		left->rdp_version == right->rdp_version &&
		left->colour_depth == right->colour_depth &&
		left->width == right->width && left->height == right->height &&
		left->geometry_type == right->geometry_type &&
		strcmp(left->keymap, right->keymap) == 0 &&
		left->fullscreen == right->fullscreen &&
		left->window_mode == right->window_mode &&
		left->audio_mode == right->audio_mode &&
		left->clipboard_enabled == right->clipboard_enabled;
}

int
main(void)
{
	RDESKTOP_CONFIG value;
	char domain[256];
	rdesktop_config_init_defaults(&value);
	if (value.password_present || value.password_from_prompt ||
	    value.password_prompt_requested || value.password[0] != '\0' ||
	    value.fullscreen)
		return 37;
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
	if (rdesktop_config_parse_geometry(&value, "800x600") != 0 ||
	    value.geometry_type != Fixed || value.width != 800 || value.height != 600)
		return 49;
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

	rdesktop_config_set_password(&value, "test-secret", False);
	if (!value.password_present || value.password_from_prompt)
		return 29;
	rdesktop_config_apply_credentials(&value);
	if (g_password[0] == '\0')
		return 30;
	rdesktop_config_clear_credentials(&value);
	if (value.password_present || value.password_from_prompt ||
	    value.password_prompt_requested)
		return 31;
	{
		size_t index;
		for (index = 0; index < sizeof(value.password); index++)
		{
			if (value.password[index] != '\0')
				return 32;
		}
	}
	rdesktop_config_clear_legacy_credentials();
	if (g_password[0] != '\0')
		return 36;
	rdesktop_config_set_password(&value, "", False);
	if (!value.password_present || value.password[0] != '\0')
		return 33;
	rdesktop_config_request_password_prompt(&value);
	if (!value.password_prompt_requested)
		return 34;
	rdesktop_config_set_password(&value, "", True);
	if (!value.password_present || !value.password_from_prompt)
		return 35;

	rdesktop_config_init_defaults(&value);
	value.fullscreen = True;
	rdesktop_config_apply_to_legacy(&value, domain, sizeof(domain));
	if (!g_fullscreen || g_window_size_type != Fullscreen)
		return 38;
	rdesktop_config_init_defaults(&value);
	if (rdesktop_config_parse_geometry(&value, "800x600") != 0)
		return 39;
	rdesktop_config_apply_to_legacy(&value, domain, sizeof(domain));
	if (g_fullscreen || g_window_size_type != Fixed)
		return 40;
	value.window_mode = RDESKTOP_WINDOW_FILL_AVAILABLE;
	rdesktop_config_apply_to_legacy(&value, domain, sizeof(domain));
	if (g_window_mode != RDESKTOP_WINDOW_FILL_AVAILABLE ||
	    g_window_size_type != Fixed ||
	    g_requested_session_width != 800 ||
	    g_requested_session_height != 600)
		return 48;

	rdesktop_config_init_defaults(&value);
	if (value.audio_mode != RDESKTOP_AUDIO_DISABLED ||
	    value.audio_driver_options[0] != '\0')
		return 41;
	value.audio_mode = RDESKTOP_AUDIO_LOCAL;
	STRNCPY(value.audio_driver_options, "winmm", sizeof(value.audio_driver_options));
	{
		char *audio_options = NULL;
		rdesktop_config_apply_audio_to_legacy(&value, &audio_options);
		if (!g_rdpsnd || audio_options == NULL || strcmp(audio_options, "winmm") != 0)
			return 42;
	}
	value.audio_mode = RDESKTOP_AUDIO_DISABLED;
	value.audio_driver_options[0] = '\0';
	{
		char *audio_options = (char *) "stale";
		rdesktop_config_apply_audio_to_legacy(&value, &audio_options);
		if (g_rdpsnd || audio_options != NULL)
			return 43;
	}

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

	{
		RDESKTOP_CONFIG cli_config;
		RDESKTOP_CONFIG gui_config;

		rdesktop_config_init_defaults(&cli_config);
		STRNCPY(cli_config.server, "192.0.2.25:3395", sizeof(cli_config.server));
		if (!rdesktop_config_parse_server_endpoint(&cli_config))
			return 44;
		STRNCPY(cli_config.username, "alice", sizeof(cli_config.username));
		cli_config.username_set = True;
		STRNCPY(cli_config.domain, "EXAMPLE", sizeof(cli_config.domain));
		cli_config.rdp_version = RDP_V4;
		cli_config.colour_depth = 8;
		if (rdesktop_config_parse_geometry(&cli_config, "1024x768") != 0)
			return 45;
		STRNCPY(cli_config.keymap, "pt-br", sizeof(cli_config.keymap));
		cli_config.audio_mode = RDESKTOP_AUDIO_DISABLED;
		cli_config.clipboard_enabled = True;

		rdesktop_config_init_defaults(&gui_config);
		STRNCPY(gui_config.server, "192.0.2.25", sizeof(gui_config.server));
		gui_config.port = 3395;
		STRNCPY(gui_config.username, "alice", sizeof(gui_config.username));
		gui_config.username_set = True;
		STRNCPY(gui_config.domain, "EXAMPLE", sizeof(gui_config.domain));
		gui_config.rdp_version = RDP_V4;
		gui_config.colour_depth = 8;
		if (rdesktop_config_parse_geometry(&gui_config, "1024x768") != 0)
			return 46;
		STRNCPY(gui_config.keymap, "pt-br", sizeof(gui_config.keymap));
		gui_config.audio_mode = RDESKTOP_AUDIO_DISABLED;
		gui_config.clipboard_enabled = True;

		if (!session_fields_equal(&cli_config, &gui_config))
			return 47;
	}

	return 0;
}
