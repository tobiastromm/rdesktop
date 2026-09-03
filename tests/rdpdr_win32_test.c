#define main rdesktop_program_main
#include "../rdesktop.h"
#undef main

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
	uint32 id = 0;

	if (serial_enum_devices(&id, ":COM1=device") >= 0 || id != 0)
		return 1;
	if (parallel_enum_devices(&id, ":LPT1=device") >= 0 || id != 0)
		return 2;
	if (printer_enum_devices(&id, ":printer") >= 0 || id != 0)
		return 3;
	if (disk_enum_devices(&id, ":share=path") >= 0 || id != 0)
		return 4;
	return 0;
}
