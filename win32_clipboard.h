/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef WIN32_CLIPBOARD_H
#define WIN32_CLIPBOARD_H

#include <windows.h>
#include "rdesktop.h"

void win32_clipboard_attach(HWND window);
void win32_clipboard_detach(void);
RD_BOOL win32_clipboard_handle_message(HWND window, UINT message,
			       WPARAM wparam, LPARAM lparam, LRESULT *result);

#endif
