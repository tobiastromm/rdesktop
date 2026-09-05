/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RDESKTOP_WIN32_GUI_PREFS_H
#define RDESKTOP_WIN32_GUI_PREFS_H
#include "config.h"
#define WIN32_GUI_MRU_COUNT 5
void win32_gui_prefs_load_servers(char servers[WIN32_GUI_MRU_COUNT][256]);
void win32_gui_prefs_remember_server(const char *server);
RD_BOOL win32_gui_prefs_load_profile(const char *server, RDESKTOP_CONFIG *config);
void win32_gui_prefs_remember_profile(const RDESKTOP_CONFIG *config);
void win32_gui_prefs_load_global(char *locale, size_t locale_size, RD_BOOL *expanded);
void win32_gui_prefs_save_global(const char *locale, RD_BOOL expanded);
#endif
/* SPDX-License-Identifier: GPL-3.0-or-later */

