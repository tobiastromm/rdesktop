#include <stdio.h>
#include "win32_clipboard_logic.h"
int main(void)
{
	int ok = 1;
	ok &= win32_clipboard_choose_format(1, 1, 13, 1) == 13;
	ok &= win32_clipboard_choose_format(0, 1, 13, 1) == 1;
	ok &= win32_clipboard_choose_format(0, 0, 13, 1) == 0;
	ok &= !win32_clipboard_may_announce(1, 0);
	ok &= !win32_clipboard_may_announce(0, 1);
	ok &= win32_clipboard_may_announce(1, 1);
	printf("Win32 clipboard logic: %s\n", ok ? "PASS" : "FAIL");
	return ok ? 0 : 1;
}
