# Win32 GUI option map

This matrix was audited against the current `rdesktop.exe -h`, the option
switch in `rdesktop_frontend_run()`, `RDESKTOP_CONFIG`, and the Win32 backend.
"Parsed" is not treated as proof that the Win32 implementation is functional.

| CLI | Name | Win32 support / actual status | Current GUI | Proposed tab | Config field | Show | Reason |
|---|---|---|---|---|---|---|---|
| `-u` | user name | functional | yes | General | `username` | yes | normal connection setting |
| `-d` | domain | functional | yes | General | `domain` | yes | normal connection setting |
| `-p` | password/prompt | functional; password runtime-only | yes | General | `password`, `prompt_password` | yes | never persist password |
| `-s` | alternate shell | functional | no | Advanced | `shell` | yes | useful advanced session setting |
| `-c` | working directory | functional with alternate shell | no | Advanced | `directory` | yes | advanced session setting |
| `-n` | client hostname | functional | no | Advanced | `hostname` | yes | protocol-visible client identity |
| `-k` | keyboard map | functional | yes | Local Resources | `keymap` | yes | use real IDs from `keymaps/` |
| `-g` | geometry/DPI/position | dimensions functional; initial monitor placement is Win32-aware | resolution only | Display | dimensions, DPI, position | yes | GUI exposes resolution/mode; raw offsets remain CLI-only |
| `-f` | fullscreen | functional | yes | Display | `window_mode` | yes | third mutually-exclusive window mode |
| `-b` | force bitmap updates | functional (disables bitmap cache negotiation) | no | Advanced | `bitmap_cache` | yes | troubleshooting/compatibility |
| `-L` | local codepage | functional core setting | no | Advanced | `codepage` | yes | advanced legacy-server setting |
| `-A` | SeamlessRDP shell | Win32 UI callbacks are stubs | no | none | `seamless_shell` | no | not functional on Win32 |
| `-V` | TLS version | functional where TLS is negotiated | no | Advanced | `tls_version` | yes | advanced security compatibility |
| `-B` | X11 BackingStore | X11-only | no | none | `own_backstore` | no | no Win32 equivalent |
| `-e` | disable encryption | functional but unsafe compatibility option | no | Advanced | `encryption` | yes, warning | needed by some legacy TS servers |
| `-E` | disable client-to-server encryption | functional but unsafe | no | Advanced | `packet_encryption` | yes, warning | expert-only compatibility |
| `-m` | disable motion events | functional | no | Experience | `send_motion` | yes | affects interactive bandwidth |
| `-M` | local mouse cursor | Win32 cursor creation is incomplete/stubbed | no | none | `local_cursor` | no | cursor completeness backlog |
| `-C` | private colour map | X11 concept; no meaningful Win32 implementation | no | none | `own_colourmap` | no | 32-bit DIB backend does not use X colormaps |
| `-D` | hide decorations | parsed; not a supported normal Win32 GUI mode | no | none | `hide_decorations` | no | conflicts with consolidated modes |
| `-K` | keep WM key bindings | X11-oriented; Win32 keyboard path differs | no | none | `grab_keyboard` | no | not a user-facing Win32 feature |
| `-S` | caption button size | Seamless/single-app option | no | none | `win_button_size` | no | Seamless Win32 is stubbed |
| `-T` | window title | functional | no | Advanced | `title` | yes | optional advanced customization |
| `-t` | disable remote Ctrl handling | functional in core/input | no | Local Resources | `use_ctrl` | yes | keyboard behavior |
| `-N` | NumLock synchronization | functional and tested | yes | Local Resources | `numlock_sync` | yes | local keyboard option |
| `-X` | embed window ID | X11 embedding | no | none | `embed_window` | no | unsupported by native Win32 frontend |
| `-a` | colour depth | functional for 8/15/16/24/32 | yes | Display | `requested_colour_depth` | yes | display setting |
| `-z` | RDP compression | functional negotiation setting | no | Advanced | `compression` | yes | advanced bandwidth setting |
| `-x` | RDP5 experience flags | functional; modem/broadband/LAN/hex | no | Experience | `performance_flags` | yes | expose presets backed by real flags |
| `-P` | persistent bitmap cache | core path exists; Win32 persistence requires conservative validation | no | Advanced | `bitmap_cache_persist` | no for now | do not advertise before runtime gate |
| `-R` | retry after network error | functional | no | Advanced | `retry_on_network_error` | yes | connection behavior |
| `-r sound` | RDPSND | local WinMM functional; off functional; remote server-side mode parsed | yes | Local Resources | `audio_mode` | yes | tested local audio path |
| `-r clipboard` | text CLIPRDR | XP/RDP5 functional; NT4 server limitation remains | yes | Local Resources | `clipboard_enabled`, `clipboard_mode` | yes | label as text clipboard |
| `-r disk` | drive redirection | parser/core present; Win32 product gate not passed | no | none | resources | no | RDPDR backlog |
| `-r comport` | serial redirection | Win32 implementation exists but not product-gated | no | none | resources | no | keep CLI/developer use only |
| `-r lptport` | parallel redirection | Win32 implementation exists but not product-gated | no | none | resources | no | keep CLI/developer use only |
| `-r printer` | printer redirection | not product-gated on Win32 | no | none | resources | no | printers backlog |
| `-r clientname` | redirected-device client name | only useful with RDPDR | no | none | `rdpdr_clientname` | no | hidden until RDPDR is supported |
| `-r scard` / `-i` / `-o` | smart card | not compiled in current Win32 build | no | none | smart-card fields | no | unavailable feature |
| `-r lspci` | PCI channel | developer/special-purpose | no | none | legacy global | no | not a desktop-client option |
| `-0`, `--admin`, `/admin` | console/admin session | functional | no | Advanced | `console_session` | yes | useful administrative option |
| `-4` / `-5` | RDP version | functional and required for NT4/XP profiles | yes | Advanced | `rdp_version` | yes | store per server |
| `-v` | verbose logger | functional CLI logging | no | Diagnostics | logger state | yes via simple diagnostic model | do not expose raw subjects |
| `-h`, `--help`, `--version` | help/version | CLI-only frontend behavior | no | none | none | no | not a session setting |

## Functional gaps versus a modern desktop RDP client

Not advertised as working: printers, local drives, file clipboard, smart
cards, RDP Gateway, multi-monitor sessions, modern authentication, complete
cursor support, SeamlessRDP on Win32, and newer RDP protocol generations.

## Diagnostic environment audit

| Variable | Classification | Notes |
|---|---|---|
| `RDESKTOP_WIN32_TRACE` | KEEP, developer-only | bounded colour/framebuffer diagnostics |
| `RDESKTOP_WIN32_TRACE_DEEP` | DEVELOPER-ONLY | expensive decoder/order detail |
| `RDESKTOP_WIN32_TRACE_PRIMARY` | DEVELOPER-ONLY | primary-order summaries/oracles |
| `RDESKTOP_WIN32_TRACE_COMPOSE` | DEVELOPER-ONLY | composition trace |
| `RDESKTOP_WIN32_TRACE_PAINT_FLASH` | DEVELOPER-ONLY | intrusive visual diagnostic |
| `RDESKTOP_WIN32_INPUT_DEBUG` | KEEP, developer-only | input event diagnostics |
| `RDESKTOP_WIN32_GUI_PERF` | KEEP, developer-only | GUI timing counters |
| `RDESKTOP_WIN32_PROFILE` | KEEP, developer-only | backend profiling |
| `RDESKTOP_WIN32_PRESENT_TEST` | DEVELOPER-ONLY | deliberately paints test colours |
| `RDESKTOP_WIN32_PRESENT_1TO1` | DEVELOPER-ONLY | presentation experiment |
| `RDESKTOP_WIN32_SCALE_FILTER` | KEEP, advanced developer setting | scaling filter experiment |
| `RDESKTOP_WIN32_TRACE_DRAG` | DEVELOPER-ONLY | drag/redraw diagnostic |
| `RDESKTOP_WIN32_AUDIO_DEBUG` | KEEP, developer-only | WinMM/RDPSND diagnostics |
| `RDESKTOP_DEBUG` | KEEP | existing logger subject selection |

The user-facing Diagnostics tab should offer one bounded logging switch and a
normal/detailed level. It must not surface every environment variable.

