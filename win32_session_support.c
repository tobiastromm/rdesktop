/* -*- c-basic-offset: 8 -*-
   Minimal Win32 session support outside the native graphics backend. */

#include "rdesktop.h"

RD_BOOL g_dynamic_session_resize = False;
time_t g_wait_for_deactivate_ts;
RD_BOOL g_notify_stamp = False;
FILEINFO g_fileinfo[256];

int
xgui_startup(int *argc, char ***argv)
{
	UNUSED(argc);
	UNUSED(argv);
	return -1;
}

RD_BOOL
xgui_choice_dialog(const char *title, const char *message, const char *accept_label,
		   const char *reject_label)
{
	int result;
	UNUSED(accept_label);
	UNUSED(reject_label);
	result = MessageBoxA(NULL, message, title, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
	return result == IDYES;
}

void
xgui_message_dialog(const char *title, const char *message, const char *button_label)
{
	UNUSED(button_label);
	MessageBoxA(NULL, message, title, MB_ICONERROR | MB_OK);
}

RD_BOOL
xwin_parse_keyboard_ungrab_combo(const char *combo)
{
	UNUSED(combo);
	logger(Core, Error, "Keyboard ungrab combinations are not supported on this Win32 build");
	return False;
}

void
cliprdr_set_mode(const char *optarg)
{
	UNUSED(optarg);
	logger(Core, Warning, "Clipboard redirection is not yet supported on this Win32 build");
}

int
ctrl_init(const char *user, const char *domain, const char *host)
{
	UNUSED(user);
	UNUSED(domain);
	UNUSED(host);
	logger(Core, Error, "SeamlessRDP control is not supported on this Win32 build");
	return -1;
}

void ctrl_cleanup(void) { }
RD_BOOL ctrl_is_slave(void) { return False; }

int
ctrl_send_command(const char *cmd, const char *args)
{
	UNUSED(cmd);
	UNUSED(args);
	return -1;
}

void ctrl_add_fds(int *n, fd_set *rfds) { UNUSED(n); UNUSED(rfds); }
void ctrl_check_fds(fd_set *rfds, fd_set *wfds) { UNUSED(rfds); UNUSED(wfds); }

int
subprocess(char *const argv[], str_handle_lines_t linehandler, void *data)
{
	UNUSED(argv);
	UNUSED(linehandler);
	UNUSED(data);
	logger(Core, Error, "External helper processes are not supported on this Win32 build");
	return -1;
}
