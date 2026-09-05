#ifdef _WIN32
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rdesktop.h"
#include "rop_semantics.h"
#include "win32_keymap.h"
#include "win32_toggle_state.h"
#include "win32_clipboard.h"
#include "config.h"
#include "win32_i18n.h"

extern RD_BOOL g_user_quit;
extern RD_BOOL g_exit_mainloop;
extern uint32 g_requested_session_width;
extern uint32 g_requested_session_height;
extern char g_title[];
extern RD_BOOL g_fullscreen;
extern RDESKTOP_WINDOW_MODE g_window_mode;
extern RD_BOOL g_show_connection_bar;
extern RD_BOOL g_sendmotion;
extern RD_BOOL g_numlock_sync;
extern int g_server_depth;
int g_win32_bitmap_bpp = 4;
int g_win32_bitmap_depth = 32;
static HWND win32_window;
static HWND win32_connection_bar;
static WNDPROC win32_connection_bar_default_proc;
static RDESKTOP_WINDOW_MODE win32_preferred_window_mode = RDESKTOP_WINDOW_CLIENT_SIZE;
#define WIN32_BAR_MINIMIZE 9101
#define WIN32_BAR_RESTORE 9102
#define WIN32_BAR_CLOSE 9103
static uint32 win32_width;
static uint32 win32_height;

static void win32_queue_input(uint16 type, uint16 flags, uint16 param1, uint16 param2);
static void win32_client_to_remote(POINT *point);
static void win32_remote_to_client(POINT *point);
static RD_BOOL win32_point_in_view(const POINT *point);
static void win32_send_key(UINT message, WPARAM key, LPARAM key_data, RD_BOOL pressed);
static void win32_release_remote_modifiers(void);
static void win32_enter_fullscreen(void);
static void win32_restore_from_fullscreen(void);
static void win32_create_connection_bar(int client_width);

static RD_BOOL win32_input_debug_enabled = False;
static RD_BOOL win32_input_debug_initialized = False;
static RD_BOOL win32_in_update = False;
static RD_BOOL win32_dirty_valid = False;
static RECT win32_dirty_rect;
static RD_BOOL win32_clip_active = False;
static RECT win32_clip_rect;
static RD_BOOL win32_resizing_guard = False;
static RD_BOOL win32_in_sizemove = False;
static int win32_view_x = 0;
static int win32_view_y = 0;
static uint32 win32_view_width = 0;
static uint32 win32_view_height = 0;
static int win32_remote_pointer_x = -1;
static int win32_remote_pointer_y = -1;
static uint32 win32_remote_pointer_time = 0;
static int win32_last_move_x = -1;
static int win32_last_move_y = -1;
static uint32 win32_last_move_time = 0;
static uint32 *win32_colourmap = NULL;
static uint16 win32_colourmap_count = 0;
static uint16 win32_colourmap_created_count = 0;
static RD_BOOL win32_pressed_keys[512];
static HHOOK win32_keyboard_hook;
static RD_BOOL win32_keyboard_sync_ready = False;

enum {
    WIN32_SCALE_FILTER_NEAREST = 0,
    WIN32_SCALE_FILTER_AUTO = 1,
    WIN32_SCALE_FILTER_HALFTONE = 2
};

static RD_BOOL win32_scale_filter_initialized = False;
static int win32_scale_filter_mode = WIN32_SCALE_FILTER_AUTO;

static uint32 win32_ticks(void)
{
    return (uint32)GetTickCount();
}

static uint32 win32_resolve_colour(uint32 colour)
{
    if (g_server_depth == 8 && win32_colourmap && colour < win32_colourmap_count)
        return win32_colourmap[colour];

    if (g_server_depth == 15) {
        uint32 blue = ((colour & 0x001f) << 3) | ((colour >> 2) & 0x07);
        uint32 green = (((colour >> 5) & 0x1f) << 3) | ((colour >> 7) & 0x07);
        uint32 red = (((colour >> 10) & 0x1f) << 3) | ((colour >> 12) & 0x07);
        return blue | (green << 8) | (red << 16);
    }

    if (g_server_depth == 16) {
        uint32 blue = ((colour & 0x001f) << 3) | ((colour >> 2) & 0x07);
        uint32 green = (((colour >> 5) & 0x3f) << 2) | ((colour >> 9) & 0x03);
        uint32 red = (((colour >> 11) & 0x1f) << 3) | ((colour >> 13) & 0x07);
        return blue | (green << 8) | (red << 16);
    }
    return colour;
}

static RD_BOOL win32_input_debug(void)
{
    if (!win32_input_debug_initialized) {
        const char *value = getenv("RDESKTOP_WIN32_INPUT_DEBUG");
        win32_input_debug_enabled = (value && value[0] && strcmp(value, "0") != 0) ? True : False;
        win32_input_debug_initialized = True;
    }
    return win32_input_debug_enabled;
}

static int win32_get_scale_filter_mode(void)
{
    if (!win32_scale_filter_initialized) {
        const char *value = getenv("RDESKTOP_WIN32_SCALE_FILTER");
        if (value && value[0]) {
            if (_stricmp(value, "auto") == 0)
                win32_scale_filter_mode = WIN32_SCALE_FILTER_AUTO;
            else if (_stricmp(value, "halftone") == 0)
                win32_scale_filter_mode = WIN32_SCALE_FILTER_HALFTONE;
            else
                win32_scale_filter_mode = WIN32_SCALE_FILTER_NEAREST;
        }
        win32_scale_filter_initialized = True;
    }
    return win32_scale_filter_mode;
}

static void win32_log_key_event(UINT message, WPARAM key, LPARAM key_data, uint16 flags, uint16 scan)
{
    if (!win32_input_debug())
        return;
    fprintf(stderr,
            "WIN32 KEY msg=0x%04x vk=0x%02x scan=0x%02x flags=0x%04x ext=%u prev=%u trans=%u\n",
            (unsigned int)message,
            (unsigned int)key,
            (unsigned int)scan,
            (unsigned int)flags,
            (unsigned int)((key_data >> 24) & 1),
            (unsigned int)((key_data >> 30) & 1),
            (unsigned int)((key_data >> 31) & 1));
}

static uint16 win32_get_toggle_state(void)
{
    return (uint16)win32_toggle_state_to_rdp_flags(
        (GetKeyState(VK_NUMLOCK) & 1) != 0,
        (GetKeyState(VK_CAPITAL) & 1) != 0,
        (GetKeyState(VK_SCROLL) & 1) != 0);
}

static void win32_synchronize_toggle_state(const char *reason)
{
    uint16 flags;
    if (!g_numlock_sync || !win32_keyboard_sync_ready)
        return;
    flags = win32_get_toggle_state();
    if (win32_input_debug())
        fprintf(stderr, "WIN32 KEY_SYNC flags=0x%04x reason=%s\n",
                (unsigned int)flags, reason);
    win32_queue_input(RDP_INPUT_SYNCHRONIZE, 0, flags, 0);
}

static void win32_log_mouse_event(UINT message, uint16 input_type, uint16 flags, int x, int y, SHORT wheel_delta)
{
    if (!win32_input_debug())
        return;
    if (message == WM_MOUSEMOVE)
        return;
    fprintf(stderr,
            "WIN32 MOUSE msg=0x%04x type=0x%04x flags=0x%04x x=%d y=%d wheel=%d\n",
            (unsigned int)message,
            (unsigned int)input_type,
            (unsigned int)flags,
            x,
            y,
            (int)wheel_delta);
}

static void win32_send_key(UINT message, WPARAM key, LPARAM key_data, RD_BOOL pressed)
{
    WIN32_KEY_TRANSLATION translated = win32_translate_key(
        (UINT)key, (unsigned long long)(size_t)key_data, pressed,
        MapVirtualKeyA((UINT)key, MAPVK_VK_TO_VSC));
    UINT scan = translated.scan;
    uint16 flags = pressed ? RDP_KEYPRESS : RDP_KEYRELEASE;
    unsigned int state_index = scan | (translated.extended ? 0x100 : 0);

    if (g_numlock_sync &&
        (key == VK_NUMLOCK || key == VK_CAPITAL || key == VK_SCROLL)) {
        if (!pressed)
            win32_synchronize_toggle_state("toggle");
        return;
    }
    if (translated.extended)
        flags |= KBD_FLAG_EXT;
    if (state_index < 512)
        win32_pressed_keys[state_index] = pressed;
    win32_log_key_event(message, key, key_data, flags, (uint16)scan);
    win32_queue_input(RDP_INPUT_SCANCODE, flags, (uint16)scan, 0);
}

static void win32_release_remote_modifiers(void)
{
    static const unsigned int modifiers[] = {
        0x02a, 0x036, 0x01d, 0x11d, 0x038, 0x138, 0x15b, 0x15c
    };
    unsigned int i;
    for (i = 0; i < sizeof(modifiers) / sizeof(modifiers[0]); i++) {
        unsigned int state = modifiers[i];
        if (win32_pressed_keys[state]) {
            uint16 flags = RDP_KEYRELEASE;
            if (state & 0x100)
                flags |= KBD_FLAG_EXT;
            win32_queue_input(RDP_INPUT_SCANCODE, flags,
                              (uint16)(state & 0xff), 0);
            win32_pressed_keys[state] = False;
        }
    }
}

static LRESULT CALLBACK win32_low_level_keyboard(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == HC_ACTION && win32_window && GetForegroundWindow() == win32_window) {
        KBDLLHOOKSTRUCT *event = (KBDLLHOOKSTRUCT *)lparam;
        if (event->vkCode == VK_LWIN || event->vkCode == VK_RWIN) {
            RD_BOOL pressed = (event->flags & LLKHF_UP) == 0;
            LPARAM key_data = (LPARAM)((event->scanCode & 0xff) << 16);
            if (event->flags & LLKHF_EXTENDED)
                key_data |= (LPARAM)(1UL << 24);
            if (!pressed)
                key_data |= (LPARAM)(1UL << 31);
            win32_send_key(pressed ? WM_KEYDOWN : WM_KEYUP,
                           event->vkCode, key_data, pressed);
            return 1;
        }
    }
    return CallNextHookEx(win32_keyboard_hook, code, wparam, lparam);
}

static void win32_send_mouse(UINT message, WPARAM state, LPARAM position)
{
    uint16 input_type = RDP_INPUT_MOUSE;
    uint16 flags = 0;
    POINT point;
    SHORT wheel_delta = 0;
    if (win32_in_sizemove)
        return;
    point.x = (int)(short)LOWORD(position);
    point.y = (int)(short)HIWORD(position);
    if (message == WM_MOUSEMOVE) {
        uint32 now = win32_ticks();
        if (point.x == win32_last_move_x && point.y == win32_last_move_y)
            return;
        if (point.x == win32_remote_pointer_x &&
            point.y == win32_remote_pointer_y &&
            (now - win32_remote_pointer_time) < 100)
            return;
        win32_last_move_x = point.x;
        win32_last_move_y = point.y;
        win32_last_move_time = now;
    }
    if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL)
        ScreenToClient(win32_window, &point);
    if (!win32_point_in_view(&point))
        return;
    win32_client_to_remote(&point);
    switch (message) {
    case WM_MOUSEMOVE:
        if (!g_sendmotion)
            return;
        flags = MOUSE_FLAG_MOVE;
        break;
    case WM_LBUTTONDOWN: flags = MOUSE_FLAG_BUTTON1 | MOUSE_FLAG_DOWN; break;
    case WM_LBUTTONUP: flags = MOUSE_FLAG_BUTTON1; break;
    case WM_RBUTTONDOWN: flags = MOUSE_FLAG_BUTTON2 | MOUSE_FLAG_DOWN; break;
    case WM_RBUTTONUP: flags = MOUSE_FLAG_BUTTON2; break;
    case WM_MBUTTONDOWN: flags = MOUSE_FLAG_BUTTON3 | MOUSE_FLAG_DOWN; break;
    case WM_MBUTTONUP: flags = MOUSE_FLAG_BUTTON3; break;
    case WM_MOUSEWHEEL:
        wheel_delta = GET_WHEEL_DELTA_WPARAM(state);
        flags = (wheel_delta >= 0 ? MOUSE_FLAG_BUTTON4 : MOUSE_FLAG_BUTTON5) | MOUSE_FLAG_DOWN;
        break;
    case WM_XBUTTONDOWN:
        input_type = RDP_INPUT_MOUSEX;
        flags = ((HIWORD(state) == XBUTTON1) ? MOUSEX_FLAG_BUTTON1 : MOUSEX_FLAG_BUTTON2) | MOUSE_FLAG_DOWN;
        break;
    case WM_XBUTTONUP:
        input_type = RDP_INPUT_MOUSEX;
        flags = (HIWORD(state) == XBUTTON1) ? MOUSEX_FLAG_BUTTON1 : MOUSEX_FLAG_BUTTON2;
        break;
    default: return;
    }
    if (win32_width > 0 && win32_height > 0) {
        if (point.x < 0)
            point.x = 0;
        if (point.y < 0)
            point.y = 0;
        if (point.x >= (int)win32_width)
            point.x = (int)win32_width - 1;
        if (point.y >= (int)win32_height)
            point.y = (int)win32_height - 1;
    }
    win32_log_mouse_event(message, input_type, flags, point.x, point.y, wheel_delta);
    win32_queue_input(input_type, flags, (uint16)point.x, (uint16)point.y);
}

static HDC win32_dc;
static HBITMAP win32_frame;
static HBITMAP win32_old_frame;
static uint8 *win32_pixels;
static uint32 win32_width;
static uint32 win32_height;
static uint32 win32_stride;
static WNDCLASSA win32_class;
static const char win32_class_name[] = "rdesktopWin32";
typedef struct {
    uint32 time;
    uint16 type;
    uint16 flags;
    uint16 param1;
    uint16 param2;
} WIN32_INPUT_EVENT;

static RD_BOOL win32_pending_move_valid = False;
static WIN32_INPUT_EVENT win32_pending_move;

static WIN32_INPUT_EVENT win32_input_queue[2048];
static unsigned int win32_input_head;
static unsigned int win32_input_tail;

typedef struct {
    HBITMAP bitmap;
    HDC source_dc;
    HGDIOBJ old_bitmap;
    void *bits;
    int width;
    int height;
} WIN32_BITMAP;

void ui_rect(int x, int y, int cx, int cy, uint32 colour);
static void win32_invalidate_rect(int x, int y, int cx, int cy);
static void win32_force_client_size(uint32 width, uint32 height);
static void win32_update_view_size(void);
static void win32_client_to_remote(POINT *point);
static void win32_remote_to_client(POINT *point);
static RD_BOOL win32_clip_rect_area(int *x, int *y, int *cx, int *cy);

static RD_BOOL win32_clip_rect_area(int *x, int *y, int *cx, int *cy)
{
    int right;
    int bottom;

    if (!x || !y || !cx || !cy || *cx <= 0 || *cy <= 0)
        return False;

    right = *x + *cx;
    bottom = *y + *cy;

    if (win32_clip_active) {
        if (*x < win32_clip_rect.left)
            *x = win32_clip_rect.left;
        if (*y < win32_clip_rect.top)
            *y = win32_clip_rect.top;
        if (right > win32_clip_rect.right)
            right = win32_clip_rect.right;
        if (bottom > win32_clip_rect.bottom)
            bottom = win32_clip_rect.bottom;
    }

    if (*x < 0)
        *x = 0;
    if (*y < 0)
        *y = 0;
    if (right > (int)win32_width)
        right = (int)win32_width;
    if (bottom > (int)win32_height)
        bottom = (int)win32_height;

    *cx = right - *x;
    *cy = bottom - *y;
    return (*cx > 0 && *cy > 0) ? True : False;
}

static uint32 win32_rop2_apply(uint8 opcode, uint32 dst, uint32 src)
{
    return rdp_rop2_apply24(opcode, dst, src);
}

static uint32 win32_read_pixel(int x, int y)
{
    uint8 *pixel = win32_pixels + ((size_t)y * win32_stride) + ((size_t)x * 4);
    return (uint32)pixel[0] | ((uint32)pixel[1] << 8) | ((uint32)pixel[2] << 16);
}

static void win32_write_pixel(int x, int y, uint32 colour)
{
    uint8 *pixel = win32_pixels + ((size_t)y * win32_stride) + ((size_t)x * 4);
    pixel[0] = (uint8)(colour & 0xff);
    pixel[1] = (uint8)((colour >> 8) & 0xff);
    pixel[2] = (uint8)((colour >> 16) & 0xff);
    pixel[3] = 0;
}

static void win32_update_view_size(void)
{
    RECT client;
    int client_w;
    int client_h;
    int view_w;
    int view_h;

    if (!win32_window)
        return;
    GetClientRect(win32_window, &client);
    client_w = client.right - client.left;
    client_h = client.bottom - client.top;

    if (client_w <= 0 || client_h <= 0 || win32_width == 0 || win32_height == 0) {
        win32_view_x = 0;
        win32_view_y = 0;
        win32_view_width = win32_width;
        win32_view_height = win32_height;
        return;
    }

    if ((long long)client_w * (long long)win32_height <=
        (long long)client_h * (long long)win32_width) {
        view_w = client_w;
        view_h = (int)((long long)client_w * (long long)win32_height / (long long)win32_width);
    } else {
        view_h = client_h;
        view_w = (int)((long long)client_h * (long long)win32_width / (long long)win32_height);
    }

    if (view_w < 1)
        view_w = 1;
    if (view_h < 1)
        view_h = 1;

    win32_view_x = (client_w - view_w) / 2;
    win32_view_y = (client_h - view_h) / 2;
    win32_view_width = (uint32)view_w;
    win32_view_height = (uint32)view_h;
}

static RD_BOOL win32_point_in_view(const POINT *point)
{
    if (!point)
        return False;
    if (win32_view_width == 0 || win32_view_height == 0)
        return False;
    if (point->x < win32_view_x || point->y < win32_view_y)
        return False;
    if (point->x >= win32_view_x + (int)win32_view_width)
        return False;
    if (point->y >= win32_view_y + (int)win32_view_height)
        return False;
    return True;
}

static void win32_client_to_remote(POINT *point)
{
    if (!point)
        return;
    if (win32_view_width == 0 || win32_view_height == 0 || win32_width == 0 || win32_height == 0)
        return;
    point->x -= win32_view_x;
    point->y -= win32_view_y;
    if (point->x < 0)
        point->x = 0;
    if (point->y < 0)
        point->y = 0;
    if (point->x >= (int)win32_view_width)
        point->x = (int)win32_view_width - 1;
    if (point->y >= (int)win32_view_height)
        point->y = (int)win32_view_height - 1;
    point->x = (int)((point->x * (int)win32_width) / (int)win32_view_width);
    point->y = (int)((point->y * (int)win32_height) / (int)win32_view_height);
}

static void win32_remote_to_client(POINT *point)
{
    if (!point)
        return;
    if (win32_view_width == 0 || win32_view_height == 0 || win32_width == 0 || win32_height == 0)
        return;
    point->x = win32_view_x + (int)((point->x * (int)win32_view_width) / (int)win32_width);
    point->y = win32_view_y + (int)((point->y * (int)win32_view_height) / (int)win32_height);
}

static void win32_queue_input(uint16 type, uint16 flags, uint16 param1, uint16 param2)
{
    if (type == RDP_INPUT_MOUSE && flags == MOUSE_FLAG_MOVE) {
        win32_pending_move.time = win32_ticks();
        win32_pending_move.type = type;
        win32_pending_move.flags = flags;
        win32_pending_move.param1 = param1;
        win32_pending_move.param2 = param2;
        win32_pending_move_valid = True;
        return;
    }
    unsigned int next = (win32_input_tail + 1) % (sizeof(win32_input_queue) / sizeof(win32_input_queue[0]));
    if (next == win32_input_head)
        win32_input_head = (win32_input_head + 1) % (sizeof(win32_input_queue) / sizeof(win32_input_queue[0]));
    win32_input_queue[win32_input_tail].time = win32_ticks();
    win32_input_queue[win32_input_tail].type = type;
    win32_input_queue[win32_input_tail].flags = flags;
    win32_input_queue[win32_input_tail].param1 = param1;
    win32_input_queue[win32_input_tail].param2 = param2;
    win32_input_tail = next;
}

static unsigned int win32_flush_input(void)
{
    unsigned int sent = 0;
    while (win32_input_head != win32_input_tail && sent < 256) {
        WIN32_INPUT_EVENT *event = &win32_input_queue[win32_input_head];
        rdp_send_input(event->time, event->type, event->flags,
                       event->param1, event->param2);
        win32_input_head = (win32_input_head + 1) % (sizeof(win32_input_queue) / sizeof(win32_input_queue[0]));
        sent++;
    }
    if (win32_pending_move_valid && sent < 256) {
        rdp_send_input(win32_pending_move.time, win32_pending_move.type,
                       win32_pending_move.flags, win32_pending_move.param1,
                       win32_pending_move.param2);
        win32_pending_move_valid = False;
        sent++;
    }
    return sent;
}

static unsigned int win32_pump_messages(void)
{
    MSG message;
    unsigned int pumped = 0;
    unsigned int sent;
    while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
        pumped++;
        if (message.message == WM_QUIT) {
            g_user_quit = True;
            g_exit_mainloop = True;
        }
        DispatchMessage(&message);
    }
    sent = win32_flush_input();
    UNUSED(sent);
    return pumped;
}

static LRESULT CALLBACK win32_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    LRESULT clipboard_result;
    if (win32_clipboard_handle_message(hwnd, message, wparam, lparam,
                                       &clipboard_result))
        return clipboard_result;
    switch (message) {
    case WM_COMMAND:
        if (LOWORD(wparam) == WIN32_BAR_MINIMIZE) {
            ShowWindow(hwnd, SW_MINIMIZE);
            return 0;
        }
        if (LOWORD(wparam) == WIN32_BAR_RESTORE) {
            win32_restore_from_fullscreen();
            return 0;
        }
        if (LOWORD(wparam) == WIN32_BAR_CLOSE) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        return DefWindowProcA(hwnd, message, wparam, lparam);
    case WM_ENTERSIZEMOVE:
        win32_in_sizemove = True;
        return 0;
    case WM_EXITSIZEMOVE:
        win32_in_sizemove = False;
        win32_update_view_size();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_SIZE:
        if (wparam == SIZE_MAXIMIZED && !g_fullscreen &&
            win32_preferred_window_mode == RDESKTOP_WINDOW_FULLSCREEN) {
            win32_enter_fullscreen();
            return 0;
        }
        if (wparam == SIZE_MAXIMIZED && !g_fullscreen)
            g_window_mode = RDESKTOP_WINDOW_FILL_AVAILABLE;
        win32_update_view_size();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_CLOSE:
        g_user_quit = True;
        g_exit_mainloop = True;
        DestroyWindow(hwnd);
        return 0;
    case WM_KILLFOCUS:
        win32_release_remote_modifiers();
        return 0;
    case WM_SETFOCUS:
        win32_synchronize_toggle_state("focus");
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE)
            win32_release_remote_modifiers();
        return DefWindowProcA(hwnd, message, wparam, lparam);
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        return win32_frame ? 1 : DefWindowProc(hwnd, message, wparam, lparam);
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);
        RECT client;
        HBRUSH back;
        int filter_mode;
        RD_BOOL scaled;
        if (win32_frame) {
            RECT viewport;
            RECT dirty;
            GetClientRect(hwnd, &client);
            back = CreateSolidBrush(RGB(0, 0, 0));
            if (win32_view_y > client.top) {
                RECT box = { client.left, client.top, client.right, win32_view_y };
                FillRect(dc, &box, back);
            }
            if (win32_view_y + (LONG)win32_view_height < client.bottom) {
                RECT box = { client.left, win32_view_y + (LONG)win32_view_height,
                             client.right, client.bottom };
                FillRect(dc, &box, back);
            }
            if (win32_view_x > client.left) {
                RECT box = { client.left, win32_view_y, win32_view_x,
                             win32_view_y + (LONG)win32_view_height };
                FillRect(dc, &box, back);
            }
            if (win32_view_x + (LONG)win32_view_width < client.right) {
                RECT box = { win32_view_x + (LONG)win32_view_width, win32_view_y,
                             client.right, win32_view_y + (LONG)win32_view_height };
                FillRect(dc, &box, back);
            }
            DeleteObject(back);
            scaled = ((int)win32_view_width != (int)win32_width) ||
                     ((int)win32_view_height != (int)win32_height);
            filter_mode = win32_get_scale_filter_mode();
            if (filter_mode == WIN32_SCALE_FILTER_HALFTONE && scaled) {
                SetStretchBltMode(dc, HALFTONE);
                SetBrushOrgEx(dc, 0, 0, NULL);
            } else if (filter_mode == WIN32_SCALE_FILTER_AUTO &&
                       ((int)win32_view_width < (int)win32_width ||
                        (int)win32_view_height < (int)win32_height)) {
                SetStretchBltMode(dc, HALFTONE);
                SetBrushOrgEx(dc, 0, 0, NULL);
            } else {
                SetStretchBltMode(dc, COLORONCOLOR);
            }
            if (!scaled) {
                viewport.left = win32_view_x;
                viewport.top = win32_view_y;
                viewport.right = win32_view_x + (LONG)win32_view_width;
                viewport.bottom = win32_view_y + (LONG)win32_view_height;
                if (IntersectRect(&dirty, &paint.rcPaint, &viewport))
                    BitBlt(dc, dirty.left, dirty.top,
                           dirty.right - dirty.left, dirty.bottom - dirty.top,
                           win32_dc, dirty.left - win32_view_x,
                           dirty.top - win32_view_y, SRCCOPY);
            } else {
                StretchBlt(dc, win32_view_x, win32_view_y,
                           (int)win32_view_width, (int)win32_view_height,
                           win32_dc, 0, 0, (int)win32_width, (int)win32_height,
                           SRCCOPY);
            }
        }
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_KEYDOWN:
        win32_send_key(message, wparam, lparam, True);
        return 0;
    case WM_KEYUP:
        win32_send_key(message, wparam, lparam, False);
        return 0;
    case WM_SYSKEYDOWN:
        win32_send_key(message, wparam, lparam, True);
        return 0;
    case WM_SYSKEYUP:
        win32_send_key(message, wparam, lparam, False);
        return 0;
    case WM_MOUSEMOVE:
        win32_send_mouse(message, wparam, lparam);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
        SetFocus(hwnd);
        win32_send_mouse(message, wparam, lparam);
        if (message == WM_XBUTTONDOWN)
            return TRUE;
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
        win32_send_mouse(message, wparam, lparam);
        if (message == WM_XBUTTONUP)
            return TRUE;
        return 0;
    case WM_MOUSEWHEEL:
        win32_send_mouse(message, wparam, lparam);
        return 0;
    default:
        return DefWindowProcA(hwnd, message, wparam, lparam);
    }
}

static void win32_release_frame(void)
{
    if (win32_frame) {
        if (win32_dc && win32_old_frame) {
            SelectObject(win32_dc, win32_old_frame);
            win32_old_frame = NULL;
        }
        DeleteObject(win32_frame);
        win32_frame = NULL;
        win32_pixels = NULL;
    }
}

RD_BOOL ui_init(void)
{
    HINSTANCE instance = GetModuleHandleA(NULL);
    WSADATA winsock_data;
    if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0)
        return False;
    xclip_init();
    win32_keyboard_hook = SetWindowsHookExA(WH_KEYBOARD_LL,
                                             win32_low_level_keyboard,
                                             GetModuleHandleA(NULL), 0);
    memset(&win32_class, 0, sizeof(win32_class));
    win32_class.lpfnWndProc = win32_window_proc;
    win32_class.hInstance = instance;
    win32_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    win32_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    win32_class.lpszClassName = win32_class_name;
    if (!RegisterClassA(&win32_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return False;
    return True;
}

void ui_get_screen_size(uint32 *width, uint32 *height)
{
    *width = (uint32)GetSystemMetrics(SM_CXSCREEN);
    *height = (uint32)GetSystemMetrics(SM_CYSCREEN);
}

RD_BOOL ui_get_monitor_layout(RDP_MONITOR_LAYOUT *monitors, uint32 max_monitors,
                              uint32 *monitor_count, uint32 *desktop_width,
                              uint32 *desktop_height)
{
    uint32 width, height;
    if (!monitors || max_monitors == 0 || !monitor_count || !desktop_width || !desktop_height)
        return False;
    ui_get_screen_size(&width, &height);
    monitors[0].left = 0;
    monitors[0].top = 0;
    monitors[0].right = (sint32)width - 1;
    monitors[0].bottom = (sint32)height - 1;
    monitors[0].flags = RDESKTOP_MONITOR_PRIMARY;
    *monitor_count = 1;
    *desktop_width = width;
    *desktop_height = height;
    return True;
}

void ui_get_screen_size_from_percentage(uint32 pw, uint32 ph, uint32 *width, uint32 *height)
{
    uint32 screen_width, screen_height;
    ui_get_screen_size(&screen_width, &screen_height);
    *width = screen_width * pw / 100;
    *height = screen_height * ph / 100;
}

void ui_get_workarea_size(uint32 *width, uint32 *height)
{
    RECT workarea;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &workarea, 0);
    *width = (uint32)(workarea.right - workarea.left);
    *height = (uint32)(workarea.bottom - workarea.top);
}

static LRESULT CALLBACK
win32_connection_bar_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_COMMAND) {
        SendMessage(GetParent(hwnd), message, wparam, lparam);
        return 0;
    }
    return CallWindowProcW(win32_connection_bar_default_proc, hwnd, message, wparam, lparam);
}

static void
win32_create_connection_bar(int client_width)
{
    HFONT font;
    HWND item;
    WCHAR title[256];
    if (!g_show_connection_bar || !win32_window || win32_connection_bar)
        return;
    font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    MultiByteToWideChar(CP_UTF8, 0, g_title[0] ? g_title : "rdesktop", -1, title, 256);
    win32_connection_bar = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER, (client_width - 560) / 2, 0, 560, 34,
        win32_window, NULL, GetModuleHandleW(NULL), NULL);
    if (!win32_connection_bar)
        return;
    win32_connection_bar_default_proc = (WNDPROC)SetWindowLongPtrW(
        win32_connection_bar, GWLP_WNDPROC, (LONG_PTR)win32_connection_bar_proc);
    item = CreateWindowExW(0, L"STATIC", title, WS_CHILD | WS_VISIBLE | SS_CENTER,
        8, 8, 290, 20, win32_connection_bar, NULL, GetModuleHandleW(NULL), NULL);
    SendMessage(item, WM_SETFONT, (WPARAM)font, TRUE);
    item = CreateWindowExW(0, L"BUTTON", L"_", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        310, 4, 50, 25, win32_connection_bar, (HMENU)WIN32_BAR_MINIMIZE,
        GetModuleHandleW(NULL), NULL);
    SendMessage(item, WM_SETFONT, (WPARAM)font, TRUE);
    item = CreateWindowExW(0, L"BUTTON", win32_i18n_get_w("bar.restore"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 365, 4, 85, 25, win32_connection_bar,
        (HMENU)WIN32_BAR_RESTORE, GetModuleHandleW(NULL), NULL);
    SendMessage(item, WM_SETFONT, (WPARAM)font, TRUE);
    item = CreateWindowExW(0, L"BUTTON", win32_i18n_get_w("bar.disconnect"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 455, 4, 96, 25, win32_connection_bar,
        (HMENU)WIN32_BAR_CLOSE, GetModuleHandleW(NULL), NULL);
    SendMessage(item, WM_SETFONT, (WPARAM)font, TRUE);
}

static void
win32_enter_fullscreen(void)
{
    MONITORINFO mi;
    HMONITOR monitor;
    if (!win32_window || g_fullscreen)
        return;
    monitor = MonitorFromWindow(win32_window, MONITOR_DEFAULTTONEAREST);
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoA(monitor, &mi))
        return;
    g_fullscreen = True;
    g_window_mode = RDESKTOP_WINDOW_FULLSCREEN;
    SetWindowLongA(win32_window, GWL_STYLE, WS_POPUP);
    SetWindowPos(win32_window, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
        mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    win32_create_connection_bar(mi.rcMonitor.right - mi.rcMonitor.left);
    win32_update_view_size();
    InvalidateRect(win32_window, NULL, FALSE);
    SetFocus(win32_window);
}

static void
win32_restore_from_fullscreen(void)
{
    MONITORINFO mi;
    HMONITOR monitor;
    RECT outer = { 0, 0, (LONG)win32_width, (LONG)win32_height };
    int width, height, x, y;
    if (!win32_window || !g_fullscreen)
        return;
    monitor = MonitorFromWindow(win32_window, MONITOR_DEFAULTTONEAREST);
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoA(monitor, &mi))
        return;
    g_fullscreen = False;
    g_window_mode = RDESKTOP_WINDOW_CLIENT_SIZE;
    if (win32_connection_bar) {
        DestroyWindow(win32_connection_bar);
        win32_connection_bar = NULL;
    }
    SetWindowLongA(win32_window, GWL_STYLE, WS_OVERLAPPEDWINDOW);
    AdjustWindowRectEx(&outer, WS_OVERLAPPEDWINDOW, FALSE, 0);
    width = outer.right - outer.left;
    height = outer.bottom - outer.top;
    if (width > mi.rcWork.right - mi.rcWork.left)
        width = mi.rcWork.right - mi.rcWork.left;
    if (height > mi.rcWork.bottom - mi.rcWork.top)
        height = mi.rcWork.bottom - mi.rcWork.top;
    x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - width) / 2;
    y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - height) / 2;
    SetWindowPos(win32_window, HWND_NOTOPMOST, x, y, width, height,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    win32_update_view_size();
    InvalidateRect(win32_window, NULL, FALSE);
    SetFocus(win32_window);
}

void ui_deinit(void)
{
    win32_release_remote_modifiers();
    if (win32_keyboard_hook) {
        UnhookWindowsHookEx(win32_keyboard_hook);
        win32_keyboard_hook = NULL;
    }
    xclip_deinit();
    ui_destroy_window();
    if (win32_colourmap) {
        free(win32_colourmap);
        win32_colourmap = NULL;
        win32_colourmap_count = 0;
    }
    UnregisterClassA(win32_class_name, GetModuleHandleA(NULL));
    WSACleanup();
}

RD_BOOL ui_create_window(uint32 width, uint32 height)
{
    DWORD style = WS_OVERLAPPEDWINDOW;
    HDC screen_dc;
    RECT rect = { 0, 0, (LONG)width, (LONG)height };
    RECT workarea;
    int work_w;
    int work_h;
    int target_client_w;
    int target_client_h;
    int wnd_x = CW_USEDEFAULT;
    int wnd_y = CW_USEDEFAULT;
    win32_preferred_window_mode = g_window_mode;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &workarea, 0);
    work_w = workarea.right - workarea.left;
    work_h = workarea.bottom - workarea.top;
    if (g_fullscreen) {
        style = WS_POPUP;
        wnd_x = 0;
        wnd_y = 0;
    } else {
        target_client_w = (int)width;
        target_client_h = (int)height;
        if (target_client_w > work_w)
            target_client_w = work_w;
        if (target_client_h > work_h)
            target_client_h = work_h;
        rect.right = rect.left + target_client_w;
        rect.bottom = rect.top + target_client_h;
        AdjustWindowRect(&rect, style, FALSE);
        if (target_client_w != (int)width || target_client_h != (int)height) {
            wnd_x = workarea.left;
            wnd_y = workarea.top;
        }
    }
    win32_window = CreateWindowExA(0, win32_class_name,
                                   g_title[0] ? g_title : "rdesktop",
                                   style, wnd_x, wnd_y,
                                   rect.right - rect.left, rect.bottom - rect.top,
                                   NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!win32_window)
        return False;
    win32_connection_bar = NULL;
    if (g_fullscreen)
        win32_create_connection_bar((int)width);
    win32_clipboard_attach(win32_window);
    win32_dc = CreateCompatibleDC(NULL);
    if (!win32_dc)
        return False;
    win32_width = width;
    win32_height = height;
    win32_stride = width * 4;
    {
        BITMAPINFO info;
        screen_dc = GetDC(NULL);
        memset(&info, 0, sizeof(info));
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = (LONG)width;
        info.bmiHeader.biHeight = -(LONG)height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        win32_frame = CreateDIBSection(screen_dc, &info, DIB_RGB_COLORS,
                                       (void **)&win32_pixels, NULL, 0);
        ReleaseDC(NULL, screen_dc);
        if (win32_frame)
            win32_old_frame = (HBITMAP)SelectObject(win32_dc, win32_frame);
    }
    ShowWindow(win32_window, g_window_mode == RDESKTOP_WINDOW_FILL_AVAILABLE ?
               SW_MAXIMIZE : SW_SHOW);
    UpdateWindow(win32_window);
    win32_update_view_size();
    if (g_fullscreen)
        win32_force_client_size(width, height);
    return win32_frame != NULL;
}

void ui_resize_window(uint32 width, uint32 height)
{
    BITMAPINFO info;
    HDC screen_dc;
    if (!win32_window)
        return;
    if (width != win32_width || height != win32_height) {
        win32_release_frame();
        screen_dc = GetDC(NULL);
        memset(&info, 0, sizeof(info));
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = (LONG)width;
        info.bmiHeader.biHeight = -(LONG)height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        win32_frame = CreateDIBSection(screen_dc, &info, DIB_RGB_COLORS,
                                       (void **)&win32_pixels, NULL, 0);
        ReleaseDC(NULL, screen_dc);
        if (win32_frame)
            win32_old_frame = (HBITMAP)SelectObject(win32_dc, win32_frame);
    }
    if (g_fullscreen)
        win32_force_client_size(width, height);
    win32_width = width;
    win32_height = height;
    win32_stride = width * 4;
    win32_update_view_size();
    InvalidateRect(win32_window, NULL, FALSE);
}

void ui_destroy_window(void)
{
    win32_clipboard_detach();
    win32_release_frame();
    if (win32_dc) {
        DeleteDC(win32_dc);
        win32_dc = NULL;
    }
    if (win32_window) {
        DestroyWindow(win32_window);
        win32_window = NULL;
    }
}

void ui_update_window_sizehints(uint32 width, uint32 height) { UNUSED(width); UNUSED(height); }
RD_BOOL ui_have_window(void) { return win32_window != NULL; }
void ui_release_keyboard(void) { }
void xwin_toggle_fullscreen(void) { }

void ui_select(int rdp_socket)
{
    fd_set readfds;
    fd_set writefds;
    struct timeval timeout = { 0, 50000 };
    int max_fd;
    int rc;
    do {
        win32_pump_messages();
        if (g_exit_mainloop)
            return;
        timeout.tv_sec = 0;
        timeout.tv_usec = rdp_heartbeat_select_timeout(50) * 1000;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET((SOCKET)rdp_socket, &readfds);
        max_fd = rdp_socket;
#ifdef WITH_RDPSND
        rdpsnd_add_fds(&max_fd, &readfds, &writefds, &timeout);
#endif
        rc = select(0, &readfds, &writefds, NULL, &timeout);
#ifdef WITH_RDPSND
        rdpsnd_check_fds(&readfds, &writefds);
#endif
        rdp_heartbeat_check_timeout();
    } while (rc == 0 && !g_exit_mainloop);
}

void ui_move_pointer(int x, int y)
{
    POINT pt;
    if (win32_in_sizemove)
        return;
    win32_remote_pointer_x = x;
    win32_remote_pointer_y = y;
    win32_remote_pointer_time = win32_ticks();
    pt.x = x;
    pt.y = y;
    win32_remote_to_client(&pt);
    if (win32_window)
        ClientToScreen(win32_window, &pt);
    SetCursorPos(pt.x, pt.y);
}

RD_HBITMAP ui_create_bitmap(int width, int height, uint8 *data)
{
    WIN32_BITMAP *bitmap = (WIN32_BITMAP *)calloc(1, sizeof(*bitmap));
    BITMAPINFO info;
    if (!bitmap)
        return NULL;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap->bitmap = CreateDIBSection(win32_dc, &info, DIB_RGB_COLORS,
                                      &bitmap->bits, NULL, 0);
    bitmap->width = width;
    bitmap->height = height;
    if (!bitmap->bitmap) {
        free(bitmap);
        return NULL;
    }
    bitmap->source_dc = CreateCompatibleDC(win32_dc);
    if (!bitmap->source_dc) {
        DeleteObject(bitmap->bitmap);
        free(bitmap);
        return NULL;
    }
    bitmap->old_bitmap = SelectObject(bitmap->source_dc, bitmap->bitmap);
    {
        int source_bpp = g_win32_bitmap_bpp;
        int row;
        if (source_bpp < 1 || source_bpp > 4)
            source_bpp = 4;
        for (row = 0; row < height; row++) {
            int column;
            uint8 *source = data + row * width * source_bpp;
            uint8 *target = (uint8 *)bitmap->bits + row * width * 4;
            for (column = 0; column < width; column++) {
                uint32 pixel = 0;
                if (source_bpp == 1) {
                    pixel = source[column];
                    if (win32_colourmap && pixel < win32_colourmap_count)
                        pixel = win32_colourmap[pixel];
                }
                else if (source_bpp == 2)
                    pixel = source[column * 2] | ((uint32)source[column * 2 + 1] << 8);
                else if (source_bpp == 3)
                    pixel = source[column * 3] | ((uint32)source[column * 3 + 1] << 8) |
                            ((uint32)source[column * 3 + 2] << 16);
                else
                    pixel = source[column * 4] | ((uint32)source[column * 4 + 1] << 8) |
                            ((uint32)source[column * 4 + 2] << 16);
                if (source_bpp == 1) {
                    target[column * 4] = (uint8)(pixel & 0xff);
                    target[column * 4 + 1] = (uint8)((pixel >> 8) & 0xff);
                    target[column * 4 + 2] = (uint8)((pixel >> 16) & 0xff);
                } else if (source_bpp == 2) {
                    uint32 resolved = win32_resolve_colour(pixel);
                    target[column * 4] = (uint8)(resolved & 0xff);
                    target[column * 4 + 1] = (uint8)((resolved >> 8) & 0xff);
                    target[column * 4 + 2] = (uint8)((resolved >> 16) & 0xff);
                } else {
                    target[column * 4] = (uint8)(pixel & 0xff);
                    target[column * 4 + 1] = (uint8)((pixel >> 8) & 0xff);
                    target[column * 4 + 2] = (uint8)((pixel >> 16) & 0xff);
                }
                target[column * 4 + 3] = 0;
            }
        }
    }
    return (RD_HBITMAP)bitmap;
}

RD_BOOL ui_update_bitmap(RD_HBITMAP handle, int width, int height, uint8 *data)
{
    WIN32_BITMAP *bitmap = (WIN32_BITMAP *)handle;
    WIN32_BITMAP *replacement;
    if (!bitmap)
        return False;
    replacement = (WIN32_BITMAP *)ui_create_bitmap(width, height, data);
    if (!replacement)
        return False;
    if (bitmap->source_dc) {
        SelectObject(bitmap->source_dc, bitmap->old_bitmap);
        DeleteDC(bitmap->source_dc);
    }
    DeleteObject(bitmap->bitmap);
    *bitmap = *replacement;
    free(replacement);
    return True;
}

void ui_paint_bitmap(int x, int y, int cx, int cy, int width, int height, uint8 *data)
{
    int source_bpp;
    int row;
    int start_row;
    int start_col;
    int end_row;
    int end_col;
    int max_dst_row;
    int max_dst_col;
    int dst_x;
    int dst_y;
    if (!win32_dc || !win32_pixels || width <= 0 || height <= 0)
        return;
    source_bpp = g_win32_bitmap_bpp;
    if (source_bpp < 1 || source_bpp > 4)
        source_bpp = 4;
    start_row = (y < 0) ? -y : 0;
    start_col = (x < 0) ? -x : 0;
    end_row = height;
    end_col = width;
    max_dst_row = cy;
    max_dst_col = cx;
    if (max_dst_row < end_row)
        end_row = max_dst_row;
    if (max_dst_col < end_col)
        end_col = max_dst_col;
    if (y + end_row > (int)win32_height)
        end_row = (int)win32_height - y;
    if (x + end_col > (int)win32_width)
        end_col = (int)win32_width - x;
    if (end_row <= start_row || end_col <= start_col)
        return;
    dst_x = (x < 0) ? 0 : x;
    dst_y = (y < 0) ? 0 : y;
    for (row = start_row; row < end_row; row++) {
        int column;
        uint8 *source = data + row * width * source_bpp;
        uint8 *target = win32_pixels + (dst_y + (row - start_row)) * win32_stride + dst_x * 4;
        for (column = start_col; column < end_col; column++) {
            int dst_col = column - start_col;
            uint32 pixel = 0;
            if (source_bpp == 1) {
                pixel = source[column];
                if (win32_colourmap && pixel < win32_colourmap_count)
                    pixel = win32_colourmap[pixel];
            }
            else if (source_bpp == 2)
                pixel = source[column * 2] | ((uint32)source[column * 2 + 1] << 8);
            else if (source_bpp == 3)
                pixel = source[column * 3] | ((uint32)source[column * 3 + 1] << 8) |
                        ((uint32)source[column * 3 + 2] << 16);
            else
                pixel = source[column * 4] | ((uint32)source[column * 4 + 1] << 8) |
                        ((uint32)source[column * 4 + 2] << 16);
            if (source_bpp == 1) {
                target[dst_col * 4] = (uint8)(pixel & 0xff);
                target[dst_col * 4 + 1] = (uint8)((pixel >> 8) & 0xff);
                target[dst_col * 4 + 2] = (uint8)((pixel >> 16) & 0xff);
            } else if (source_bpp == 2) {
                uint32 resolved = win32_resolve_colour(pixel);
                target[dst_col * 4] = (uint8)(resolved & 0xff);
                target[dst_col * 4 + 1] = (uint8)((resolved >> 8) & 0xff);
                target[dst_col * 4 + 2] = (uint8)((resolved >> 16) & 0xff);
            } else {
                target[dst_col * 4] = (uint8)(pixel & 0xff);
                target[dst_col * 4 + 1] = (uint8)((pixel >> 8) & 0xff);
                target[dst_col * 4 + 2] = (uint8)((pixel >> 16) & 0xff);
            }
            target[dst_col * 4 + 3] = 0;
        }
    }
    win32_invalidate_rect(dst_x, dst_y, end_col - start_col, end_row - start_row);
}

void ui_destroy_bitmap(RD_HBITMAP handle)
{
    WIN32_BITMAP *bitmap = (WIN32_BITMAP *)handle;
    if (bitmap) {
        if (bitmap->source_dc) {
            SelectObject(bitmap->source_dc, bitmap->old_bitmap);
            DeleteDC(bitmap->source_dc);
        }
        DeleteObject(bitmap->bitmap);
        free(bitmap);
    }
}

RD_HGLYPH ui_create_glyph(int width, int height, uint8 *data)
{
    WIN32_BITMAP *bitmap = (WIN32_BITMAP *)calloc(1, sizeof(*bitmap));
    BITMAPINFO info;
    int row;
    int source_stride = (width + 7) / 8;
    if (!bitmap)
        return NULL;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap->bitmap = CreateDIBSection(win32_dc, &info, DIB_RGB_COLORS,
                                      &bitmap->bits, NULL, 0);
    bitmap->width = width;
    bitmap->height = height;
    if (!bitmap->bitmap) {
        free(bitmap);
        return NULL;
    }
    memset(bitmap->bits, 0, (size_t)width * height * 4);
    for (row = 0; row < height; row++) {
        int column;
        for (column = 0; column < width; column++) {
            if (data[row * source_stride + column / 8] & (0x80 >> (column % 8))) {
                uint8 *pixel = (uint8 *)bitmap->bits + (row * width + column) * 4;
                pixel[0] = 255;
                pixel[1] = 255;
                pixel[2] = 255;
                pixel[3] = 255;
            }
        }
    }
    return (RD_HGLYPH)bitmap;
}
void ui_destroy_glyph(RD_HGLYPH glyph) { ui_destroy_bitmap(glyph); }
RD_HCURSOR ui_create_cursor(unsigned int x, unsigned int y, uint32 width, uint32 height, uint8 *andmask, uint8 *xormask, int bpp) { UNUSED(x); UNUSED(y); UNUSED(width); UNUSED(height); UNUSED(andmask); UNUSED(xormask); UNUSED(bpp); return NULL; }
void ui_set_cursor(RD_HCURSOR cursor) { SetCursor((HCURSOR)cursor); }
void ui_destroy_cursor(RD_HCURSOR cursor) { UNUSED(cursor); }
void ui_set_null_cursor(void) { ui_set_standard_cursor(); }
void ui_set_standard_cursor(void) { SetCursor(LoadCursor(NULL, IDC_ARROW)); }
RD_HCOLOURMAP ui_create_colourmap(COLOURMAP *colours)
{
    uint16 i;
    uint32 *map;
    if (!colours || colours->ncolours == 0 || !colours->colours)
        return NULL;
    map = (uint32 *)calloc(colours->ncolours, sizeof(uint32));
    if (!map)
        return NULL;
    for (i = 0; i < colours->ncolours; i++) {
        COLOURENTRY *entry = &colours->colours[i];
        map[i] = (uint32)entry->blue | ((uint32)entry->green << 8) | ((uint32)entry->red << 16);
    }
    win32_colourmap_created_count = colours->ncolours;
    return (RD_HCOLOURMAP)map;
}
void ui_destroy_colourmap(RD_HCOLOURMAP map)
{
    if (map)
        free(map);
}
void ui_set_colourmap(RD_HCOLOURMAP map)
{
    if (win32_colourmap) {
        free(win32_colourmap);
        win32_colourmap = NULL;
        win32_colourmap_count = 0;
    }
    win32_colourmap = (uint32 *)map;
    if (win32_colourmap)
        win32_colourmap_count = win32_colourmap_created_count;
}
void ui_set_clip(int x, int y, int cx, int cy)
{
    win32_clip_rect.left = x;
    win32_clip_rect.top = y;
    win32_clip_rect.right = x + cx;
    win32_clip_rect.bottom = y + cy;
    win32_clip_active = (cx > 0 && cy > 0) ? True : False;
}
void ui_reset_clip(void)
{
    win32_clip_active = False;
}
void ui_bell(void) { MessageBeep(MB_OK); }
static COLORREF win32_colour(uint32 colour)
{
    colour = win32_resolve_colour(colour);
    return RGB((colour >> 16) & 0xff, (colour >> 8) & 0xff, colour & 0xff);
}

static void win32_invalidate(void)
{
    if (win32_window)
        InvalidateRect(win32_window, NULL, FALSE);
}

static void win32_force_client_size(uint32 width, uint32 height)
{
    RECT client;
    RECT outer;
    DWORD style;
    DWORD exstyle;
    RECT window;
    int attempt;
    if (!win32_window)
        return;
    style = (DWORD)GetWindowLongA(win32_window, GWL_STYLE);
    exstyle = (DWORD)GetWindowLongA(win32_window, GWL_EXSTYLE);
    outer.left = 0;
    outer.top = 0;
    outer.right = (LONG)width;
    outer.bottom = (LONG)height;
    AdjustWindowRectEx(&outer, style, FALSE, exstyle);
    for (attempt = 0; attempt < 3; attempt++) {
        GetClientRect(win32_window, &client);
        if ((uint32)(client.right - client.left) == width &&
            (uint32)(client.bottom - client.top) == height)
            return;
        GetWindowRect(win32_window, &window);
        win32_resizing_guard = True;
        SetWindowPos(win32_window, NULL,
                     window.left,
                     window.top,
                     outer.right - outer.left,
                     outer.bottom - outer.top,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        win32_resizing_guard = False;
    }
}

static void win32_invalidate_rect(int x, int y, int cx, int cy)
{
    RECT rect;
    if (!win32_window || cx <= 0 || cy <= 0)
        return;
    if (win32_view_width > 0 && win32_view_height > 0 && win32_width > 0 && win32_height > 0) {
        int x2 = x + cx;
        int y2 = y + cy;
        rect.left = (LONG)(win32_view_x + ((x * (int)win32_view_width) / (int)win32_width));
        rect.top = (LONG)(win32_view_y + ((y * (int)win32_view_height) / (int)win32_height));
        rect.right = (LONG)(win32_view_x + ((x2 * (int)win32_view_width + (int)win32_width - 1) / (int)win32_width));
        rect.bottom = (LONG)(win32_view_y + ((y2 * (int)win32_view_height + (int)win32_height - 1) / (int)win32_height));
    } else {
        rect.left = x;
        rect.top = y;
        rect.right = x + cx;
        rect.bottom = y + cy;
    }
    if (win32_in_update) {
        if (!win32_dirty_valid) {
            win32_dirty_rect = rect;
            win32_dirty_valid = True;
        } else {
            if (rect.left < win32_dirty_rect.left)
                win32_dirty_rect.left = rect.left;
            if (rect.top < win32_dirty_rect.top)
                win32_dirty_rect.top = rect.top;
            if (rect.right > win32_dirty_rect.right)
                win32_dirty_rect.right = rect.right;
            if (rect.bottom > win32_dirty_rect.bottom)
                win32_dirty_rect.bottom = rect.bottom;
        }
        return;
    }
    InvalidateRect(win32_window, &rect, FALSE);
}

void ui_destblt(uint8 opcode, int x, int y, int cx, int cy)
{
    int row;
    int col;
    if (!win32_pixels || !win32_clip_rect_area(&x, &y, &cx, &cy))
        return;

    for (row = 0; row < cy; row++) {
        for (col = 0; col < cx; col++) {
            uint32 dst = win32_read_pixel(x + col, y + row);
            win32_write_pixel(x + col, y + row, win32_rop2_apply(opcode, dst, 0));
        }
    }
    win32_invalidate_rect(x, y, cx, cy);
}

void ui_patblt(uint8 opcode, int x, int y, int cx, int cy, BRUSH *brush, uint32 bgcolour, uint32 fgcolour)
{
    static uint8 hatch_patterns[] = {
        0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
        0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
        0x08, 0x08, 0x08, 0xff, 0x08, 0x08, 0x08, 0x08,
        0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81
    };
    int row;
    int col;
    uint32 resolved_bg = win32_resolve_colour(bgcolour);
    uint32 resolved_fg = win32_resolve_colour(fgcolour);

    if (!win32_clip_rect_area(&x, &y, &cx, &cy))
        return;

    if (!win32_pixels)
        return;

    for (row = 0; row < cy; row++) {
        int py = y + row;
        for (col = 0; col < cx; col++) {
            int px = x + col;
            uint32 src_colour = resolved_fg;
            uint32 dst_colour = win32_read_pixel(px, py);

            if (brush) {
                switch (brush->style) {
                case 0:
                    src_colour = resolved_fg;
                    break;
                case 2: {
                    int pattern_row = (py - brush->yorigin) & 7;
                    int pattern_col = (px - brush->xorigin) & 7;
                    uint8 bits = hatch_patterns[(brush->pattern[0] % 6) * 8 + pattern_row];
                    src_colour = (bits & (0x80 >> pattern_col)) ? resolved_fg : resolved_bg;
                    break;
                }
                case 3:
                    if (brush->bd && brush->bd->data && brush->bd->colour_code <= 1) {
                        int pattern_row = (py - brush->yorigin) & 7;
                        int pattern_col = (px - brush->xorigin) & 7;
                        uint8 bits = brush->bd ? brush->bd->data[pattern_row] : brush->pattern[7 - pattern_row];
                        src_colour = (bits & (0x80 >> pattern_col)) ? resolved_bg : resolved_fg;
                    } else if (!brush->bd) {
                        int pattern_row = (py - brush->yorigin) & 7;
                        int pattern_col = (px - brush->xorigin) & 7;
                        uint8 bits = brush->pattern[7 - pattern_row];
                        src_colour = (bits & (0x80 >> pattern_col)) ? resolved_bg : resolved_fg;
                    } else {
                        src_colour = resolved_fg;
                    }
                    break;
                default:
                    src_colour = resolved_fg;
                    break;
                }
            }

            win32_write_pixel(px, py, win32_rop2_apply(opcode, dst_colour, src_colour));
        }
    }
    win32_invalidate_rect(x, y, cx, cy);
}

void ui_screenblt(uint8 opcode, int x, int y, int cx, int cy, int srcx, int srcy)
{
    int dst_x = x;
    int dst_y = y;
    int src_x = srcx;
    int src_y = srcy;
    int width = cx;
    int height = cy;
    int row;
    size_t row_bytes;
    if (!win32_pixels || cx <= 0 || cy <= 0)
        return;

    if (win32_clip_active) {
        if (dst_x < win32_clip_rect.left) {
            src_x += win32_clip_rect.left - dst_x;
            width -= win32_clip_rect.left - dst_x;
            dst_x = win32_clip_rect.left;
        }
        if (dst_y < win32_clip_rect.top) {
            src_y += win32_clip_rect.top - dst_y;
            height -= win32_clip_rect.top - dst_y;
            dst_y = win32_clip_rect.top;
        }
        if (dst_x + width > win32_clip_rect.right)
            width = win32_clip_rect.right - dst_x;
        if (dst_y + height > win32_clip_rect.bottom)
            height = win32_clip_rect.bottom - dst_y;
    }

    if (dst_x < 0) {
        src_x -= dst_x;
        width += dst_x;
        dst_x = 0;
    }
    if (dst_y < 0) {
        src_y -= dst_y;
        height += dst_y;
        dst_y = 0;
    }
    if (src_x < 0) {
        dst_x -= src_x;
        width += src_x;
        src_x = 0;
    }
    if (src_y < 0) {
        dst_y -= src_y;
        height += src_y;
        src_y = 0;
    }

    if (dst_x + width > (int)win32_width)
        width = (int)win32_width - dst_x;
    if (dst_y + height > (int)win32_height)
        height = (int)win32_height - dst_y;
    if (src_x + width > (int)win32_width)
        width = (int)win32_width - src_x;
    if (src_y + height > (int)win32_height)
        height = (int)win32_height - src_y;

    if (width <= 0 || height <= 0)
        return;

    row_bytes = (size_t)width * 4;
    if ((opcode & 0x0f) == 0x0c) {
        if (dst_y > src_y && dst_y < src_y + height) {
            for (row = height - 1; row >= 0; row--) {
                uint8 *dst = win32_pixels + ((size_t)(dst_y + row) * win32_stride) + ((size_t)dst_x * 4);
                uint8 *src = win32_pixels + ((size_t)(src_y + row) * win32_stride) + ((size_t)src_x * 4);
                memmove(dst, src, row_bytes);
            }
        } else {
            for (row = 0; row < height; row++) {
                uint8 *dst = win32_pixels + ((size_t)(dst_y + row) * win32_stride) + ((size_t)dst_x * 4);
                uint8 *src = win32_pixels + ((size_t)(src_y + row) * win32_stride) + ((size_t)src_x * 4);
                memmove(dst, src, row_bytes);
            }
        }
    } else {
        uint32 *source = (uint32 *)malloc((size_t)width * height * sizeof(uint32));
        int column;
        if (!source)
            return;
        for (row = 0; row < height; row++)
            for (column = 0; column < width; column++)
                source[(size_t)row * width + column] = win32_read_pixel(src_x + column, src_y + row);
        for (row = 0; row < height; row++)
            for (column = 0; column < width; column++) {
                uint32 dest = win32_read_pixel(dst_x + column, dst_y + row);
                win32_write_pixel(dst_x + column, dst_y + row,
                                  win32_rop2_apply(opcode, dest,
                                      source[(size_t)row * width + column]));
            }
        free(source);
    }

    win32_invalidate_rect(dst_x, dst_y, width, height);
}

void ui_memblt(uint8 opcode, int x, int y, int cx, int cy, RD_HBITMAP src, int srcx, int srcy)
{
    WIN32_BITMAP *bitmap = (WIN32_BITMAP *)src;
    int dx = x, dy = y, sx = srcx, sy = srcy, width = cx, height = cy;
    int row, column;

    if (!win32_pixels || !bitmap || !bitmap->bits || width <= 0 || height <= 0)
        return;
    if (win32_clip_active) {
        if (dx < win32_clip_rect.left) { sx += win32_clip_rect.left - dx; width -= win32_clip_rect.left - dx; dx = win32_clip_rect.left; }
        if (dy < win32_clip_rect.top) { sy += win32_clip_rect.top - dy; height -= win32_clip_rect.top - dy; dy = win32_clip_rect.top; }
        if (dx + width > win32_clip_rect.right) width = win32_clip_rect.right - dx;
        if (dy + height > win32_clip_rect.bottom) height = win32_clip_rect.bottom - dy;
    }
    if (dx < 0) { sx -= dx; width += dx; dx = 0; }
    if (dy < 0) { sy -= dy; height += dy; dy = 0; }
    if (sx < 0) { dx -= sx; width += sx; sx = 0; }
    if (sy < 0) { dy -= sy; height += sy; sy = 0; }
    if (dx + width > (int)win32_width) width = (int)win32_width - dx;
    if (dy + height > (int)win32_height) height = (int)win32_height - dy;
    if (sx + width > bitmap->width) width = bitmap->width - sx;
    if (sy + height > bitmap->height) height = bitmap->height - sy;
    if (width <= 0 || height <= 0)
        return;

    if ((opcode & 0x0f) == 0x0c && bitmap->source_dc) {
        BitBlt(win32_dc, dx, dy, width, height, bitmap->source_dc, sx, sy, SRCCOPY);
    } else {
        for (row = 0; row < height; row++) {
            for (column = 0; column < width; column++) {
                const uint8 *sp = (const uint8 *)bitmap->bits +
                    ((size_t)(sy + row) * bitmap->width + sx + column) * 4;
                uint32 source_colour = (uint32)sp[0] | ((uint32)sp[1] << 8) |
                                       ((uint32)sp[2] << 16);
                uint32 dest_colour = win32_read_pixel(dx + column, dy + row);
                win32_write_pixel(dx + column, dy + row,
                                  win32_rop2_apply(opcode, dest_colour, source_colour));
            }
        }
    }
    win32_invalidate_rect(dx, dy, width, height);
}

void ui_triblt(uint8 opcode, int x, int y, int cx, int cy, RD_HBITMAP src, int srcx, int srcy, BRUSH *brush, uint32 bgcolour, uint32 fgcolour)
{
    UNUSED(brush);
    UNUSED(bgcolour);
    UNUSED(fgcolour);
    ui_memblt(opcode, x, y, cx, cy, src, srcx, srcy);
}

void ui_line(uint8 opcode, int startx, int starty, int endx, int endy, PEN *pen)
{
    uint32 colour = win32_resolve_colour(pen ? pen->colour : 0);
    UNUSED(opcode);
    if (win32_pixels && (startx == endx || starty == endy)) {
        int left = startx < endx ? startx : endx;
        int right = startx < endx ? endx : startx;
        int top = starty < endy ? starty : endy;
        int bottom = starty < endy ? endy : starty;
        int x;
        int y;

        if (left < 0)
            left = 0;
        if (top < 0)
            top = 0;
        if (right >= (int)win32_width)
            right = (int)win32_width - 1;
        if (bottom >= (int)win32_height)
            bottom = (int)win32_height - 1;

        if (win32_clip_active) {
            if (left < win32_clip_rect.left)
                left = win32_clip_rect.left;
            if (right >= win32_clip_rect.right)
                right = win32_clip_rect.right - 1;
            if (top < win32_clip_rect.top)
                top = win32_clip_rect.top;
            if (bottom >= win32_clip_rect.bottom)
                bottom = win32_clip_rect.bottom - 1;
        }

        if (right < left || bottom < top)
            return;

        if (startx == endx) {
            for (y = top; y <= bottom; y++) {
                uint32 dest = win32_read_pixel(left, y);
                win32_write_pixel(left, y, win32_rop2_apply(opcode, dest, colour));
            }
        } else {
            for (x = left; x <= right; x++) {
                uint32 dest = win32_read_pixel(x, top);
                win32_write_pixel(x, top, win32_rop2_apply(opcode, dest, colour));
            }
        }
    } else {
        HPEN line = CreatePen(PS_SOLID, pen ? pen->width : 1, win32_colour(pen ? pen->colour : 0));
        HGDIOBJ old = SelectObject(win32_dc, line);
        int old_rop2 = SetROP2(win32_dc, (int)opcode + 1);
        MoveToEx(win32_dc, startx, starty, NULL);
        LineTo(win32_dc, endx, endy);
        SetROP2(win32_dc, old_rop2);
        SelectObject(win32_dc, old);
        DeleteObject(line);
    }
    win32_invalidate_rect(startx < endx ? startx : endx,
                          starty < endy ? starty : endy,
                          (startx < endx ? endx - startx : startx - endx) + 2,
                          (starty < endy ? endy - starty : starty - endy) + 2);
}

void ui_rect(int x, int y, int cx, int cy, uint32 colour)
{
    HBRUSH brush = CreateSolidBrush(win32_colour(colour));
    RECT rect;
    if (!win32_clip_rect_area(&x, &y, &cx, &cy)) {
        DeleteObject(brush);
        return;
    }
    rect.left = x;
    rect.top = y;
    rect.right = x + cx;
    rect.bottom = y + cy;
    FillRect(win32_dc, &rect, brush);
    DeleteObject(brush);
    win32_invalidate_rect(x, y, cx, cy);
}

void ui_polygon(uint8 opcode, uint8 fillmode, RD_POINT *point, int npoints, BRUSH *brush, uint32 bgcolour, uint32 fgcolour)
{
    POINT *points = (POINT *)calloc((size_t)npoints, sizeof(*points));
    int i;
    HBRUSH fill = CreateSolidBrush(win32_colour(fgcolour));
    HGDIOBJ old_brush;
    HGDIOBJ old_pen;
    int old_rop2;
    UNUSED(fillmode);
    UNUSED(brush);
    UNUSED(bgcolour);
    for (i = 0; points && i < npoints; i++) {
        if (i == 0) {
            points[i].x = point[i].x;
            points[i].y = point[i].y;
        } else {
            points[i].x = points[i - 1].x + point[i].x;
            points[i].y = points[i - 1].y + point[i].y;
        }
    }
    old_rop2 = SetROP2(win32_dc, (int)opcode + 1);
    old_brush = SelectObject(win32_dc, fill);
    old_pen = SelectObject(win32_dc, GetStockObject(NULL_PEN));
    if (points)
        Polygon(win32_dc, points, npoints);
    SelectObject(win32_dc, old_pen);
    SelectObject(win32_dc, old_brush);
    SetROP2(win32_dc, old_rop2);
    DeleteObject(fill);
    free(points);
    win32_invalidate();
}

void ui_polyline(uint8 opcode, RD_POINT *points, int npoints, PEN *pen)
{
    HPEN line = CreatePen(PS_SOLID, pen ? pen->width : 1, win32_colour(pen ? pen->colour : 0));
    HGDIOBJ old = SelectObject(win32_dc, line);
    POINT *native_points = (POINT *)calloc((size_t)npoints, sizeof(*native_points));
    int i;
    int old_rop2;
    for (i = 0; native_points && i < npoints; i++) {
        if (i == 0) {
            native_points[i].x = points[i].x;
            native_points[i].y = points[i].y;
        } else {
            native_points[i].x = native_points[i - 1].x + points[i].x;
            native_points[i].y = native_points[i - 1].y + points[i].y;
        }
    }
    old_rop2 = SetROP2(win32_dc, (int)opcode + 1);
    if (native_points)
        Polyline(win32_dc, native_points, npoints);
    free(native_points);
    SelectObject(win32_dc, old);
    SetROP2(win32_dc, old_rop2);
    DeleteObject(line);
    win32_invalidate();
}

void ui_ellipse(uint8 opcode, uint8 fillmode, int x, int y, int cx, int cy, BRUSH *brush, uint32 bgcolour, uint32 fgcolour)
{
    HBRUSH fill = CreateSolidBrush(win32_colour(bgcolour));
    HGDIOBJ old_brush;
    UNUSED(opcode);
    UNUSED(fillmode);
    UNUSED(brush);
    UNUSED(fgcolour);
    old_brush = SelectObject(win32_dc, fill);
    Ellipse(win32_dc, x, y, x + cx, y + cy);
    SelectObject(win32_dc, old_brush);
    DeleteObject(fill);
    win32_invalidate_rect(x, y, cx, cy);
}
void ui_draw_glyph(int mixmode, int x, int y, int cx, int cy, RD_HGLYPH glyph, int srcx, int srcy, uint32 bgcolour, uint32 fgcolour)
{
    WIN32_BITMAP *bitmap = (WIN32_BITMAP *)glyph;
    int row;
    int col;
    uint32 draw_colour;
    uint32 draw_bgcolour;

    draw_colour = win32_resolve_colour(fgcolour);
    draw_bgcolour = win32_resolve_colour(bgcolour);

    if (!win32_pixels || !bitmap || !bitmap->bits)
        return;

    for (row = 0; row < cy; row++) {
        for (col = 0; col < cx; col++) {
            int dx = x + col;
            int dy = y + row;
            int sx = srcx + col;
            int sy = srcy + row;

            if (dx < 0 || dy < 0 || dx >= (int)win32_width || dy >= (int)win32_height)
                continue;
            if (win32_clip_active &&
                (dx < win32_clip_rect.left || dy < win32_clip_rect.top ||
                 dx >= win32_clip_rect.right || dy >= win32_clip_rect.bottom))
                continue;
            if (sx < 0 || sy < 0 ||
                sx >= bitmap->width || sy >= bitmap->height)
                continue;

            {
                uint8 *src =
                    (uint8 *)bitmap->bits +
                    ((size_t)sy * bitmap->width + sx) * 4;

                uint8 *dst =
                    win32_pixels +
                    ((size_t)dy * win32_stride) +
                    ((size_t)dx * 4);

                if (src[0] || src[1] || src[2]) {
                    dst[0] = (uint8)(draw_colour & 0xff);
                    dst[1] = (uint8)((draw_colour >> 8) & 0xff);
                    dst[2] = (uint8)((draw_colour >> 16) & 0xff);
                    dst[3] = 0;
                } else if (mixmode == MIX_OPAQUE) {
                    dst[0] = (uint8)(draw_bgcolour & 0xff);
                    dst[1] = (uint8)((draw_bgcolour >> 8) & 0xff);
                    dst[2] = (uint8)((draw_bgcolour >> 16) & 0xff);
                    dst[3] = 0;
                }
            }
        }
    }

    win32_invalidate_rect(x, y, cx, cy);
}

static void win32_draw_text_glyph(uint8 font, uint8 flags, int mixmode,
                                  int *x, int *y,
                                  uint32 bgcolour, uint32 fgcolour,
                                  uint8 *buffer, int buffer_len, int *index,
                                  int *drawn_count)
{
    FONTGLYPH *glyph = cache_get_font(font, buffer[*index]);
    if (!(flags & TEXT2_IMPLICIT_X)) {
        int xyoffset;
        (*index)++;
        if (*index >= buffer_len)
            return;
        xyoffset = buffer[*index];
        if (xyoffset & 0x80) {
            if (*index + 2 >= buffer_len)
                return;
            if (flags & TEXT2_VERTICAL)
                *y += buffer[*index + 1] | (buffer[*index + 2] << 8);
            else
                *x += buffer[*index + 1] | (buffer[*index + 2] << 8);
            *index += 2;
        } else {
            if (flags & TEXT2_VERTICAL)
                *y += xyoffset;
            else
                *x += xyoffset;
        }
    }
    if (glyph && glyph->pixmap) {
        int x1 = *x + glyph->offset;
        int y1 = *y + glyph->baseline;
        ui_draw_glyph(mixmode, x1, y1, glyph->width, glyph->height,
                      glyph->pixmap, 0, 0, bgcolour, fgcolour);
        (*drawn_count)++;
        if (flags & TEXT2_IMPLICIT_X)
            *x += glyph->width;
    }
}

void ui_draw_text(uint8 font, uint8 flags, uint8 opcode, int mixmode, int x, int y, int clipx, int clipy, int clipcx, int clipcy, int boxx, int boxy, int boxcx, int boxcy, BRUSH *brush, uint32 bgcolour, uint32 fgcolour, uint8 *text, uint8 length)
{
    DATABLOB *entry;
    int i, j;
    int drawn_count = 0;
    uint8 *orig_text = text;
    int orig_length = length;
    UNUSED(opcode);
    UNUSED(brush);

    if (boxcx > 1)
        ui_rect(boxx, boxy, boxcx, boxcy, bgcolour);
    else if (mixmode == MIX_OPAQUE)
        ui_rect(clipx, clipy, clipcx, clipcy, bgcolour);

    for (i = 0; i < (int)length;) {
        switch (text[i]) {
        case 0xff:
            if (i + 3 > (int)length) {
                i = length;
                break;
            }
            cache_put_text(text[i + 1], text, text[i + 2]);
            i += 3;
            length -= i;
            text = &text[i];
            i = 0;
            break;
        case 0xfe:
            if (i + 2 > (int)length) {
                i = length;
                break;
            }
            entry = cache_get_text(text[i + 1]);
            if (entry && entry->data) {
                uint8 *edata = (uint8 *)entry->data;
                if ((edata[1] == 0) && !(flags & TEXT2_IMPLICIT_X) && (i + 2 < (int)length)) {
                    if (flags & TEXT2_VERTICAL)
                        y += text[i + 2];
                    else
                        x += text[i + 2];
                }
                for (j = 0; j < (int)entry->size; j++)
                    win32_draw_text_glyph(font, flags, mixmode, &x, &y,
                                          bgcolour, fgcolour,
                                          edata, (int)entry->size, &j,
                                          &drawn_count);
            }
            if (i + 2 < (int)length)
                i += 3;
            else
                i += 2;
            length -= i;
            text = &text[i];
            i = 0;
            break;
        default:
            win32_draw_text_glyph(font, flags, mixmode, &x, &y,
                                  bgcolour, fgcolour,
                                  text, (int)length, &i,
                                  &drawn_count);
            i++;
            break;
        }
    }

    if (drawn_count == 0) {
        int cursor_x = x;
        for (i = 0; i < orig_length; i++) {
            FONTGLYPH *glyph = cache_get_font(font, orig_text[i]);
            if (glyph && glyph->pixmap) {
                ui_draw_glyph(mixmode, cursor_x + glyph->offset,
                              y + glyph->baseline,
                              glyph->width, glyph->height,
                              glyph->pixmap, 0, 0, bgcolour, fgcolour);
                cursor_x += glyph->width;
            }
        }
    }

}
void ui_desktop_save(uint32 offset, int x, int y, int cx, int cy)
{
    int src_x = x;
    int src_y = y;
    int copy_cx = cx;
    int copy_cy = cy;
    int skip_x = 0;
    int skip_y = 0;
    uint8 *src;
    uint32 cache_offset;

    if (!win32_pixels || cx <= 0 || cy <= 0)
        return;
    if (src_x < 0) {
        skip_x = -src_x;
        copy_cx -= skip_x;
        src_x = 0;
    }
    if (src_y < 0) {
        skip_y = -src_y;
        copy_cy -= skip_y;
        src_y = 0;
    }
    if (src_x + copy_cx > (int)win32_width)
        copy_cx = (int)win32_width - src_x;
    if (src_y + copy_cy > (int)win32_height)
        copy_cy = (int)win32_height - src_y;
    if (copy_cx <= 0 || copy_cy <= 0)
        return;

    src = win32_pixels + ((size_t)src_y * win32_stride) + ((size_t)src_x * 4);
    cache_offset = offset * 4 + (uint32)(((size_t)skip_y * (size_t)cx + (size_t)skip_x) * 4);
    cache_put_desktop(cache_offset, copy_cx, copy_cy, win32_stride, 4, src);

}

void ui_desktop_restore(uint32 offset, int x, int y, int cx, int cy)
{
    int dst_x = x;
    int dst_y = y;
    int copy_cx = cx;
    int copy_cy = cy;
    int skip_x = 0;
    int skip_y = 0;
    uint8 *dst;
    uint8 *src;
    int row;
    uint32 cache_offset;

    if (!win32_pixels || cx <= 0 || cy <= 0)
        return;
    if (dst_x < 0) {
        skip_x = -dst_x;
        copy_cx -= skip_x;
        dst_x = 0;
    }
    if (dst_y < 0) {
        skip_y = -dst_y;
        copy_cy -= skip_y;
        dst_y = 0;
    }
    if (dst_x + copy_cx > (int)win32_width)
        copy_cx = (int)win32_width - dst_x;
    if (dst_y + copy_cy > (int)win32_height)
        copy_cy = (int)win32_height - dst_y;
    if (copy_cx <= 0 || copy_cy <= 0)
        return;

    cache_offset = offset * 4 + (uint32)(((size_t)skip_y * (size_t)cx + (size_t)skip_x) * 4);
    src = cache_get_desktop(cache_offset, copy_cx, copy_cy, 4);
    if (!src)
        return;

    dst = win32_pixels + ((size_t)dst_y * win32_stride) + ((size_t)dst_x * 4);
    for (row = 0; row < copy_cy; row++) {
        memcpy(dst + ((size_t)row * win32_stride),
               src + ((size_t)row * copy_cx * 4),
               (size_t)copy_cx * 4);
    }
    win32_invalidate_rect(dst_x, dst_y, copy_cx, copy_cy);

}
void ui_begin_update(void)
{
    win32_in_update = True;
    win32_dirty_valid = False;
}
void ui_end_update(void)
{
    win32_in_update = False;
    if (win32_window && win32_dirty_valid) {
        InvalidateRect(win32_window, &win32_dirty_rect, FALSE);
        win32_dirty_valid = False;
    }
}

void ui_seamless_begin(RD_BOOL hidden) { UNUSED(hidden); }
void ui_seamless_end(void) { }
void ui_seamless_hide_desktop(void) { }
void ui_seamless_unhide_desktop(void) { }
void ui_seamless_toggle(void) { }
void ui_seamless_create_window(unsigned long id, unsigned long group, unsigned long parent, unsigned long flags) { UNUSED(id); UNUSED(group); UNUSED(parent); UNUSED(flags); }
void ui_seamless_destroy_window(unsigned long id, unsigned long flags) { UNUSED(id); UNUSED(flags); }
void ui_seamless_destroy_group(unsigned long id, unsigned long flags) { UNUSED(id); UNUSED(flags); }
void ui_seamless_seticon(unsigned long id, const char *format, int width, int height, int chunk, const char *data, size_t chunk_len) { UNUSED(id); UNUSED(format); UNUSED(width); UNUSED(height); UNUSED(chunk); UNUSED(data); UNUSED(chunk_len); }
void ui_seamless_delicon(unsigned long id, const char *format, int width, int height) { UNUSED(id); UNUSED(format); UNUSED(width); UNUSED(height); }
void ui_seamless_move_window(unsigned long id, int x, int y, int width, int height, unsigned long flags) { UNUSED(id); UNUSED(x); UNUSED(y); UNUSED(width); UNUSED(height); UNUSED(flags); }
void ui_seamless_restack_window(unsigned long id, unsigned long behind, unsigned long flags) { UNUSED(id); UNUSED(behind); UNUSED(flags); }
void ui_seamless_settitle(unsigned long id, const char *title, unsigned long flags) { UNUSED(id); UNUSED(title); UNUSED(flags); }
void ui_seamless_setstate(unsigned long id, unsigned int state, unsigned long flags) { UNUSED(id); UNUSED(state); UNUSED(flags); }
void ui_seamless_syncbegin(unsigned long flags) { UNUSED(flags); }
void ui_seamless_ack(unsigned int serial) { UNUSED(serial); }

RD_BOOL xkeymap_from_locale(const char *locale) { UNUSED(locale); return True; }
FILE *xkeymap_open(const char *filename) { UNUSED(filename); return NULL; }
void xkeymap_init(void) { }
RD_BOOL handle_special_keys(uint32 keysym, unsigned int state, uint32 ev_time, RD_BOOL pressed) { UNUSED(keysym); UNUSED(state); UNUSED(ev_time); UNUSED(pressed); return False; }
key_translation xkeymap_translate_key(uint32 keysym, unsigned int keycode, unsigned int state) { key_translation result; UNUSED(keysym); UNUSED(keycode); UNUSED(state); memset(&result, 0, sizeof(result)); return result; }
void xkeymap_send_keys(uint32 keysym, unsigned int keycode, unsigned int state, uint32 ev_time, RD_BOOL pressed, uint8 nesting) { UNUSED(keysym); UNUSED(keycode); UNUSED(state); UNUSED(ev_time); UNUSED(pressed); UNUSED(nesting); }
uint16 xkeymap_translate_button(unsigned int button, uint16 *input_type) { UNUSED(button); if (input_type) *input_type = 0; return 0; }
char *get_ksname(uint32 keysym) { UNUSED(keysym); return NULL; }
void save_remote_modifiers(uint8 scancode) { UNUSED(scancode); }
void restore_remote_modifiers(uint32 ev_time, uint8 scancode) { UNUSED(ev_time); UNUSED(scancode); }
void ensure_remote_modifiers(uint32 ev_time, key_translation tr) { UNUSED(ev_time); UNUSED(tr); }
unsigned int read_keyboard_state(void) { return win32_get_toggle_state(); }
uint16 ui_get_numlock_state(unsigned int state)
{
    uint16 flags = (uint16)state &
        (KBD_FLAG_NUMLOCK | KBD_FLAG_CAPITAL | KBD_FLAG_SCROLL);
    win32_keyboard_sync_ready = True;
    return flags;
}
void reset_modifier_keys(void) { }
RD_BOOL get_key_state(unsigned int state, uint32 keysym) { UNUSED(state); UNUSED(keysym); return False; }

#endif
