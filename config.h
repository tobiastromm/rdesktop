/* -*- c-basic-offset: 8 -*- */
#ifndef RDESKTOP_CONFIG_H
#define RDESKTOP_CONFIG_H

#include "rdesktop.h"

typedef struct rdesktop_config
{
	/* Add fields only when their configuration source is migrated. */
	RDP_VERSION rdp_version;
	char hostname[16];
	char server[256];
	uint16 port;
}
RDESKTOP_CONFIG;

void rdesktop_config_init_defaults(RDESKTOP_CONFIG *config);
RD_BOOL rdesktop_config_validate(const RDESKTOP_CONFIG *config);
RD_BOOL rdesktop_config_parse_server_endpoint(RDESKTOP_CONFIG *config);
void rdesktop_config_apply_to_legacy(const RDESKTOP_CONFIG *config);

#endif
