/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "constants.h"
#include "win32_toggle_state.h"

static int
check(int numlock, int capslock, int scrolllock, unsigned int expected)
{
	return win32_toggle_state_to_rdp_flags(numlock, capslock, scrolllock) == expected;
}

int
main(void)
{
	int ok = 1;
	ok &= check(0, 0, 0, 0);
	ok &= check(1, 0, 0, KBD_FLAG_NUMLOCK);
	ok &= check(0, 1, 0, KBD_FLAG_CAPITAL);
	ok &= check(0, 0, 1, KBD_FLAG_SCROLL);
	ok &= check(1, 1, 0, KBD_FLAG_NUMLOCK | KBD_FLAG_CAPITAL);
	ok &= check(1, 1, 1, KBD_FLAG_NUMLOCK | KBD_FLAG_CAPITAL | KBD_FLAG_SCROLL);
	printf("Win32 toggle synchronization: %s\n", ok ? "PASS" : "FAIL");
	return ok ? 0 : 1;
}
