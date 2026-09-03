#define main rdesktop_program_main
#include "../config.h"
#undef main

RDP_VERSION g_rdp_version = RDP_V4;

int
main(void)
{
	RDESKTOP_CONFIG value;
	rdesktop_config_init_defaults(&value);
	if (value.rdp_version != RDP_V5 || !rdesktop_config_validate(&value))
		return 1;
	value.rdp_version = RDP_V4;
	if (!rdesktop_config_validate(&value))
		return 2;
	value.rdp_version = (RDP_VERSION) 0;
	if (rdesktop_config_validate(&value))
		return 3;
	value.rdp_version = RDP_V5;
	g_rdp_version = RDP_V4;
	rdesktop_config_apply_to_legacy(&value);
	return g_rdp_version == RDP_V5 ? 0 : 4;
}
