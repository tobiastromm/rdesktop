#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define main rdesktop_program_main
#include "../rdesktop.h"
#undef main

int g_pstcache_fd[8];
static int bitmap_destroyed;
static int glyph_destroyed;
static int cursor_destroyed;

void logger(log_subject_t subject, log_level_t level, char *format, ...)
{ UNUSED(subject); UNUSED(level); UNUSED(format); }
void *xmalloc(int size) { return malloc((size_t) size); }
void xfree(void *memory) { free(memory); }
RD_BOOL pstcache_load_bitmap(uint8 id, uint16 idx)
{ UNUSED(id); UNUSED(idx); return False; }
void pstcache_touch_bitmap(uint8 id, uint16 idx, uint32 stamp)
{ UNUSED(id); UNUSED(idx); UNUSED(stamp); }
void ui_destroy_bitmap(RD_HBITMAP bitmap)
{ bitmap_destroyed++; free(bitmap); }
void ui_destroy_glyph(RD_HGLYPH glyph)
{ glyph_destroyed++; free(glyph); }
void ui_destroy_cursor(RD_HCURSOR cursor)
{ cursor_destroyed++; free(cursor); }

static void check(int condition, const char *message)
{
	if (!condition)
	{
		fprintf(stderr, "FAIL: %s\n", message);
		exit(1);
	}
}

int main(void)
{
	uint8 desktop[4] = { 1, 2, 3, 4 };
	BRUSHDATA brush;

	memset(&brush, 0, sizeof(brush));
	cache_put_bitmap(0, 1, malloc(1));
	cache_put_bitmap_with_dimensions(1, 0x7fff, malloc(1), 2, 2);
	cache_put_font(0, 1, 0, 0, 1, 1, malloc(1));
	cache_put_text(1, "text", 4);
	cache_put_cursor(1, malloc(1));
	brush.data = xmalloc(8);
	cache_put_brush_data(1, 1, &brush);
	cache_put_desktop(0, 1, 1, 4, 4, desktop);

	cache_reset_state();
	check(bitmap_destroyed == 2, "bitmap and volatile bitmap destroyed");
	check(glyph_destroyed == 1, "glyph destroyed");
	check(cursor_destroyed == 1, "cursor destroyed");
	check(cache_get_bitmap(0, 1) == NULL, "bitmap slot cleared");
	check(cache_get_bitmap(1, 0x7fff) == NULL, "volatile slot cleared");
	check(cache_get_font(0, 1) == NULL, "font slot cleared");
	check(cache_get_text(1)->data == NULL, "text slot cleared");
	check(cache_get_brush_data(1, 1)->data == NULL, "brush slot cleared");
	check(cache_get_desktop(0, 1, 1, 4)[0] == 0, "desktop cache cleared");

	cache_reset_state();
	check(bitmap_destroyed == 2 && glyph_destroyed == 1 && cursor_destroyed == 1,
	      "second reset is idempotent");
	puts("Session cache reset: PASS");
	return 0;
}
