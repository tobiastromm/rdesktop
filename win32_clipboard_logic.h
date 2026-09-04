#ifndef RDESKTOP_WIN32_CLIPBOARD_LOGIC_H
#define RDESKTOP_WIN32_CLIPBOARD_LOGIC_H
static unsigned int win32_clipboard_choose_format(int unicode_available,
		int text_available, unsigned int unicode_format, unsigned int text_format)
{
	return unicode_available ? unicode_format : (text_available ? text_format : 0);
}
static int win32_clipboard_may_announce(int enabled, int monitor_ready)
{
	return enabled && monitor_ready;
}
#endif
