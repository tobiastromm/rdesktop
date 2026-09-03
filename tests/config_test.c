#define main rdesktop_program_main
#include "../config.h"
#undef main

RDP_VERSION g_rdp_version = RDP_V4;
char g_hostname[16] = "legacy-host";

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
	return strcmp(g_hostname, "client-host") == 0 ? 0 : 5;
}
