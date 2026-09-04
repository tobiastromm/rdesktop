/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RDESKTOP_WIN32_TOGGLE_STATE_H
#define RDESKTOP_WIN32_TOGGLE_STATE_H

static unsigned int
win32_toggle_state_to_rdp_flags(int numlock, int capslock, int scrolllock)
{
	unsigned int flags = 0;
	if (numlock)
		flags |= KBD_FLAG_NUMLOCK;
	if (capslock)
		flags |= KBD_FLAG_CAPITAL;
	if (scrolllock)
		flags |= KBD_FLAG_SCROLL;
	return flags;
}

#endif
