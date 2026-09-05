#include "win32_window_geometry.h"

static int
positive(int value)
{
	return value > 0 ? value : 1;
}

void
win32_calculate_window_geometry(const WIN32_GEOMETRY_RECT *work,
				const WIN32_GEOMETRY_RECT *monitor,
				int remote_width, int remote_height,
				int chrome_width, int chrome_height,
				int fill_available, int fullscreen,
				WIN32_WINDOW_GEOMETRY *result)
{
	int available_width;
	int available_height;
	int outer_width;
	int outer_height;

	remote_width = positive(remote_width);
	remote_height = positive(remote_height);
	if (fullscreen)
	{
		result->window = *monitor;
		result->client_width = positive(monitor->right - monitor->left);
		result->client_height = positive(monitor->bottom - monitor->top);
	}
	else
	{
		available_width = positive((work->right - work->left) - chrome_width);
		available_height = positive((work->bottom - work->top) - chrome_height);
		if (fill_available)
		{
			result->client_width = available_width;
			result->client_height = available_height;
		}
		else if (remote_width <= available_width && remote_height <= available_height)
		{
			result->client_width = remote_width;
			result->client_height = remote_height;
		}
		else if ((long long) available_width * remote_height <=
			 (long long) available_height * remote_width)
		{
			result->client_width = available_width;
			result->client_height = (int) ((long long) available_width * remote_height /
						       remote_width);
		}
		else
		{
			result->client_height = available_height;
			result->client_width = (int) ((long long) available_height * remote_width /
						      remote_height);
		}
		outer_width = result->client_width + chrome_width;
		outer_height = result->client_height + chrome_height;
		result->window.left = work->left + ((work->right - work->left) - outer_width) / 2;
		result->window.top = work->top + ((work->bottom - work->top) - outer_height) / 2;
		result->window.right = result->window.left + outer_width;
		result->window.bottom = result->window.top + outer_height;
	}

	if ((long long) result->client_width * remote_height <=
	    (long long) result->client_height * remote_width)
	{
		result->view_width = result->client_width;
		result->view_height = (int) ((long long) result->client_width * remote_height /
					    remote_width);
	}
	else
	{
		result->view_height = result->client_height;
		result->view_width = (int) ((long long) result->client_height * remote_width /
					   remote_height);
	}
	result->view_width = positive(result->view_width);
	result->view_height = positive(result->view_height);
	result->view_x = (result->client_width - result->view_width) / 2;
	result->view_y = (result->client_height - result->view_height) / 2;
}

WIN32_WINDOW_STATE
win32_initial_window_state(int configured_mode)
{
	if (configured_mode == 2)
		return WIN32_WINDOW_STATE_FULLSCREEN;
	if (configured_mode == 1)
		return WIN32_WINDOW_STATE_MAXIMIZED;
	return WIN32_WINDOW_STATE_NORMAL;
}

WIN32_WINDOW_STATE
win32_maximize_window_state(int configured_mode)
{
	return configured_mode == 2 ? WIN32_WINDOW_STATE_FULLSCREEN :
		WIN32_WINDOW_STATE_MAXIMIZED;
}
