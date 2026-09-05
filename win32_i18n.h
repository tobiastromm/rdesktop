/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RDESKTOP_WIN32_I18N_H
#define RDESKTOP_WIN32_I18N_H
#ifdef _WIN32
#include <windows.h>
#endif
void win32_i18n_load(const char *locale);
const char *win32_i18n_get(const char *key);
#ifdef _WIN32
const WCHAR *win32_i18n_get_w(const char *key);
#endif
const char *win32_i18n_locale(void);
#endif
/* SPDX-License-Identifier: GPL-3.0-or-later */

