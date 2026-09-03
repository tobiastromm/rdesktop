/* -*- c-basic-offset: 8 -*-
   Win32 capability boundary for Remote Credential Guard/RDPEAR. */

#include "rdesktop.h"

extern RD_BOOL g_remote_guard;

RD_BOOL
rdpear_preflight(void)
{
	if (!g_remote_guard)
		return True;

	logger(Core, Error,
	       "Remote Guard/RDPEAR is not supported on this Win32 build");
	return False;
}

RD_BOOL
rdpear_init(void)
{
	return !g_remote_guard;
}

STREAM
rdpear_call_helper(const char *package_name, STREAM request)
{
	UNUSED(package_name);
	UNUSED(request);
	return NULL;
}
