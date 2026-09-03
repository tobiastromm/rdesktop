/* -*- c-basic-offset: 8 -*- */
#include "config.h"
#include <errno.h>

extern RDP_VERSION g_rdp_version;
extern char g_hostname[16];
extern int g_tcp_port_rdp;
extern char *g_username;
extern uint32 g_requested_session_width;
extern uint32 g_requested_session_height;
extern window_size_type_t g_window_size_type;
extern int g_xpos;
extern int g_ypos;
extern int g_pos;
extern int g_dpi;
extern int g_server_depth;
extern char g_keymapname[PATH_MAX];
extern char g_password[64];

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
	config->width = 1024;
	config->height = 768;
	config->geometry_type = Fixed;
	config->colour_depth = -1;
}

RD_BOOL
rdesktop_config_validate(const RDESKTOP_CONFIG *config)
{
	return config != NULL &&
		(config->rdp_version == RDP_V4 || config->rdp_version == RDP_V5) &&
		config->port != 0 &&
		(config->colour_depth == -1 || config->colour_depth == 8 ||
		 config->colour_depth == 15 || config->colour_depth == 16 ||
		 config->colour_depth == 24 || config->colour_depth == 32);
}

int
rdesktop_config_parse_geometry(RDESKTOP_CONFIG *config, const char *geometry)
{
	sint32 value;
	const char *cursor;
	char *end;

	if (strcmp(geometry, "workarea") == 0)
	{
		config->geometry_type = Workarea;
		config->geometry_set = True;
		return 0;
	}
	cursor = geometry;
	value = strtol(cursor, &end, 10);
	if (cursor == end || value <= 0)
	{
		logger(Core, Error, "invalid geometry, expected positive integer for width");
		return -1;
	}
	config->width = value;
	cursor = end;
	if (*cursor != '%' && *cursor != 'x')
	{
		logger(Core, Error, "invalid geometry, expected '%%' or 'x' after width");
		return -1;
	}
	if (*cursor == '%')
	{
		config->geometry_type = PercentageOfScreen;
		cursor++;
	}
	if (*cursor == 'x')
	{
		cursor++;
		value = strtol(cursor, &end, 10);
		if (cursor == end || value <= 0)
		{
			logger(Core, Error, "invalid geometry, expected positive integer for height");
			return -1;
		}
		config->height = value;
		cursor = end;
		if (*cursor == '%' && config->geometry_type == Fixed)
		{
			logger(Core, Error, "invalid geometry, unexpected '%%' after height");
			return -1;
		}
		if (config->geometry_type == PercentageOfScreen)
		{
			if (*cursor != '%')
			{
				logger(Core, Error, "invalid geometry, expected '%%' after height");
				return -1;
			}
			cursor++;
		}
	}
	else if (config->geometry_type == PercentageOfScreen)
		config->height = config->width;
	else
	{
		logger(Core, Error, "invalid geometry, missing height (WxH)");
		return -1;
	}
	if (*cursor == '@')
	{
		cursor++;
		value = strtol(cursor, &end, 10);
		if (cursor == end || value <= 0)
		{
			logger(Core, Error, "invalid geometry, expected positive integer for DPI");
			return -1;
		}
		config->dpi = value;
		cursor = end;
	}
	if (*cursor == '+' || *cursor == '-')
	{
		value = strtol(cursor, &end, 10);
		if (cursor == end)
		{
			logger(Core, Error, "invalid geometry, expected an integer for X position");
			return -1;
		}
		config->position |= value < 0 ? 2 : 1;
		config->x = value;
		cursor = end;
	}
	if (*cursor == '+' || *cursor == '-')
	{
		value = strtol(cursor, &end, 10);
		if (cursor == end)
		{
			logger(Core, Error, "invalid geometry, expected an integer for Y position");
			return -1;
		}
		config->position |= value < 0 ? 4 : 1;
		config->y = value;
		cursor = end;
	}
	if (*cursor != '\0')
	{
		logger(Core, Error, "invalid geometry, unexpected characters at end of string");
		return -1;
	}
	config->geometry_set = True;
	return 0;
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
rdesktop_config_set_password(RDESKTOP_CONFIG *config, const char *password,
			     RD_BOOL from_prompt)
{
	STRNCPY(config->password, password, sizeof(config->password));
	config->password_present = True;
	config->password_from_prompt = from_prompt;
}

void
rdesktop_config_request_password_prompt(RDESKTOP_CONFIG *config)
{
	config->password_prompt_requested = True;
}

void
rdesktop_config_apply_credentials(const RDESKTOP_CONFIG *config)
{
	if (config->password_present)
		STRNCPY(g_password, config->password, sizeof(g_password));
}

void
rdesktop_config_clear_credentials(RDESKTOP_CONFIG *config)
{
	volatile char *cursor = config->password;
	size_t remaining = sizeof(config->password);

	while (remaining-- != 0)
		*cursor++ = '\0';
	config->password_present = False;
	config->password_from_prompt = False;
	config->password_prompt_requested = False;
}

void
rdesktop_config_clear_legacy_credentials(void)
{
	volatile char *cursor = g_password;
	size_t remaining = sizeof(g_password);

	while (remaining-- != 0)
		*cursor++ = '\0';
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
	if (config->geometry_set)
	{
		g_requested_session_width = config->width;
		g_requested_session_height = config->height;
		g_window_size_type = config->geometry_type;
		g_xpos = config->x;
		g_ypos = config->y;
		g_pos = config->position;
		g_dpi = config->dpi;
	}
	g_server_depth = config->colour_depth;
	STRNCPY(g_keymapname, config->keymap, sizeof(g_keymapname));
}
