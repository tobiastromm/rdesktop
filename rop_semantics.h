/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RDESKTOP_ROP_SEMANTICS_H
#define RDESKTOP_ROP_SEMANTICS_H

#include <stdint.h>

static inline uint32_t
rdp_rop2_apply24(uint8_t rop2, uint32_t dest, uint32_t source)
{
    uint32_t result;
    switch (rop2 & 0x0f) {
    case 0x0: result = 0; break;
    case 0x1: result = ~(dest | source); break;
    case 0x2: result = dest & ~source; break;
    case 0x3: result = ~source; break;
    case 0x4: result = ~dest & source; break;
    case 0x5: result = ~dest; break;
    case 0x6: result = dest ^ source; break;
    case 0x7: result = ~(dest & source); break;
    case 0x8: result = dest & source; break;
    case 0x9: result = ~(dest ^ source); break;
    case 0xa: result = dest; break;
    case 0xb: result = dest | ~source; break;
    case 0xc: result = source; break;
    case 0xd: result = ~dest | source; break;
    case 0xe: result = dest | source; break;
    default: result = 0x00ffffffU; break;
    }
    return result & 0x00ffffffU;
}

static inline uint8_t
rdp_rop2_source_dest_as_rop3(uint8_t rop2)
{
    rop2 &= 0x0f;
    return (uint8_t)(rop2 | (rop2 << 4));
}

static inline uint32_t
rdp_rop3_win32_dwrop(uint8_t rop3)
{
    switch (rop3) {
    case 0xcc: return 0x00cc0020U; /* SRCCOPY */
    case 0x88: return 0x008800c6U; /* SRCAND */
    case 0x66: return 0x00660046U; /* SRCINVERT */
    case 0x33: return 0x00330008U; /* NOTSRCCOPY */
    default: return 0;
    }
}

#endif
/* SPDX-License-Identifier: GPL-3.0-or-later */
