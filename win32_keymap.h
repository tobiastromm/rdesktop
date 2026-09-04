/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RDESKTOP_WIN32_KEYMAP_H
#define RDESKTOP_WIN32_KEYMAP_H

typedef struct
{
	unsigned int scan;
	int extended;
	int release;
} WIN32_KEY_TRANSLATION;

static WIN32_KEY_TRANSLATION
win32_translate_key(unsigned int vk, unsigned long long key_data,
		    int pressed, unsigned int fallback_scan)
{
	WIN32_KEY_TRANSLATION result;
	result.scan = (unsigned int) ((key_data >> 16) & 0xff);
	if (result.scan == 0)
		result.scan = fallback_scan & 0xff;
	result.extended = (key_data & (1ULL << 24)) != 0;
	switch (vk)
	{
		case 0x21: case 0x22: case 0x23: case 0x24:
		case 0x25: case 0x26: case 0x27: case 0x28:
		case 0x2d: case 0x2e: case 0x6f:
		case 0x5b: case 0x5c:
			result.extended = 1;
			break;
		default:
			break;
	}
	result.release = !pressed;
	return result;
}

#endif
