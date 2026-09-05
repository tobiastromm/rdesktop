/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RDESKTOP_WIN32_GUI_H
#define RDESKTOP_WIN32_GUI_H

#include "config.h"

/* Returns True when Connect was selected and config is valid. */
RD_BOOL win32_config_dialog(RDESKTOP_CONFIG *config, char *error, size_t error_size);
void win32_gui_log_event(const char *message);
void win32_gui_show_connection_failure(const RDESKTOP_CONFIG *config, int result);
HWND win32_gui_show_connecting(const RDESKTOP_CONFIG *config);
void win32_gui_close_connecting(HWND window);
RD_BOOL win32_gui_connect_cancelled(void);

#endif
/* SPDX-License-Identifier: GPL-3.0-or-later */
