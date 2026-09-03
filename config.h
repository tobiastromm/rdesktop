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
	char username[256];
	RD_BOOL username_set;
	char domain[256];
	uint32 width;
	uint32 height;
	window_size_type_t geometry_type;
	int x;
	int y;
	int position;
	int dpi;
	RD_BOOL geometry_set;
	int colour_depth;
	char keymap[PATH_MAX];
}
RDESKTOP_CONFIG;

void rdesktop_config_init_defaults(RDESKTOP_CONFIG *config);
RD_BOOL rdesktop_config_validate(const RDESKTOP_CONFIG *config);
RD_BOOL rdesktop_config_parse_server_endpoint(RDESKTOP_CONFIG *config);
int rdesktop_config_parse_geometry(RDESKTOP_CONFIG *config, const char *geometry);
void rdesktop_config_apply_to_legacy(const RDESKTOP_CONFIG *config, char *domain,
				     size_t domain_size);

#endif
