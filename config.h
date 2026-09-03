/* -*- c-basic-offset: 8 -*- */
#ifndef RDESKTOP_CONFIG_H
#define RDESKTOP_CONFIG_H

#include "rdesktop.h"

typedef struct rdesktop_config
{
	/* Add fields only when their configuration source is migrated. */
	RDP_VERSION rdp_version;
}
RDESKTOP_CONFIG;

void rdesktop_config_init_defaults(RDESKTOP_CONFIG *config);
RD_BOOL rdesktop_config_validate(const RDESKTOP_CONFIG *config);
void rdesktop_config_apply_to_legacy(const RDESKTOP_CONFIG *config);

#endif
