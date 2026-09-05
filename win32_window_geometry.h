#ifndef RDESKTOP_WIN32_WINDOW_GEOMETRY_H
#define RDESKTOP_WIN32_WINDOW_GEOMETRY_H

typedef struct win32_geometry_rect
{
	int left;
	int top;
	int right;
	int bottom;
}
WIN32_GEOMETRY_RECT;

typedef struct win32_window_geometry
{
	WIN32_GEOMETRY_RECT window;
	int client_width;
	int client_height;
	int view_x;
	int view_y;
	int view_width;
	int view_height;
}
WIN32_WINDOW_GEOMETRY;

typedef enum
{
	WIN32_WINDOW_STATE_NORMAL,
	WIN32_WINDOW_STATE_MAXIMIZED,
	WIN32_WINDOW_STATE_FULLSCREEN
}
WIN32_WINDOW_STATE;

void win32_calculate_window_geometry(const WIN32_GEOMETRY_RECT *work,
				     const WIN32_GEOMETRY_RECT *monitor,
				     int remote_width, int remote_height,
				     int chrome_width, int chrome_height,
				     int fill_available, int fullscreen,
				     WIN32_WINDOW_GEOMETRY *result);
WIN32_WINDOW_STATE win32_initial_window_state(int configured_mode);
WIN32_WINDOW_STATE win32_maximize_window_state(int configured_mode);

#endif
