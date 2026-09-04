/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rdesktop.h"
#include "win32_clipboard.h"
#include "win32_clipboard_logic.h"

extern RD_BOOL g_rdpclip;

typedef BOOL (WINAPI *ADD_CLIPBOARD_LISTENER)(HWND);
typedef BOOL (WINAPI *REMOVE_CLIPBOARD_LISTENER)(HWND);
#define WM_RDESKTOP_CLIPBOARD_REQUEST (WM_APP + 41)

static HWND clipboard_window;
static HWND clipboard_next_viewer;
static RD_BOOL clipboard_modern_listener;
static RD_BOOL clipboard_remote_unicode;
static RD_BOOL clipboard_remote_text;
static RD_BOOL clipboard_channel_ready;
static uint32 clipboard_pending_format;
static DWORD clipboard_suppress_sequence;
static int clipboard_debug_initialized;
static RD_BOOL clipboard_debug_enabled;

static RD_BOOL
win32_clipboard_debug(void)
{
	if (!clipboard_debug_initialized)
	{
		const char *value = getenv("RDESKTOP_WIN32_CLIPBOARD_DEBUG");
		clipboard_debug_enabled = value && value[0] && strcmp(value, "0");
		clipboard_debug_initialized = 1;
	}
	return clipboard_debug_enabled;
}

static RD_BOOL
win32_clipboard_open(HWND owner)
{
	return OpenClipboard(owner) != 0;
}

static void
win32_clipboard_announce_local(void)
{
	uint8 formats[72];
	uint32 length = 0;
	if (!win32_clipboard_may_announce(g_rdpclip, clipboard_channel_ready) ||
	    !win32_clipboard_open(clipboard_window))
		return;
	if (IsClipboardFormatAvailable(CF_UNICODETEXT))
	{
		buf_out_uint32(formats + length, CF_UNICODETEXT);
		memset(formats + length + 4, 0, 32);
		length += 36;
	}
	if (IsClipboardFormatAvailable(CF_TEXT))
	{
		buf_out_uint32(formats + length, CF_TEXT);
		memset(formats + length + 4, 0, 32);
		length += 36;
	}
	CloseClipboard();
	if (length)
		cliprdr_send_native_format_announce(formats, length);
}

static void
win32_clipboard_set_remote(uint8 *data, uint32 length, uint32 format)
{
	HGLOBAL unicode_memory = NULL;
	HGLOBAL ansi_memory = NULL;
	WCHAR *unicode = NULL;
	char *ansi = NULL;
	int unicode_chars = 0;
	int ansi_bytes = 0;

	if (format == CF_UNICODETEXT)
	{
		unicode_chars = (int)(length / sizeof(WCHAR));
		while (unicode_chars > 0 && ((WCHAR *)data)[unicode_chars - 1] == 0)
			unicode_chars--;
		unicode_memory = GlobalAlloc(GMEM_MOVEABLE,
					     (unicode_chars + 1) * sizeof(WCHAR));
		if (unicode_memory)
		{
			unicode = (WCHAR *)GlobalLock(unicode_memory);
			if (unicode)
			{
				memcpy(unicode, data, unicode_chars * sizeof(WCHAR));
				unicode[unicode_chars] = 0;
				GlobalUnlock(unicode_memory);
			}
		}
	}
	else if (format == CF_TEXT)
	{
		int source_bytes = (int)length;
		while (source_bytes > 0 && data[source_bytes - 1] == 0)
			source_bytes--;
		unicode_chars = MultiByteToWideChar(CP_ACP, 0, (char *)data,
						 source_bytes, NULL, 0);
		unicode_memory = GlobalAlloc(GMEM_MOVEABLE,
					     (unicode_chars + 1) * sizeof(WCHAR));
		if (unicode_memory)
		{
			unicode = (WCHAR *)GlobalLock(unicode_memory);
			if (unicode)
			{
				MultiByteToWideChar(CP_ACP, 0, (char *)data, source_bytes,
						    unicode, unicode_chars);
				unicode[unicode_chars] = 0;
				GlobalUnlock(unicode_memory);
			}
		}
	}

	if (!unicode_memory || !win32_clipboard_open(clipboard_window))
	{
		if (unicode_memory)
			GlobalFree(unicode_memory);
		return;
	}
	EmptyClipboard();
	if (!SetClipboardData(CF_UNICODETEXT, unicode_memory))
		GlobalFree(unicode_memory);
	else
		unicode_memory = NULL;

	unicode = (WCHAR *)GetClipboardData(CF_UNICODETEXT);
	if (unicode)
	{
		ansi_bytes = WideCharToMultiByte(CP_ACP, 0, unicode, -1,
						NULL, 0, NULL, NULL);
		ansi_memory = GlobalAlloc(GMEM_MOVEABLE, ansi_bytes);
		if (ansi_memory)
		{
			ansi = (char *)GlobalLock(ansi_memory);
			if (ansi)
			{
				WideCharToMultiByte(CP_ACP, 0, unicode, -1, ansi,
						    ansi_bytes, NULL, NULL);
				GlobalUnlock(ansi_memory);
				if (!SetClipboardData(CF_TEXT, ansi_memory))
					GlobalFree(ansi_memory);
			}
			else
				GlobalFree(ansi_memory);
		}
	}
	clipboard_suppress_sequence = GetClipboardSequenceNumber();
	if (win32_clipboard_debug())
		fprintf(stderr, "WIN32 CLIPBOARD remote format=%lu bytes=%lu\n",
			(unsigned long)format, (unsigned long)length);
	CloseClipboard();
}

void
win32_clipboard_attach(HWND window)
{
	HMODULE user32;
	ADD_CLIPBOARD_LISTENER add_listener;
	union { FARPROC proc; ADD_CLIPBOARD_LISTENER listener; } resolver;
	if (!g_rdpclip)
		return;
	clipboard_window = window;
	user32 = GetModuleHandleA("user32.dll");
	resolver.proc = user32 ? GetProcAddress(user32, "AddClipboardFormatListener") : NULL;
	add_listener = resolver.listener;
	if (add_listener && add_listener(window))
		clipboard_modern_listener = True;
	else
		clipboard_next_viewer = SetClipboardViewer(window);
}

void
win32_clipboard_detach(void)
{
	HMODULE user32 = GetModuleHandleA("user32.dll");
	REMOVE_CLIPBOARD_LISTENER remove_listener;
	union { FARPROC proc; REMOVE_CLIPBOARD_LISTENER listener; } resolver;
	resolver.proc = user32 ? GetProcAddress(user32, "RemoveClipboardFormatListener") : NULL;
	remove_listener = resolver.listener;
	if (clipboard_window)
	{
		if (clipboard_modern_listener && remove_listener)
			remove_listener(clipboard_window);
		else
			ChangeClipboardChain(clipboard_window, clipboard_next_viewer);
	}
	clipboard_window = NULL;
	clipboard_next_viewer = NULL;
	clipboard_modern_listener = False;
	clipboard_channel_ready = False;
	clipboard_pending_format = 0;
}

RD_BOOL
win32_clipboard_handle_message(HWND window, UINT message, WPARAM wparam,
			       LPARAM lparam, LRESULT *result)
{
	DWORD sequence;
	if (message == WM_RDESKTOP_CLIPBOARD_REQUEST)
	{
		if (clipboard_pending_format == (uint32)wparam)
			cliprdr_send_data_request(clipboard_pending_format);
		*result = 0;
		return True;
	}
	if (message == WM_CLIPBOARDUPDATE || message == WM_DRAWCLIPBOARD)
	{
		sequence = GetClipboardSequenceNumber();
		if (GetClipboardOwner() != clipboard_window &&
		    (!clipboard_suppress_sequence || sequence != clipboard_suppress_sequence))
			win32_clipboard_announce_local();
		clipboard_suppress_sequence = 0;
		if (message == WM_DRAWCLIPBOARD && clipboard_next_viewer)
			SendMessage(clipboard_next_viewer, message, wparam, lparam);
		*result = 0;
		return True;
	}
	if (message == WM_CHANGECBCHAIN)
	{
		if ((HWND)wparam == clipboard_next_viewer)
			clipboard_next_viewer = (HWND)lparam;
		else if (clipboard_next_viewer)
			SendMessage(clipboard_next_viewer, message, wparam, lparam);
		*result = 0;
		return True;
	}
	UNUSED(window);
	return False;
}

void
ui_clip_format_announce(uint8 *data, uint32 length)
{
	uint32 offset;
	clipboard_remote_unicode = False;
	clipboard_remote_text = False;
	for (offset = 0; offset + 4 <= length; offset += 36)
	{
		uint32 format = (uint32)data[offset] |
			((uint32)data[offset + 1] << 8) |
			((uint32)data[offset + 2] << 16) |
			((uint32)data[offset + 3] << 24);
		if (format == CF_UNICODETEXT)
			clipboard_remote_unicode = True;
		if (format == CF_TEXT)
			clipboard_remote_text = True;
	}
	clipboard_pending_format = win32_clipboard_choose_format(
		clipboard_remote_unicode, clipboard_remote_text,
		CF_UNICODETEXT, CF_TEXT);
	if (clipboard_pending_format && clipboard_window)
		PostMessage(clipboard_window, WM_RDESKTOP_CLIPBOARD_REQUEST,
			    (WPARAM)clipboard_pending_format, 0);
}

void
ui_clip_handle_data(uint8 *data, uint32 length)
{
	if (clipboard_pending_format)
		win32_clipboard_set_remote(data, length, clipboard_pending_format);
	clipboard_pending_format = 0;
}

void ui_clip_request_failed(void) { clipboard_pending_format = 0; }

void
ui_clip_request_data(uint32 format)
{
	HANDLE handle;
	uint8 *source;
	uint8 *copy;
	uint32 length;
	if ((format != CF_TEXT && format != CF_UNICODETEXT) ||
	    !win32_clipboard_open(clipboard_window))
	{
		cliprdr_send_data(NULL, 0);
		return;
	}
	handle = GetClipboardData(format);
	if (!handle)
	{
		CloseClipboard();
		cliprdr_send_data(NULL, 0);
		return;
	}
	source = (uint8 *)GlobalLock(handle);
	length = (uint32)GlobalSize(handle);
	if (!source || !length)
	{
		if (source)
			GlobalUnlock(handle);
		CloseClipboard();
		cliprdr_send_data(NULL, 0);
		return;
	}
	copy = (uint8 *)xmalloc(length);
	memcpy(copy, source, length);
	GlobalUnlock(handle);
	CloseClipboard();
	cliprdr_send_data(copy, length);
	xfree(copy);
}

void
ui_clip_sync(void)
{
	clipboard_channel_ready = True;
	win32_clipboard_announce_local();
}

void ui_clip_set_mode(const char *optarg) { UNUSED(optarg); }
void xclip_init(void) { if (g_rdpclip) cliprdr_init(); }
void xclip_deinit(void) { win32_clipboard_detach(); }
