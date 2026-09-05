#ifndef RDESKTOP_WIN32_DIAGNOSTICS_H
#define RDESKTOP_WIN32_DIAGNOSTICS_H

#include <stddef.h>
#include "rdesktop.h"

#define WIN32_DIAGNOSTIC_LOG_CAPACITY 8192

typedef struct win32_diagnostic_log
{
	char data[WIN32_DIAGNOSTIC_LOG_CAPACITY];
	size_t length;
}
WIN32_DIAGNOSTIC_LOG;

typedef void (*WIN32_DIAGNOSTIC_SINK)(void *context, const char *message);

void win32_diagnostic_log_init(WIN32_DIAGNOSTIC_LOG *log);
void win32_diagnostic_log_append(WIN32_DIAGNOSTIC_LOG *log, const char *message);
void win32_diagnostic_log_clear(WIN32_DIAGNOSTIC_LOG *log);
const char *win32_diagnostic_log_text(const WIN32_DIAGNOSTIC_LOG *log);

void win32_diagnostics_set_sink(WIN32_DIAGNOSTIC_SINK sink, void *context);
void win32_diagnostics_emit(const char *message);
void win32_diagnostics_set_input_enabled(RD_BOOL enabled);
RD_BOOL win32_diagnostics_input_enabled(void);

#endif
