/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "win32_keymap.h"

static int
check(unsigned int vk, unsigned int scan, int extended)
{
	WIN32_KEY_TRANSLATION down = win32_translate_key(vk,
		((unsigned long long) scan << 16) |
		((unsigned long long) extended << 24), 1, 0);
	WIN32_KEY_TRANSLATION up = win32_translate_key(vk,
		((unsigned long long) scan << 16) |
		((unsigned long long) extended << 24) | (1ULL << 31), 0, 0);
	return down.scan == scan && down.extended == extended && !down.release &&
		up.scan == scan && up.extended == extended && up.release;
}

int
main(void)
{
	int ok = 1;
	ok &= check('A', 0x1e, 0);
	ok &= check('0', 0x0b, 0);
	ok &= check(0xa2, 0x1d, 0); /* left Ctrl */
	ok &= check(0xa3, 0x1d, 1); /* right Ctrl */
	ok &= check(0xa4, 0x38, 0); /* left Alt */
	ok &= check(0xa5, 0x38, 1); /* right Alt / AltGr */
	ok &= check(0xa0, 0x2a, 0); /* left Shift */
	ok &= check(0xa1, 0x36, 0); /* right Shift */
	ok &= check(0x5b, 0x5b, 1); /* left Windows */
	ok &= check(0x5c, 0x5c, 1); /* right Windows */
	ok &= check(0x25, 0x4b, 1); /* left arrow */
	ok &= check(0x24, 0x47, 1); /* Home */
	ok &= check(0x2e, 0x53, 1); /* Delete */
	ok &= check(0x60, 0x52, 0); /* keypad 0 */
	ok &= check(0x69, 0x49, 0); /* keypad 9 */
	ok &= check(0x6f, 0x35, 1); /* keypad Divide */
	ok &= check(0x0d, 0x1c, 1); /* keypad Enter */
	printf("Win32 key translation: %s\n", ok ? "PASS" : "FAIL");
	return ok ? 0 : 1;
}
