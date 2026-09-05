/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "rop_semantics.h"

static int check(uint8_t rop3, uint32_t s, uint32_t d, uint32_t expected)
{
    uint8_t rop2 = rop3 & 0x0f;
    uint32_t actual = rdp_rop2_apply24(rop2, d, s);
    if (rdp_rop2_source_dest_as_rop3(rop2) != rop3 || actual != expected) {
        fprintf(stderr, "ROP3 %02x S=%06x D=%06x expected=%06x actual=%06x\n",
                rop3, s, d, expected, actual);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;
    failures += rdp_rop3_win32_dwrop(0xcc) != 0x00cc0020U;
    failures += rdp_rop3_win32_dwrop(0x88) != 0x008800c6U;
    failures += rdp_rop3_win32_dwrop(0x66) != 0x00660046U;
    failures += rdp_rop3_win32_dwrop(0x33) != 0x00330008U;
    failures += check(0xcc, 0x12abef, 0xc055aa, 0x12abef);
    failures += check(0x88, 0xffffff, 0xc0c0c0, 0xc0c0c0);
    failures += check(0x88, 0x5aa55a, 0x3cf0c3, 0x18a042);
    failures += check(0x66, 0x000000, 0xffffff, 0xffffff);
    failures += check(0x66, 0x5aa55a, 0x3cf0c3, 0x665599);
    failures += check(0x33, 0x000000, 0x12abef, 0xffffff);
    failures += check(0x33, 0xffffff, 0x12abef, 0x000000);
    failures += check(0x33, 0xc0c0c0, 0x12abef, 0x3f3f3f);
    if (!failures)
        puts("MemBlt ROP truth tables: PASS (CC, 88, 66, 33)");
    return failures ? 1 : 0;
}
