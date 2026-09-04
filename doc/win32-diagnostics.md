# Win32 diagnostics

`RDESKTOP_WIN32_INPUT_DEBUG=1` enables lightweight keyboard synchronization and
non-motion input logging. The flag is read once, and the disabled path performs
no formatting or per-pixel work.

`RDESKTOP_WIN32_SCALE_FILTER` selects the presentation scaling filter. Accepted
values are `auto`, `halftone` and `nearest` (the fallback for other values).
