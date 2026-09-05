#include <stdio.h>
#include <stdlib.h>
#include "../win32_window_geometry.h"

static void
check(int condition, const char *message)
{
	if (!condition)
	{
		fprintf(stderr, "FAIL: %s\n", message);
		exit(1);
	}
}

static void
test_work_area(WIN32_GEOMETRY_RECT work, const char *name)
{
	WIN32_GEOMETRY_RECT monitor = { work.left, work.top, work.right, work.bottom + 40 };
	WIN32_WINDOW_GEOMETRY geometry;
	win32_calculate_window_geometry(&work, &monitor, 1024, 768, 16, 39, 0, 0,
					&geometry);
	check(geometry.window.left >= work.left && geometry.window.top >= work.top &&
	      geometry.window.right <= work.right && geometry.window.bottom <= work.bottom,
	      name);
}

int
main(void)
{
	WIN32_GEOMETRY_RECT work = { 0, 0, 1920, 1040 };
	WIN32_GEOMETRY_RECT monitor = { 0, 0, 1920, 1080 };
	WIN32_GEOMETRY_RECT negative_work = { -1920, -40, 0, 1000 };
	WIN32_GEOMETRY_RECT negative_monitor = { -1920, -40, 0, 1040 };
	WIN32_WINDOW_GEOMETRY geometry;

	win32_calculate_window_geometry(&work, &monitor, 1024, 768, 16, 39, 0, 0,
					&geometry);
	check(geometry.client_width == 1024 && geometry.client_height == 768,
	      "client-size exact fit");
	check(geometry.window.left == 440 && geometry.window.top == 116,
	      "client-size centered");

	win32_calculate_window_geometry(&work, &monitor, 2560, 1440, 16, 39, 0, 0,
					&geometry);
	check(geometry.window.left >= 0 && geometry.window.top >= 0 &&
	      geometry.window.right <= 1920 && geometry.window.bottom <= 1040,
	      "oversize client fits work area");
	check(llabs((long long) geometry.client_width * 1440 -
		    (long long) geometry.client_height * 2560) < 2560,
	      "oversize client preserves aspect ratio");

	win32_calculate_window_geometry(&work, &monitor, 1024, 768, 16, 39, 1, 0,
					&geometry);
	check(geometry.window.left == 0 && geometry.window.top == 0 &&
	      geometry.window.right == 1920 && geometry.window.bottom == 1040,
	      "fill uses work area");

	win32_calculate_window_geometry(&negative_work, &negative_monitor, 800, 600,
					16, 39, 0, 1, &geometry);
	check(geometry.window.left == -1920 && geometry.window.top == -40 &&
	      geometry.window.right == 0 && geometry.window.bottom == 1040,
	      "fullscreen uses monitor with negative coordinates");

	test_work_area((WIN32_GEOMETRY_RECT) { 0, 40, 1920, 1080 }, "taskbar top");
	test_work_area((WIN32_GEOMETRY_RECT) { 0, 0, 1880, 1080 }, "taskbar right");
	test_work_area((WIN32_GEOMETRY_RECT) { 40, 0, 1920, 1080 }, "taskbar left");
	test_work_area((WIN32_GEOMETRY_RECT) { 0, 0, 1920, 1040 }, "taskbar bottom");

	check(win32_initial_window_state(0) == WIN32_WINDOW_STATE_NORMAL,
	      "client size starts normal");
	check(win32_initial_window_state(1) == WIN32_WINDOW_STATE_MAXIMIZED,
	      "fill starts maximized");
	check(win32_initial_window_state(2) == WIN32_WINDOW_STATE_FULLSCREEN,
	      "fullscreen starts fullscreen");
	check(win32_maximize_window_state(0) == WIN32_WINDOW_STATE_MAXIMIZED,
	      "client-size maximize enters fill");
	check(win32_maximize_window_state(1) == WIN32_WINDOW_STATE_MAXIMIZED,
	      "fill restore/maximize returns to fill");
	check(win32_maximize_window_state(2) == WIN32_WINDOW_STATE_FULLSCREEN,
	      "fullscreen restore/maximize returns to fullscreen");

	puts("Win32 window geometry and mode transitions: PASS");
	return 0;
}
