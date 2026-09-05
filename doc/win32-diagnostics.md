# Win32 diagnostics

`RDESKTOP_WIN32_INPUT_DEBUG=1` enables lightweight keyboard synchronization and
non-motion input logging. The flag is read once, and the disabled path performs
no formatting or per-pixel work.

`RDESKTOP_WIN32_SCALE_FILTER` selects the presentation scaling filter. Accepted
values are `auto`, `halftone` and `nearest` (the fallback for other values).

`RDESKTOP_WIN32_CLIPBOARD_DEBUG=1` enables lightweight CLIPRDR adapter logging.
The flag is read once and does not change protocol sequencing.

The Win32 GUI exposes only the permanent input diagnostic. Its read-only log
viewer uses a bounded in-memory buffer and a diagnostic sink callback; the RDP
core never accesses GUI controls. The setting is off by default, remains global
for the current GUI process, and is not stored in server profiles or the
Registry. Logged input records contain event metadata only, never credentials,
clipboard contents, or channel payloads.

The scale filter is a presentation setting rather than a diagnostic switch and
is therefore not exposed in the Diagnostics tab. Clipboard debugging remains a
developer environment flag and is likewise not exposed in the GUI.
