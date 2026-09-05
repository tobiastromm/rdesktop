# Win32 platform architecture

The Win32 port surrounds the preserved rdesktop 1.9.1 protocol core with
Windows frontends and platform adapters. Both executables populate
`RDESKTOP_CONFIG`, use the same validation and legacy-global adapter, and invoke
the same session implementation.

The current target is `win64-modern-mingw64`. Unix/X11 sources and behavior
remain in the tree. Optional upstream subsystems that require POSIX facilities
use explicit Win32 capability boundaries: AddIn processes, serial, parallel,
printer and disk devices, and RDPEAR/Remote Guard fail with a clear error only
when requested. Smart-card support is not compiled into this Windows target.

The graphics backend stores BGRX pixels in a 32-bit DIB backstore. Palette,
15-bpp BGR555, and 16-bpp BGR565 conversion occurs exactly once at the relevant
boundary. Drawing orders preserve clipping, overlap, brush, and raster-operation
semantics; presentation never changes the logical framebuffer representation.

Keyboard, text clipboard, and remote audio reuse the portable input/channel
cores and provide Win32 API adapters. SeamlessRDP, NineGrid, file clipboard,
local drives and optional redirected devices remain unsupported or deferred on
Windows and are not advertised as completed GUI features.
