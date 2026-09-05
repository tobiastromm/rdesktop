/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "frontend.h"
#include "win32_gui.h"
#include "win32_gui_prefs.h"
#include "win32_profile.h"

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int show_command)
{
	RDESKTOP_CONFIG config;
	char error[256];
	char username[256];
	DWORD username_length = sizeof(username);

	UNUSED(instance);
	UNUSED(previous_instance);
	UNUSED(command_line);
	UNUSED(show_command);
	rdesktop_config_init(&config);
	if (GetUserNameA(username, &username_length))
		STRNCPY(config.username, username, sizeof(config.username));
	while (win32_config_dialog(&config, error, sizeof(error)))
	{
		int result;
		HWND connecting;
		win32_gui_log_event("Session starting");
		rdesktop_frontend_prepare_session();
		connecting=win32_gui_show_connecting(&config);
		result=rdesktop_frontend_run(0, NULL, &config);
		win32_gui_close_connecting(connecting);
		if(win32_profile_should_commit(rdesktop_frontend_was_activated()))
			win32_gui_prefs_remember_profile(&config);
		else if(result!=EXRD_WINDOW_CLOSED&&!win32_gui_connect_cancelled())
			win32_gui_show_connection_failure(&config,result);
		win32_gui_log_event("Session ended; returning to configuration");
		rdesktop_config_clear_password(&config);
	}
	return EX_OK;
}
/* SPDX-License-Identifier: GPL-3.0-or-later */
