/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "win32_gui.h"
#include "win32_gui_prefs.h"
#include "win32_i18n.h"
#include "win32_diagnostics.h"
#include "frontend.h"
#include <commctrl.h>

#define requested_width width
#define requested_height height
#define requested_colour_depth colour_depth
#define window_size_type geometry_type
#define window_x x
#define window_y y
#define window_position position
#define audio_driver audio_driver_options
#define RDESKTOP_AUDIO_OFF RDESKTOP_AUDIO_DISABLED

#ifdef _WIN32

#define GUI_CLASS L"RdesktopConfigWindow"
#define ID_SERVER 101
#define ID_PORT 102
#define ID_USERNAME 103
#define ID_PASSWORD 104
#define ID_DOMAIN 105
#define ID_RESOLUTION 106
#define ID_FULLSCREEN 107 /* retired; retained to avoid renumbering persisted automation IDs */
#define ID_DEPTH 108
#define ID_PROTOCOL 109
#define ID_KEYBOARD 110
#define ID_NUMLOCK 111
#define ID_CLIPBOARD 112
#define ID_AUDIO 113
#define ID_CONNECT IDOK
#define ID_CANCEL IDCANCEL
#define ID_WINDOW_MODE 116
#define ID_SHOW_OPTIONS 117
#define ID_TABS 118
#define ID_SHOW_BAR 119
#define ID_EXPERIENCE 120
#define ID_SEND_MOTION 121
#define ID_BITMAP_CACHE 122
#define ID_LANGUAGE 123
#define ID_DIAGNOSTICS 124
#define ID_DIAG_LOG 126
#define ID_DIAG_CLEAR 127
#define ID_DIAG_COPY 128
#define ID_COMPRESSION 129
#define ID_ADMIN 130
#define ID_RETRY 131
#define ID_TLS 132
#define ID_HOSTNAME 133
#define ID_CODEPAGE 134
#define ID_SHELL 135
#define ID_DIRECTORY 136
#define ID_TITLE 137

typedef struct win32_gui_state
{
	RDESKTOP_CONFIG *config;
	RD_BOOL done;
	int result;
	RD_BOOL moving;
	HBITMAP move_snapshot;
	HWND move_parking_window;
	int move_snapshot_width;
	int move_snapshot_height;
	RD_BOOL expanded;
	RD_BOOL applying_language;
	char locale[16];
}
WIN32_GUI_STATE;

enum { GUI_EXIT_USER_CLOSE, GUI_EXIT_CONNECT, GUI_EXIT_LANGUAGE_RELOAD };

#define TR(k) win32_i18n_get(k)
#define TRW(k) win32_i18n_get_w(k)
static WIN32_DIAGNOSTIC_LOG gui_diagnostic_log;
static void gui_diagnostic_sink(void *context,const char *message){win32_diagnostic_log_append((WIN32_DIAGNOSTIC_LOG*)context,message);}
void win32_gui_log_event(const char *message){win32_diagnostics_set_sink(gui_diagnostic_sink,&gui_diagnostic_log);if(win32_diagnostics_input_enabled())win32_diagnostics_emit(message);}

static void gui_utf8_to_wide(const char *text,WCHAR *wide,int count){if(!MultiByteToWideChar(CP_UTF8,0,text?text:"",-1,wide,count))wide[0]=0;}
static void gui_wide_to_utf8(const WCHAR *wide,char *text,size_t size){if(!WideCharToMultiByte(CP_UTF8,0,wide?wide:L"",-1,text,(int)size,NULL,NULL))text[0]=0;text[size-1]=0;}
static void gui_set_text(HWND window,const char *text){WCHAR wide[512];gui_utf8_to_wide(text,wide,512);SetWindowTextW(window,wide);}
static void gui_set_item_text(HWND window,int id,const char *text){gui_set_text(GetDlgItem(window,id),text);}
static void gui_refresh_diagnostic_log(HWND window){static WCHAR wide[WIN32_DIAGNOSTIC_LOG_CAPACITY];if(!MultiByteToWideChar(CP_UTF8,0,win32_diagnostic_log_text(&gui_diagnostic_log),-1,wide,WIN32_DIAGNOSTIC_LOG_CAPACITY))wide[0]=0;SetDlgItemTextW(window,ID_DIAG_LOG,wide);SendDlgItemMessageW(window,ID_DIAG_LOG,EM_SETSEL,(WPARAM)-1,(LPARAM)-1);SendDlgItemMessageW(window,ID_DIAG_LOG,EM_SCROLLCARET,0,0);}

static HWND
gui_control(HWND parent, const WCHAR *class_name, const char *text, DWORD style,
	    int x, int y, int width, int height, int id)
{
	WCHAR wide[512]; gui_utf8_to_wide(text,wide,512);
	return CreateWindowExW(0, class_name, wide, WS_CHILD | WS_VISIBLE | style,
			       x, y, width, height, parent, (HMENU) (INT_PTR) id,
			       GetModuleHandleW(NULL), NULL);
}

static void
gui_set_font(HWND window, HFONT font)
{
	HWND child;
	SendMessage(window, WM_SETFONT, (WPARAM) font, TRUE);
	for (child = GetWindow(window, GW_CHILD); child != NULL;
	     child = GetWindow(child, GW_HWNDNEXT))
		SendMessage(child, WM_SETFONT, (WPARAM) font, TRUE);
}

static void
gui_get_text(HWND window, int id, char *destination, size_t size)
{
	WCHAR wide[512]; GetDlgItemTextW(window,id,wide,512); gui_wide_to_utf8(wide,destination,size);
}

static void
gui_combo_add(HWND combo, const char *text)
{
	WCHAR wide[512];gui_utf8_to_wide(text,wide,512);SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)wide);
}

static void
gui_combo_caret_end(HWND combo)
{
	int length = GetWindowTextLengthW(combo);
	SendMessage(combo, CB_SETEDITSEL, 0, MAKELPARAM(length, length));
	{
		HWND edit = GetWindow(combo, GW_CHILD);
		if (edit != NULL)
			SendMessage(edit, EM_SETSEL, length, length);
	}
}

static void
gui_apply_config(HWND window, const RDESKTOP_CONFIG *config)
{
	char value[64]; int keysel=1;
	gui_set_item_text(window,ID_SERVER,config->server); gui_set_item_text(window,ID_USERNAME,config->username);
	gui_set_item_text(window,ID_PASSWORD,config->password); gui_set_item_text(window,ID_DOMAIN,config->domain);
	if(config->port){sprintf(value,"%u",config->port);gui_set_item_text(window,ID_PORT,value);}else gui_set_item_text(window,ID_PORT,"");
	sprintf(value,"%ux%u",config->requested_width,config->requested_height);gui_set_item_text(window,ID_RESOLUTION,value);
	SendDlgItemMessage(window,ID_WINDOW_MODE,CB_SETCURSEL,config->window_mode==RDESKTOP_WINDOW_FULLSCREEN?2:(config->window_mode==RDESKTOP_WINDOW_FILL_AVAILABLE?1:0),0);
	SendDlgItemMessage(window,ID_DEPTH,CB_SETCURSEL,config->requested_colour_depth==-1?0:config->requested_colour_depth==8?1:config->requested_colour_depth==15?2:config->requested_colour_depth==16?3:config->requested_colour_depth==24?4:5,0);
	SendDlgItemMessage(window,ID_PROTOCOL,CB_SETCURSEL,config->rdp_version==RDP_V4?0:1,0);
	if(!_stricmp(config->keymap,"pt-br"))keysel=0;else if(!_stricmp(config->keymap,"en-us-intl"))keysel=2;else if(!_stricmp(config->keymap,"en-gb"))keysel=3;else if(!_stricmp(config->keymap,"pt"))keysel=4;
	SendDlgItemMessage(window,ID_KEYBOARD,CB_SETCURSEL,keysel,0);
	CheckDlgButton(window,ID_NUMLOCK,config->numlock_sync?BST_CHECKED:BST_UNCHECKED);CheckDlgButton(window,ID_CLIPBOARD,config->clipboard_enabled?BST_CHECKED:BST_UNCHECKED);
	SendDlgItemMessage(window,ID_AUDIO,CB_SETCURSEL,config->audio_mode==RDESKTOP_AUDIO_LOCAL?1:0,0);
	CheckDlgButton(window,ID_SHOW_BAR,config->show_connection_bar?BST_CHECKED:BST_UNCHECKED);
	SendDlgItemMessage(window,ID_EXPERIENCE,CB_SETCURSEL,(config->performance_flags&PERF_DISABLE_THEMING)?0:((config->performance_flags&PERF_DISABLE_WALLPAPER)?1:2),0);
	CheckDlgButton(window,ID_SEND_MOTION,config->send_motion?BST_CHECKED:BST_UNCHECKED);CheckDlgButton(window,ID_BITMAP_CACHE,config->bitmap_cache?BST_CHECKED:BST_UNCHECKED);CheckDlgButton(window,ID_COMPRESSION,config->compression?BST_CHECKED:BST_UNCHECKED);CheckDlgButton(window,ID_ADMIN,config->console_session?BST_CHECKED:BST_UNCHECKED);CheckDlgButton(window,ID_RETRY,config->retry_on_network_error?BST_CHECKED:BST_UNCHECKED);
	gui_set_item_text(window,ID_TLS,config->tls_version);gui_set_item_text(window,ID_HOSTNAME,config->hostname);gui_set_item_text(window,ID_CODEPAGE,config->codepage);gui_set_item_text(window,ID_SHELL,config->shell);gui_set_item_text(window,ID_DIRECTORY,config->directory);gui_set_item_text(window,ID_TITLE,config->title);
}

static const int page_ids[6][24] = {
	{104,105,204,205,0},
	{106,108,116,119,206,208,216,0},
	{110,111,112,113,210,213,0},
	{120,121,122,220,0},
	{102,109,123,129,130,131,132,133,134,135,136,137,240,241,242,243,244,245,246,247,248,0},
	{124,126,127,128,225,0}
};

static void gui_show_page(HWND w,int page){int p,i;for(p=0;p<6;p++)for(i=0;i<24&&page_ids[p][i];i++)ShowWindow(GetDlgItem(w,page_ids[p][i]),p==page?SW_SHOW:SW_HIDE);}
static void gui_set_expanded(HWND w,WIN32_GUI_STATE*s,RD_BOOL expanded){RECT r;MONITORINFO mi;HMONITOR m;int ww=680,wh=expanded?650:220;s->expanded=expanded;GetWindowRect(w,&r);m=MonitorFromWindow(w,MONITOR_DEFAULTTONEAREST);mi.cbSize=sizeof(mi);GetMonitorInfoA(m,&mi);SetWindowPos(w,NULL,mi.rcWork.left+((mi.rcWork.right-mi.rcWork.left)-ww)/2,mi.rcWork.top+((mi.rcWork.bottom-mi.rcWork.top)-wh)/2,ww,wh,SWP_NOZORDER|SWP_NOACTIVATE);ShowWindow(GetDlgItem(w,ID_TABS),expanded?SW_SHOW:SW_HIDE);SetWindowTextW(GetDlgItem(w,ID_SHOW_OPTIONS),TRW(expanded?"button.hide_options":"button.show_options"));if(expanded)gui_show_page(w,(int)SendDlgItemMessage(w,ID_TABS,TCM_GETCURSEL,0,0));else gui_show_page(w,-1);win32_gui_prefs_save_global(s->locale,expanded);}

static void gui_relabel_combo(HWND window,int id,const char **keys,int count)
{
	HWND combo=GetDlgItem(window,id);int selected=(int)SendMessage(combo,CB_GETCURSEL,0,0),i;
	SendMessage(combo,CB_RESETCONTENT,0,0);
	for(i=0;i<count;i++)gui_combo_add(combo,TR(keys[i]));
	SendMessage(combo,CB_SETCURSEL,selected<0?0:selected,0);
}

static void gui_apply_language(HWND window,WIN32_GUI_STATE *state)
{
	static const struct {int id;const char*key;} text[]={
		{300,"app.subtitle"},{301,"label.server"},{302,"label.username"},{204,"label.password"},{205,"label.domain"},{206,"label.resolution"},{216,"label.window_mode"},{208,"label.depth"},{210,"label.keyboard"},{213,"label.audio"},{220,"label.experience"},{240,"label.port"},{241,"label.protocol"},{242,"label.language"},{243,"label.tls"},{244,"label.hostname"},{245,"label.codepage"},{246,"label.shell"},{247,"label.directory"},{248,"label.title"},{225,"label.diagnostic_log"},
		{ID_CONNECT,"button.connect"},{ID_CANCEL,"button.cancel"},{ID_NUMLOCK,"check.numlock"},{ID_CLIPBOARD,"check.clipboard"},{ID_SHOW_BAR,"check.connection_bar"},{ID_DIAGNOSTICS,"check.input_diagnostics"},{ID_SEND_MOTION,"check.motion"},{ID_BITMAP_CACHE,"check.bitmap_cache"},{ID_COMPRESSION,"check.compression"},{ID_ADMIN,"check.admin"},{ID_RETRY,"check.retry"},{ID_DIAG_CLEAR,"button.clear"},{ID_DIAG_COPY,"button.copy"}};
	static const char *tabs[]={"tab.general","tab.display","tab.local","tab.experience","tab.advanced","tab.diagnostics"};
	static const char *modes[]={"mode.client","mode.fill","mode.fullscreen"},*audio[]={"audio.off","audio.local"},*experience[]={"experience.modem","experience.broadband","experience.lan"},*keyboards[]={"keyboard.pt-br","keyboard.en-us","keyboard.en-us-intl","keyboard.en-gb","keyboard.pt"},*locales[]={"locale.en-US","locale.en-GB","locale.pt-BR","locale.pt-PT"};
	HWND tab=GetDlgItem(window,ID_TABS);TCITEMW item;int i,page;
	page=state->expanded?TabCtrl_GetCurSel(tab):-1;
	if(state->expanded&&(page<0||page>=6))page=0;
	state->applying_language=True;
	SetWindowTextW(window,TRW("app.title"));
	for(i=0;i<(int)(sizeof(text)/sizeof(text[0]));i++)SetWindowTextW(GetDlgItem(window,text[i].id),TRW(text[i].key));
	SetWindowTextW(GetDlgItem(window,ID_SHOW_OPTIONS),TRW(state->expanded?"button.hide_options":"button.show_options"));
	memset(&item,0,sizeof(item));item.mask=TCIF_TEXT;for(i=0;i<6;i++){item.pszText=(WCHAR*)TRW(tabs[i]);SendMessageW(tab,TCM_SETITEMW,i,(LPARAM)&item);}
	gui_relabel_combo(window,ID_WINDOW_MODE,modes,3);gui_relabel_combo(window,ID_AUDIO,audio,2);gui_relabel_combo(window,ID_EXPERIENCE,experience,3);gui_relabel_combo(window,ID_KEYBOARD,keyboards,5);gui_relabel_combo(window,ID_LANGUAGE,locales,4);
	if(page>=0)TabCtrl_SetCurSel(tab,page);
	state->applying_language=False;
	gui_show_page(window,page);
	RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
}

static RD_BOOL
gui_read_config(HWND window, RDESKTOP_CONFIG *config, char *error, size_t error_size)
{
	char value[256];
	char server[256];
	char *end;
	long port;
	int selection;
	RECT window_rect;

	gui_get_text(window, ID_SERVER, server, sizeof(server));
	gui_get_text(window, ID_PORT, value, sizeof(value));
	port = strtol(value, &end, 10);
	if (value[0] == '\0')
		config->port = 0;
	else if (*end != '\0' || port < 1 || port > 65535)
	{
		STRNCPY(error, TR("error.port"), error_size);
		return False;
	}
	else
		config->port = (uint16) port;
	STRNCPY(config->server, server, sizeof(config->server));
	gui_get_text(window, ID_USERNAME, config->username, sizeof(config->username));
	gui_get_text(window, ID_PASSWORD, config->password, sizeof(config->password));
	config->password_present = config->password[0] != '\0';
	gui_get_text(window, ID_DOMAIN, config->domain, sizeof(config->domain));

	gui_get_text(window, ID_RESOLUTION, value, sizeof(value));
	selection = (int) SendDlgItemMessage(window, ID_WINDOW_MODE, CB_GETCURSEL, 0, 0);
	config->window_mode = selection == 2 ? RDESKTOP_WINDOW_FULLSCREEN :
		(selection == 1 ? RDESKTOP_WINDOW_FILL_AVAILABLE : RDESKTOP_WINDOW_CLIENT_SIZE);
	config->fullscreen = config->window_mode == RDESKTOP_WINDOW_FULLSCREEN;
	if (config->fullscreen)
		config->window_size_type = Fullscreen;
	else if (rdesktop_config_parse_geometry(config, value) != 0)
	{
		STRNCPY(error, TR("error.resolution"), error_size);
		return False;
	}

	selection = (int) SendDlgItemMessage(window, ID_DEPTH, CB_GETCURSEL, 0, 0);
	config->requested_colour_depth = selection == 0 ? -1 :
		(selection == 1 ? 8 : selection == 2 ? 15 : selection == 3 ? 16 :
		 selection == 4 ? 24 : 32);
	config->rdp_version = SendDlgItemMessage(window, ID_PROTOCOL, CB_GETCURSEL, 0, 0) == 0 ?
		RDP_V4 : RDP_V5;
	selection=(int)SendDlgItemMessage(window,ID_KEYBOARD,CB_GETCURSEL,0,0);
	STRNCPY(config->keymap,selection==0?"pt-br":selection==2?"en-us-intl":selection==3?"en-gb":selection==4?"pt":"en-us",sizeof(config->keymap));
	config->numlock_sync = SendDlgItemMessage(window, ID_NUMLOCK, BM_GETCHECK, 0, 0) == BST_CHECKED;
	config->clipboard_enabled = SendDlgItemMessage(window, ID_CLIPBOARD, BM_GETCHECK, 0, 0) == BST_CHECKED;
	selection = (int) SendDlgItemMessage(window, ID_AUDIO, CB_GETCURSEL, 0, 0);
	config->audio_mode = selection == 1 ? RDESKTOP_AUDIO_LOCAL : RDESKTOP_AUDIO_OFF;
	config->show_connection_bar=IsDlgButtonChecked(window,ID_SHOW_BAR)==BST_CHECKED;
	selection=(int)SendDlgItemMessage(window,ID_EXPERIENCE,CB_GETCURSEL,0,0);
	config->performance_flags=selection==0?(PERF_DISABLE_CURSOR_SHADOW|PERF_DISABLE_WALLPAPER|PERF_DISABLE_FULLWINDOWDRAG|PERF_DISABLE_MENUANIMATIONS|PERF_DISABLE_THEMING):selection==1?(PERF_DISABLE_WALLPAPER|PERF_ENABLE_FONT_SMOOTHING):PERF_ENABLE_FONT_SMOOTHING;
	config->send_motion=IsDlgButtonChecked(window,ID_SEND_MOTION)==BST_CHECKED;config->bitmap_cache=IsDlgButtonChecked(window,ID_BITMAP_CACHE)==BST_CHECKED;
	config->compression=IsDlgButtonChecked(window,ID_COMPRESSION)==BST_CHECKED;config->console_session=IsDlgButtonChecked(window,ID_ADMIN)==BST_CHECKED;config->retry_on_network_error=IsDlgButtonChecked(window,ID_RETRY)==BST_CHECKED;
	gui_get_text(window,ID_TLS,config->tls_version,sizeof(config->tls_version));gui_get_text(window,ID_HOSTNAME,config->hostname,sizeof(config->hostname));gui_get_text(window,ID_CODEPAGE,config->codepage,sizeof(config->codepage));gui_get_text(window,ID_SHELL,config->shell,sizeof(config->shell));gui_get_text(window,ID_DIRECTORY,config->directory,sizeof(config->directory));gui_get_text(window,ID_TITLE,config->title,sizeof(config->title));
	/* Preserve the GUI's monitor as the placement hint for the session window. */
	if (GetWindowRect(window, &window_rect))
	{
		config->window_x = window_rect.left + (window_rect.right - window_rect.left) / 2;
		config->window_y = window_rect.top + (window_rect.bottom - window_rect.top) / 2;
		config->window_position = 1;
	}
	if (config->audio_mode == RDESKTOP_AUDIO_LOCAL)
	{
		STRNCPY(config->audio_driver, "winmm", sizeof(config->audio_driver));
	}
	else
		config->audio_driver[0] = '\0';

	return rdesktop_config_validate_message(config, error, error_size) == 0;
}

static void
gui_begin_move_snapshot(HWND window, WIN32_GUI_STATE *state)
{
	RECT client;
	HDC window_dc;
	HDC snapshot_dc;
	HBITMAP previous;
	HWND child;
	HWND next;

	if (!GetClientRect(window, &client))
		return;
	state->move_snapshot_width = client.right;
	state->move_snapshot_height = client.bottom;
	window_dc = GetDC(window);
	snapshot_dc = CreateCompatibleDC(window_dc);
	state->move_snapshot = CreateCompatibleBitmap(window_dc,
		state->move_snapshot_width, state->move_snapshot_height);
	if (snapshot_dc == NULL || state->move_snapshot == NULL)
	{
		if (snapshot_dc != NULL)
			DeleteDC(snapshot_dc);
		if (window_dc != NULL)
			ReleaseDC(window, window_dc);
		state->move_snapshot = NULL;
		return;
	}
	previous = (HBITMAP) SelectObject(snapshot_dc, state->move_snapshot);
	PrintWindow(window, snapshot_dc, PW_CLIENTONLY);
	SelectObject(snapshot_dc, previous);
	DeleteDC(snapshot_dc);
	ReleaseDC(window, window_dc);
	state->move_parking_window = CreateWindowExW(0, L"STATIC", L"",
		WS_POPUP, 0, 0, 0, 0, NULL, NULL, GetModuleHandleW(NULL), NULL);
	if (state->move_parking_window == NULL)
		return;
	for (child = GetWindow(window, GW_CHILD); child != NULL; child = next)
	{
		next = GetWindow(child, GW_HWNDNEXT);
		SetParent(child, state->move_parking_window);
	}
	InvalidateRect(window, NULL, FALSE);
	UpdateWindow(window);
}

static void
gui_restore_after_move(HWND window, WIN32_GUI_STATE *state)
{
	HWND child;
	HWND next;

	for (child = state->move_parking_window != NULL ?
	     GetWindow(state->move_parking_window, GW_CHILD) : NULL;
	     child != NULL; child = next)
	{
		next = GetWindow(child, GW_HWNDNEXT);
		SetParent(child, window);
	}
	if (state->move_parking_window != NULL)
		DestroyWindow(state->move_parking_window);
	state->move_parking_window = NULL;
	if (state->move_snapshot != NULL)
	{
		DeleteObject(state->move_snapshot);
		state->move_snapshot = NULL;
	}
	RedrawWindow(window, NULL, NULL,
		RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

static LRESULT CALLBACK
gui_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	WIN32_GUI_STATE *state = (WIN32_GUI_STATE *) GetWindowLongPtr(window, GWLP_USERDATA);

	if (message == WM_NCCREATE)
	{
		CREATESTRUCTW *create = (CREATESTRUCTW *) lparam;
		SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR) create->lpCreateParams);
		return TRUE;
	}
	if (state == NULL)
		return DefWindowProcW(window, message, wparam, lparam);
	if (message == DM_GETDEFID)
		return MAKELRESULT(ID_CONNECT, DC_HASDEFID);
	if (message == DM_SETDEFID)
		return TRUE;
	if (message == WM_ENTERSIZEMOVE)
	{
		state->moving = True;
		gui_begin_move_snapshot(window, state);
		return 0;
	}
	if (message == WM_ERASEBKGND && state->moving)
		return TRUE;
	if (message == WM_PAINT && state->moving)
	{
		PAINTSTRUCT paint;
		HDC destination = BeginPaint(window, &paint);
		if (state->move_snapshot != NULL)
		{
			HDC source = CreateCompatibleDC(destination);
			HBITMAP previous = (HBITMAP) SelectObject(source, state->move_snapshot);
			BitBlt(destination, 0, 0, state->move_snapshot_width,
				state->move_snapshot_height, source, 0, 0, SRCCOPY);
			SelectObject(source, previous);
			DeleteDC(source);
		}
		EndPaint(window, &paint);
		return 0;
	}
	if (message == WM_EXITSIZEMOVE)
	{
		state->moving = False;
		gui_restore_after_move(window, state);
		return 0;
	}
	if (message == WM_CREATE)
		return 0;
	if (message == WM_COMMAND && LOWORD(wparam) == ID_CONNECT)
	{
		char error[256];
		if (gui_read_config(window, state->config, error, sizeof(error)))
		{
			state->result = GUI_EXIT_CONNECT;
			state->done = True;
			DestroyWindow(window);
		}
		else
			{WCHAR wide[256];gui_utf8_to_wide(error,wide,256);MessageBoxW(window,wide,TRW("app.title"),MB_OK|MB_ICONWARNING);}
		return 0;
	}
	if(message==WM_COMMAND&&LOWORD(wparam)==ID_SHOW_OPTIONS){gui_set_expanded(window,state,!state->expanded);return 0;}
	if(message==WM_COMMAND&&LOWORD(wparam)==ID_DIAGNOSTICS){win32_diagnostics_set_input_enabled(IsDlgButtonChecked(window,ID_DIAGNOSTICS)==BST_CHECKED);return 0;}
	if(message==WM_COMMAND&&LOWORD(wparam)==ID_DIAG_CLEAR){win32_diagnostic_log_clear(&gui_diagnostic_log);SetDlgItemTextW(window,ID_DIAG_LOG,L"");return 0;}
	if(message==WM_COMMAND&&LOWORD(wparam)==ID_DIAG_COPY){SendDlgItemMessage(window,ID_DIAG_LOG,EM_SETSEL,0,-1);SendDlgItemMessage(window,ID_DIAG_LOG,WM_COPY,0,0);return 0;}
	if(message==WM_COMMAND&&LOWORD(wparam)==ID_LANGUAGE&&HIWORD(wparam)==CBN_SELCHANGE){static const char*locales[]={"en-US","en-GB","pt-BR","pt-PT"};int n;if(state->applying_language)return 0;n=(int)SendDlgItemMessage(window,ID_LANGUAGE,CB_GETCURSEL,0,0);if(n<0||n>3)n=0;STRNCPY(state->locale,locales[n],sizeof(state->locale));win32_i18n_load(state->locale);win32_gui_prefs_save_global(state->locale,state->expanded);gui_apply_language(window,state);return 0;}
	if(message==WM_COMMAND&&LOWORD(wparam)==ID_SERVER&&HIWORD(wparam)==CBN_SELCHANGE)
	{
		char server[256];WCHAR wide[256];int index=(int)SendDlgItemMessage(window,ID_SERVER,CB_GETCURSEL,0,0);
		SendDlgItemMessageW(window,ID_SERVER,CB_GETLBTEXT,index,(LPARAM)wide);gui_wide_to_utf8(wide,server,sizeof(server));
		if(win32_gui_prefs_load_profile(server,state->config))gui_apply_config(window,state->config);
		return 0;
	}
	if ((message == WM_COMMAND && LOWORD(wparam) == ID_CANCEL) || message == WM_CLOSE)
	{
		state->result = GUI_EXIT_USER_CLOSE;
		state->done = True;
		DestroyWindow(window);
		return 0;
	}
	if(message==WM_NOTIFY&&((NMHDR*)lparam)->idFrom==ID_TABS&&((NMHDR*)lparam)->code==TCN_SELCHANGE){if(!state->applying_language)gui_show_page(window,TabCtrl_GetCurSel(GetDlgItem(window,ID_TABS)));return 0;}
	return DefWindowProcW(window, message, wparam, lparam);
}

void win32_gui_show_connection_failure(const RDESKTOP_CONFIG *config,int result)
{
	const WCHAR *reason=win32_i18n_get_w("connect.reason.protocol");
	WCHAR server[256],message[768];
	if(result==EX_NOHOST)
		reason=win32_i18n_get_w("connect.reason.host");
	else if(result==EX_UNAVAILABLE||result==EX_TEMPFAIL||result==EX_IOERR)
		reason=win32_i18n_get_w("connect.reason.network");
	else if(result==EXRD_DENIED||result==EXRD_DENIED_FIPS||result==EXRD_INSUFFICIENT_PRIVILEGES||result==EX_NOPERM)
		reason=win32_i18n_get_w("connect.reason.access");
	else if(result>=EXRD_LIC_INTERNAL&&result<=EXRD_LIC_NOREMOTE)
		reason=win32_i18n_get_w("connect.reason.license");
	else if(result==EX_OSERR)
		reason=win32_i18n_get_w("connect.reason.local");
	gui_utf8_to_wide(config->server,server,256);
	_snwprintf(message,768,win32_i18n_get_w("connect.failure.body"),server,reason);
	message[767]=0;
	MessageBoxW(NULL,message,win32_i18n_get_w("connect.failure.title"),MB_OK|MB_ICONERROR);
}

static HANDLE connecting_thread;
static HANDLE connecting_ready;
static HWND connecting_window;
static HWND connecting_progress;
static WCHAR connecting_server[256];
static volatile LONG connecting_cancelled;
#define ID_CONNECT_CANCEL 150

static LRESULT CALLBACK
connecting_window_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam)
{
	static int progress_position;
	if(message==WM_TIMER&&wparam==1)
	{
		HWND session_window=FindWindowA("rdesktopWin32",NULL);
		DWORD session_process=0;
		if(session_window)
			GetWindowThreadProcessId(session_window,&session_process);
		if(session_window&&session_process==GetCurrentProcessId()&&IsWindowVisible(session_window))
		{
			PostMessageW(window,WM_CLOSE,0,0);
			return 0;
		}
		progress_position=(progress_position+2)%101;
		if(connecting_progress)SendMessageW(connecting_progress,PBM_SETPOS,progress_position,0);
		return 0;
	}
	if(message==WM_COMMAND&&LOWORD(wparam)==ID_CONNECT_CANCEL)
	{
		InterlockedExchange(&connecting_cancelled,1);
		EnableWindow((HWND)lparam,FALSE);
		SetWindowTextW((HWND)lparam,win32_i18n_get_w("connect.progress.cancelling"));
		rdesktop_frontend_cancel_session();
		return 0;
	}
	if(message==WM_CLOSE){DestroyWindow(window);return 0;}
	if(message==WM_DESTROY){KillTimer(window,1);PostQuitMessage(0);return 0;}
	return DefWindowProcW(window,message,wparam,lparam);
}

static DWORD WINAPI
connecting_thread_proc(LPVOID unused)
{
	WNDCLASSW window_class;
	HWND window,progress;
	WCHAR server[256],message[512];
	MSG msg;
	RECT r;MONITORINFO mi;HMONITOR monitor;
	UNUSED(unused);
	memset(&window_class,0,sizeof(window_class));
	window_class.lpfnWndProc=connecting_window_proc;
	window_class.hInstance=GetModuleHandleW(NULL);
	window_class.hCursor=LoadCursor(NULL,IDC_ARROW);
	window_class.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
	window_class.lpszClassName=L"RdesktopConnectingWindow";
	RegisterClassW(&window_class);
	wcsncpy(server,connecting_server,255);server[255]=0;
	_snwprintf(message,512,win32_i18n_get_w("connect.progress.body"),server);
	message[511]=0;
	window=CreateWindowExW(WS_EX_DLGMODALFRAME,L"RdesktopConnectingWindow",
		win32_i18n_get_w("connect.progress.title"),WS_OVERLAPPED|WS_CAPTION,
		CW_USEDEFAULT,CW_USEDEFAULT,460,150,NULL,NULL,GetModuleHandleW(NULL),NULL);
	connecting_window=window;
	if(!window){SetEvent(connecting_ready);return 1;}
	CreateWindowExW(0,L"STATIC",message,WS_CHILD|WS_VISIBLE|SS_LEFT,
		24,22,405,42,window,NULL,GetModuleHandleW(NULL),NULL);
	progress=CreateWindowExW(0,PROGRESS_CLASSW,L"",WS_CHILD|WS_VISIBLE|PBS_SMOOTH,
		24,72,300,18,window,NULL,GetModuleHandleW(NULL),NULL);
	connecting_progress=progress;
	SendMessageW(progress,PBM_SETRANGE,0,MAKELPARAM(0,100));
	CreateWindowExW(0,L"BUTTON",win32_i18n_get_w("button.cancel"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
		338,66,92,28,window,(HMENU)ID_CONNECT_CANCEL,GetModuleHandleW(NULL),NULL);
	SetTimer(window,1,60,NULL);
	monitor=MonitorFromWindow(window,MONITOR_DEFAULTTOPRIMARY);mi.cbSize=sizeof(mi);
	if(GetMonitorInfoW(monitor,&mi)&&GetWindowRect(window,&r))
		SetWindowPos(window,HWND_TOP,
			mi.rcWork.left+((mi.rcWork.right-mi.rcWork.left)-(r.right-r.left))/2,
			mi.rcWork.top+((mi.rcWork.bottom-mi.rcWork.top)-(r.bottom-r.top))/2,
			0,0,SWP_NOSIZE|SWP_SHOWWINDOW);
	ShowWindow(window,SW_SHOW);UpdateWindow(window);
	SetEvent(connecting_ready);
	while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
	connecting_window=NULL;
	return 0;
}

HWND win32_gui_show_connecting(const RDESKTOP_CONFIG *config)
{
	InterlockedExchange(&connecting_cancelled,0);
	gui_utf8_to_wide(config->server,connecting_server,256);
	connecting_ready=CreateEventW(NULL,TRUE,FALSE,NULL);
	if(!connecting_ready)return NULL;
	connecting_thread=CreateThread(NULL,0,connecting_thread_proc,NULL,0,NULL);
	if(!connecting_thread){CloseHandle(connecting_ready);connecting_ready=NULL;return NULL;}
	WaitForSingleObject(connecting_ready,3000);
	CloseHandle(connecting_ready);connecting_ready=NULL;
	return connecting_window;
}

void win32_gui_close_connecting(HWND window)
{
	UNUSED(window);
	if(connecting_window)PostMessageW(connecting_window,WM_CLOSE,0,0);
	if(connecting_thread){WaitForSingleObject(connecting_thread,3000);CloseHandle(connecting_thread);connecting_thread=NULL;}
	connecting_window=NULL;
	connecting_progress=NULL;
}

RD_BOOL win32_gui_connect_cancelled(void)
{
	return InterlockedCompareExchange(&connecting_cancelled,0,0)?True:False;
}

static int
win32_config_dialog_once(RDESKTOP_CONFIG *config, char *error, size_t error_size)
{
	WNDCLASSW window_class;
	WIN32_GUI_STATE state;
	HWND window, combo;
	MSG message;
	HFONT font = (HFONT) GetStockObject(DEFAULT_GUI_FONT);
	char number[32];
	char servers[WIN32_GUI_MRU_COUNT][256];
	int i;
	INITCOMMONCONTROLSEX controls;
	/* A closed session window posts WM_QUIT.  It belongs to the completed
	   session loop and must not terminate a newly-created configuration UI. */
	while (PeekMessage(&message, NULL, WM_QUIT, WM_QUIT, PM_REMOVE))
		;

	memset(&window_class, 0, sizeof(window_class));
	window_class.lpfnWndProc = gui_window_proc;
	window_class.hInstance = GetModuleHandleW(NULL);
	window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	window_class.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);
	window_class.lpszClassName = GUI_CLASS;
	RegisterClassW(&window_class);
	memset(&state, 0, sizeof(state));
	state.result = GUI_EXIT_USER_CLOSE;
	win32_gui_prefs_load_global(state.locale,sizeof(state.locale),&state.expanded);
	win32_i18n_load(state.locale);
	STRNCPY(state.locale,win32_i18n_locale(),sizeof(state.locale));
	controls.dwSize=sizeof(controls);controls.dwICC=ICC_TAB_CLASSES;InitCommonControlsEx(&controls);
	state.config = config;
	window = CreateWindowExW(WS_EX_CONTROLPARENT, GUI_CLASS, TRW("app.title"),
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 680, state.expanded?650:220, NULL, NULL,
		GetModuleHandleW(NULL), &state);
	if (window == NULL)
	{
		STRNCPY(error, "Could not create the configuration window.", error_size);
		return GUI_EXIT_USER_CLOSE;
	}
	SetWindowTextW(window, TRW("app.title"));

#define LAB(id,key,x,y) gui_control(window,L"STATIC",TR(key),SS_LEFT,x,y,145,20,id)
	gui_control(window,L"STATIC",TR("app.subtitle"),SS_LEFT,20,16,400,24,300);
	LAB(301,"label.server",20,58); combo=gui_control(window,L"COMBOBOX","",CBS_DROPDOWN|WS_TABSTOP|WS_VSCROLL,170,54,300,140,ID_SERVER);
	win32_gui_prefs_load_servers(servers); for(i=0;i<WIN32_GUI_MRU_COUNT;i++)if(servers[i][0])gui_combo_add(combo,servers[i]);
	if(!config->server[0]&&servers[0][0])win32_gui_prefs_load_profile(servers[0],config);
	if(config->server[0])gui_set_text(combo,config->server);else if(servers[0][0])gui_set_text(combo,servers[0]);
	gui_combo_caret_end(combo);
	LAB(302,"label.username",20,94);gui_control(window,L"EDIT",config->username,WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,170,90,300,24,ID_USERNAME);
	gui_control(window,L"BUTTON",TR("button.show_options"),BS_PUSHBUTTON|WS_TABSTOP,20,130,140,27,ID_SHOW_OPTIONS);
	combo=gui_control(window,WC_TABCONTROLW,"",WS_TABSTOP,15,170,640,390,ID_TABS);{TCITEMW ti;const char*keys[]={"tab.general","tab.display","tab.local","tab.experience","tab.advanced","tab.diagnostics"};memset(&ti,0,sizeof(ti));ti.mask=TCIF_TEXT;for(i=0;i<6;i++){ti.pszText=(WCHAR*)TRW(keys[i]);SendMessageW(combo,TCM_INSERTITEMW,i,(LPARAM)&ti);}}
	LAB(204,"label.password",45,220);gui_control(window,L"EDIT","",WS_BORDER|WS_TABSTOP|ES_PASSWORD|ES_AUTOHSCROLL,200,216,300,24,ID_PASSWORD);
	LAB(205,"label.domain",45,258);gui_control(window,L"EDIT",config->domain,WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,200,254,300,24,ID_DOMAIN);
	LAB(206,"label.resolution",45,220);combo=gui_control(window,L"COMBOBOX","",CBS_DROPDOWN|WS_TABSTOP|WS_VSCROLL,200,216,250,160,ID_RESOLUTION);
	gui_combo_add(combo, "800x600"); gui_combo_add(combo, "1024x768"); gui_combo_add(combo, "1280x1024"); gui_combo_add(combo, "1600x900");
	sprintf(number, "%ux%u", config->requested_width, config->requested_height); gui_set_text(combo, number);
	gui_combo_caret_end(combo);
	LAB(216,"label.window_mode",45,258);combo=gui_control(window,L"COMBOBOX","",CBS_DROPDOWNLIST|WS_TABSTOP,200,254,250,110,ID_WINDOW_MODE);gui_combo_add(combo,TR("mode.client"));gui_combo_add(combo,TR("mode.fill"));gui_combo_add(combo,TR("mode.fullscreen"));
	LAB(208,"label.depth",45,296);combo=gui_control(window,L"COMBOBOX","",CBS_DROPDOWNLIST|WS_TABSTOP,200,292,250,150,ID_DEPTH);gui_combo_add(combo,TR("depth.default"));gui_combo_add(combo,"8 bpp");gui_combo_add(combo,"15 bpp");gui_combo_add(combo,"16 bpp");gui_combo_add(combo,"24 bpp");gui_combo_add(combo,"32 bpp");
	gui_control(window,L"BUTTON",TR("check.connection_bar"),BS_AUTOCHECKBOX|WS_TABSTOP,200,334,320,22,ID_SHOW_BAR);
	LAB(210,"label.keyboard",45,220);combo=gui_control(window,L"COMBOBOX","",CBS_DROPDOWNLIST|WS_TABSTOP,200,216,300,150,ID_KEYBOARD);gui_combo_add(combo,TR("keyboard.pt-br"));gui_combo_add(combo,TR("keyboard.en-us"));gui_combo_add(combo,TR("keyboard.en-us-intl"));gui_combo_add(combo,TR("keyboard.en-gb"));gui_combo_add(combo,TR("keyboard.pt"));
	gui_control(window,L"BUTTON",TR("check.numlock"),BS_AUTOCHECKBOX|WS_TABSTOP,200,258,300,22,ID_NUMLOCK);gui_control(window,L"BUTTON",TR("check.clipboard"),BS_AUTOCHECKBOX|WS_TABSTOP,200,292,330,22,ID_CLIPBOARD);
	LAB(213,"label.audio",45,334);combo=gui_control(window,L"COMBOBOX","",CBS_DROPDOWNLIST|WS_TABSTOP,200,330,300,100,ID_AUDIO);gui_combo_add(combo,TR("audio.off"));gui_combo_add(combo,TR("audio.local"));
	LAB(220,"label.experience",45,220);combo=gui_control(window,L"COMBOBOX","",CBS_DROPDOWNLIST|WS_TABSTOP,200,216,250,100,ID_EXPERIENCE);gui_combo_add(combo,TR("experience.modem"));gui_combo_add(combo,TR("experience.broadband"));gui_combo_add(combo,TR("experience.lan"));gui_control(window,L"BUTTON",TR("check.motion"),BS_AUTOCHECKBOX|WS_TABSTOP,200,258,300,22,ID_SEND_MOTION);gui_control(window,L"BUTTON",TR("check.bitmap_cache"),BS_AUTOCHECKBOX|WS_TABSTOP,200,292,300,22,ID_BITMAP_CACHE);
	LAB(240,"label.port",45,205);gui_control(window,L"EDIT","",WS_BORDER|WS_TABSTOP|ES_NUMBER,200,201,100,24,ID_PORT);LAB(241,"label.protocol",45,237);combo=gui_control(window,L"COMBOBOX","",CBS_DROPDOWNLIST|WS_TABSTOP,200,233,180,80,ID_PROTOCOL);gui_combo_add(combo,"RDP 4");gui_combo_add(combo,"RDP 5");LAB(242,"label.language",45,269);combo=gui_control(window,L"COMBOBOX","",CBS_DROPDOWNLIST|WS_TABSTOP,200,265,250,110,ID_LANGUAGE);gui_combo_add(combo,TR("locale.en-US"));gui_combo_add(combo,TR("locale.en-GB"));gui_combo_add(combo,TR("locale.pt-BR"));gui_combo_add(combo,TR("locale.pt-PT"));
	gui_control(window,L"BUTTON",TR("check.compression"),BS_AUTOCHECKBOX|WS_TABSTOP,45,305,210,22,ID_COMPRESSION);gui_control(window,L"BUTTON",TR("check.admin"),BS_AUTOCHECKBOX|WS_TABSTOP,270,305,260,22,ID_ADMIN);gui_control(window,L"BUTTON",TR("check.retry"),BS_AUTOCHECKBOX|WS_TABSTOP,45,333,250,22,ID_RETRY);
	LAB(243,"label.tls",45,369);gui_control(window,L"EDIT","",WS_BORDER|WS_TABSTOP,200,365,90,24,ID_TLS);LAB(244,"label.hostname",310,369);gui_control(window,L"EDIT","",WS_BORDER|WS_TABSTOP,455,365,140,24,ID_HOSTNAME);LAB(245,"label.codepage",45,401);gui_control(window,L"EDIT","",WS_BORDER|WS_TABSTOP,200,397,90,24,ID_CODEPAGE);LAB(246,"label.shell",310,401);gui_control(window,L"EDIT","",WS_BORDER|WS_TABSTOP,455,397,140,24,ID_SHELL);LAB(247,"label.directory",45,433);gui_control(window,L"EDIT","",WS_BORDER|WS_TABSTOP,200,429,200,24,ID_DIRECTORY);LAB(248,"label.title",45,465);gui_control(window,L"EDIT","",WS_BORDER|WS_TABSTOP,200,461,300,24,ID_TITLE);
	gui_control(window,L"BUTTON",TR("check.input_diagnostics"),BS_AUTOCHECKBOX|WS_TABSTOP,45,215,300,22,ID_DIAGNOSTICS);LAB(225,"label.diagnostic_log",45,250);gui_control(window,L"EDIT","",WS_BORDER|ES_MULTILINE|ES_READONLY|WS_VSCROLL,45,275,550,220,ID_DIAG_LOG);gui_control(window,L"BUTTON",TR("button.clear"),BS_PUSHBUTTON,405,505,90,26,ID_DIAG_CLEAR);gui_control(window,L"BUTTON",TR("button.copy"),BS_PUSHBUTTON,505,505,90,26,ID_DIAG_COPY);
	gui_control(window,L"BUTTON",TR("button.connect"),BS_DEFPUSHBUTTON|WS_TABSTOP,455,125,95,30,ID_CONNECT);gui_control(window,L"BUTTON",TR("button.cancel"),BS_PUSHBUTTON|WS_TABSTOP,560,125,95,30,ID_CANCEL);
#undef LAB
	CheckDlgButton(window, ID_NUMLOCK, config->numlock_sync ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(window, ID_CLIPBOARD, config->clipboard_enabled ? BST_CHECKED : BST_UNCHECKED);
	gui_apply_config(window,config);
	SendDlgItemMessage(window,ID_LANGUAGE,CB_SETCURSEL,!_stricmp(state.locale,"en-GB")?1:!_stricmp(state.locale,"pt-BR")?2:!_stricmp(state.locale,"pt-PT")?3:0,0);
	win32_diagnostics_set_sink(gui_diagnostic_sink,&gui_diagnostic_log);
	CheckDlgButton(window,ID_DIAGNOSTICS,win32_diagnostics_input_enabled()?BST_CHECKED:BST_UNCHECKED);gui_refresh_diagnostic_log(window);
	SendMessage(window, DM_SETDEFID, ID_CONNECT, 0);
	SetFocus(GetDlgItem(window, ID_SERVER));
	gui_combo_caret_end(GetDlgItem(window, ID_SERVER));
	gui_set_font(window, font);
	gui_set_expanded(window,&state,state.expanded);
	{
		MONITORINFO mi; RECT wr; POINT origin={0,0}; HMONITOR monitor=MonitorFromPoint(origin,MONITOR_DEFAULTTOPRIMARY);
		mi.cbSize=sizeof(mi); if(GetMonitorInfoA(monitor,&mi)&&GetWindowRect(window,&wr))
			SetWindowPos(window,NULL,mi.rcWork.left+((mi.rcWork.right-mi.rcWork.left)-(wr.right-wr.left))/2,mi.rcWork.top+((mi.rcWork.bottom-mi.rcWork.top)-(wr.bottom-wr.top))/2,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
	}
	ShowWindow(window, SW_SHOW);
	UpdateWindow(window);
	SetFocus(GetDlgItem(window, ID_SERVER));
	gui_combo_caret_end(GetDlgItem(window, ID_SERVER));
	gui_combo_caret_end(GetDlgItem(window, ID_RESOLUTION));
	gui_combo_caret_end(GetDlgItem(window, ID_KEYBOARD));
	while (!state.done && GetMessage(&message, NULL, 0, 0) > 0)
	{
		if (!IsDialogMessage(window, &message))
		{
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
	}
	return state.result;
}

RD_BOOL
win32_config_dialog(RDESKTOP_CONFIG *config, char *error, size_t error_size)
{
	int result;
	do result=win32_config_dialog_once(config,error,error_size);
	while(result==GUI_EXIT_LANGUAGE_RELOAD);
	if(result==GUI_EXIT_USER_CLOSE)rdesktop_config_clear_password(config);
	return result==GUI_EXIT_CONNECT;
}

#else
RD_BOOL win32_config_dialog(RDESKTOP_CONFIG *config, char *error, size_t error_size)
{
	UNUSED(config); UNUSED(error); UNUSED(error_size); return False;
}
#endif
/* SPDX-License-Identifier: GPL-3.0-or-later */
