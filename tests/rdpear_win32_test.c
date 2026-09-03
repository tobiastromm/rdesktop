#define main rdesktop_program_main
#include "../rdesktop.h"
#undef main

RD_BOOL g_remote_guard = False;

void
logger(log_subject_t subject, log_level_t level, char *format, ...)
{
	UNUSED(subject);
	UNUSED(level);
	UNUSED(format);
}

int
main(void)
{
	if (!rdpear_preflight() || !rdpear_init())
		return 1;

	g_remote_guard = True;
	if (rdpear_preflight() || rdpear_init())
		return 2;

	return 0;
}
