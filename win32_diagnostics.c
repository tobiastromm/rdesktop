#include <string.h>
#include "win32_diagnostics.h"

static WIN32_DIAGNOSTIC_SINK diagnostic_sink;
static void *diagnostic_sink_context;
static RD_BOOL input_diagnostics_enabled;

void
win32_diagnostic_log_init(WIN32_DIAGNOSTIC_LOG *log)
{
	if (!log)
		return;
	log->length = 0;
	log->data[0] = '\0';
}

void
win32_diagnostic_log_append(WIN32_DIAGNOSTIC_LOG *log, const char *message)
{
	size_t message_length;
	size_t required;
	size_t drop;
	size_t newline;

	if (!log || !message || !message[0])
		return;
	message_length = strlen(message);
	newline = 2;
	if (message_length + newline >= sizeof(log->data))
	{
		message += message_length + newline - sizeof(log->data) + 1;
		message_length = strlen(message);
	}
	required = log->length + message_length + newline;
	if (required >= sizeof(log->data))
	{
		drop = required - sizeof(log->data) + 1;
		while (drop < log->length && log->data[drop] != '\n')
			drop++;
		if (drop < log->length)
			drop++;
		memmove(log->data, log->data + drop, log->length - drop);
		log->length -= drop;
	}
	memcpy(log->data + log->length, message, message_length);
	log->length += message_length;
	log->data[log->length++] = '\r';
	log->data[log->length++] = '\n';
	log->data[log->length] = '\0';
}

void
win32_diagnostic_log_clear(WIN32_DIAGNOSTIC_LOG *log)
{
	win32_diagnostic_log_init(log);
}

const char *
win32_diagnostic_log_text(const WIN32_DIAGNOSTIC_LOG *log)
{
	return log ? log->data : "";
}

void
win32_diagnostics_set_sink(WIN32_DIAGNOSTIC_SINK sink, void *context)
{
	diagnostic_sink = sink;
	diagnostic_sink_context = context;
}

void
win32_diagnostics_emit(const char *message)
{
	if (diagnostic_sink && message)
		diagnostic_sink(diagnostic_sink_context, message);
}

void
win32_diagnostics_set_input_enabled(RD_BOOL enabled)
{
	input_diagnostics_enabled = enabled;
}

RD_BOOL
win32_diagnostics_input_enabled(void)
{
	return input_diagnostics_enabled;
}
