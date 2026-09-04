# Win32 clipboard

The Win32 adapter implements the platform callbacks of the existing upstream
`cliprdr.c` static-channel core. Current scope is bidirectional text through
`CF_UNICODETEXT` and `CF_TEXT`; file clipboard and drive redirection remain
separate features.

Text received as ANSI is converted through the active Windows codepage, while
Unicode is retained as UTF-16LE. Clipboard handles are copied while open and a
successful `SetClipboardData` transfers allocation ownership to Windows.

Notifications use `AddClipboardFormatListener` when available and the clipboard
viewer chain as a legacy fallback. Owner and sequence checks suppress feedback
loops. A local Format List is never sent before CLIPRDR Monitor Ready.

XP validation covers text in both directions, Portuguese accents and multiline
content. The tested NT4/RDP4 endpoint accepts the advertised static channel but
sends no Monitor Ready; the client therefore sends no unilateral Format List.
