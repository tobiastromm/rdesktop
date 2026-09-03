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
	fd_set read_fds;
	fd_set write_fds;
	int max_fd = 0;

	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	if (!addin_init())
		return 1;
	addin_add_fds(&max_fd, &read_fds, &write_fds);
	addin_check_fds(&read_fds, &write_fds);
	addin_deinit();
	if (addin_parse_option(":TEST=ignored"))
		return 2;
	return 0;
}
