/* -*- c-basic-offset: 8 -*- */
#include "config.h"

extern RDP_VERSION g_rdp_version;
extern char g_hostname[16];

void
rdesktop_config_init_defaults(RDESKTOP_CONFIG *config)
{
	memset(config, 0, sizeof(*config));
	config->rdp_version = RDP_V5;
}

RD_BOOL
rdesktop_config_validate(const RDESKTOP_CONFIG *config)
{
	return config != NULL &&
		(config->rdp_version == RDP_V4 || config->rdp_version == RDP_V5);
}

void
rdesktop_config_apply_to_legacy(const RDESKTOP_CONFIG *config)
{
	g_rdp_version = config->rdp_version;
	STRNCPY(g_hostname, config->hostname, sizeof(g_hostname));
}
