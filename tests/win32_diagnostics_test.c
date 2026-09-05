#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define main rdesktop_program_main
#include "../win32_diagnostics.h"
#undef main

static void
check(int condition, const char *message)
{
	if (!condition)
	{
		fprintf(stderr, "FAIL: %s\n", message);
		exit(1);
	}
}

static void
capture(void *context, const char *message)
{
	win32_diagnostic_log_append((WIN32_DIAGNOSTIC_LOG *) context, message);
}

int
main(void)
{
	WIN32_DIAGNOSTIC_LOG log;
	char line[256];
	int index;

	win32_diagnostic_log_init(&log);
	win32_diagnostic_log_append(&log, "first");
	win32_diagnostic_log_append(&log, "second");
	check(!strcmp(win32_diagnostic_log_text(&log), "first\r\nsecond\r\n"),
	      "append preserves complete lines");

	for (index = 0; index < 400; index++)
	{
		snprintf(line, sizeof(line), "event-%03d: bounded diagnostic data", index);
		win32_diagnostic_log_append(&log, line);
	}
	check(log.length < WIN32_DIAGNOSTIC_LOG_CAPACITY, "log remains bounded");
	check(strstr(log.data, "event-399") != NULL, "newest event retained");
	check(strstr(log.data, "first") == NULL, "oldest event trimmed");

	win32_diagnostic_log_clear(&log);
	check(log.length == 0 && log.data[0] == '\0', "clear resets log");
	win32_diagnostics_set_sink(capture, &log);
	win32_diagnostics_emit("routed");
	check(!strcmp(log.data, "routed\r\n"), "sink receives event");
	win32_diagnostics_set_input_enabled(True);
	check(win32_diagnostics_input_enabled(), "input diagnostic enabled");
	win32_diagnostics_set_input_enabled(False);
	check(!win32_diagnostics_input_enabled(), "input diagnostic disabled");
	win32_diagnostics_set_sink(NULL, NULL);

	puts("Win32 diagnostic buffer and sink: PASS");
	return 0;
}
