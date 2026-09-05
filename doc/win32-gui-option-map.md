# Win32 GUI option map

This matrix was audited against the current `rdesktop.exe -h`, the option
switch in `rdesktop_frontend_run()`, `RDESKTOP_CONFIG`, and the Win32 backend.
"Parsed" is not treated as proof that the Win32 implementation is functional.

| CLI | Name | Win32 support / actual status | Current GUI | Proposed tab | Config field | Show | Reason |
|---|---|---|---|---|---|---|---|
| `-u` | user name | functional | yes | General | `username` | yes | normal connection setting |
| `-d` | domain | functional | yes | General | `domain` | yes | normal connection setting |
| `-p` | password/prompt | functional; password runtime-only | yes | General | `password`, `prompt_password` | yes | never persist password |
| `-s` | alternate shell | functional | yes | Advanced | `shell` | yes | useful advanced session setting |
| `-c` | working directory | functional with alternate shell | yes | Advanced | `directory` | yes | advanced session setting |
| `-n` | client hostname | functional | yes | Advanced | `hostname` | yes | protocol-visible client identity |
| `-k` | keyboard map | functional | yes | Local Resources | `keymap` | yes | use real IDs from `keymaps/` |
| `-g` | geometry/DPI/position | dimensions functional; initial monitor placement is Win32-aware | resolution and mode | Display | dimensions, DPI, position | yes | raw offsets and DPI remain CLI-only |
| `-f` | fullscreen | functional | yes | Display | `window_mode` | yes | third mutually-exclusive window mode |
| `-b` | force bitmap updates | functional (disables bitmap cache negotiation) | yes, inverse checkbox | Experience | `bitmap_cache` | yes | GUI checkbox enables/disables bitmap caching |
| `-L` | local codepage | functional core setting | yes | Advanced | `codepage` | yes | advanced legacy-server setting |
| `-A` | SeamlessRDP shell | Win32 UI callbacks are stubs | no | none | `seamless_shell` | no | not functional on Win32 |
| `-V` | TLS version | functional where TLS is negotiated | yes | Advanced | `tls_version` | yes | advanced security compatibility |
| `-B` | X11 BackingStore | X11-only | no | none | `own_backstore` | no | no Win32 equivalent |
| `-e` | disable encryption | functional but unsafe compatibility option | no | Advanced | `encryption` | yes, warning | needed by some legacy TS servers |
| `-E` | disable client-to-server encryption | functional but unsafe | no | Advanced | `packet_encryption` | yes, warning | expert-only compatibility |
| `-m` | disable motion events | functional | yes, inverse checkbox | Experience | `send_motion` | yes | affects interactive bandwidth |
| `-M` | local mouse cursor | Win32 cursor creation is incomplete/stubbed | no | none | `local_cursor` | no | cursor completeness backlog |
| `-C` | private colour map | X11 concept; no meaningful Win32 implementation | no | none | `own_colourmap` | no | 32-bit DIB backend does not use X colormaps |
| `-D` | hide decorations | parsed; not a supported normal Win32 GUI mode | no | none | `hide_decorations` | no | conflicts with consolidated modes |
| `-K` | keep WM key bindings | X11-oriented; Win32 keyboard path differs | no | none | `grab_keyboard` | no | not a user-facing Win32 feature |
| `-S` | caption button size | Seamless/single-app option | no | none | `win_button_size` | no | Seamless Win32 is stubbed |
| `-T` | window title | functional | yes | Advanced | `title` | yes | optional advanced customization |
| `-t` | disable remote Ctrl handling | functional in core/input | no | Local Resources | `use_ctrl` | yes | keyboard behavior |
| `-N` | NumLock synchronization | functional and tested | yes | Local Resources | `numlock_sync` | yes | local keyboard option |
| `-X` | embed window ID | X11 embedding | no | none | `embed_window` | no | unsupported by native Win32 frontend |
| `-a` | colour depth | functional for 8/15/16/24/32 | yes | Display | `requested_colour_depth` | yes | display setting |
| `-z` | RDP compression | functional negotiation setting | yes | Advanced | `compression` | yes | advanced bandwidth setting |
| `-x` | RDP5 experience flags | functional; modem/broadband/LAN/hex | presets | Experience | `performance_flags` | yes | GUI exposes modem, broadband and LAN presets |
| `-P` | persistent bitmap cache | core path exists; Win32 persistence requires conservative validation | no | Advanced | `bitmap_cache_persist` | no for now | do not advertise before runtime gate |
| `-R` | retry after network error | functional | yes | Advanced | `retry_on_network_error` | yes | connection behavior |
| `-r sound` | RDPSND | local WinMM functional; off functional; remote server-side mode parsed | yes | Local Resources | `audio_mode` | yes | tested local audio path |
| `-r clipboard` | text CLIPRDR | XP/RDP5 functional; NT4 server limitation remains | yes | Local Resources | `clipboard_enabled`, `clipboard_mode` | yes | label as text clipboard |
| `-r disk` | drive redirection | parser/core present; Win32 product gate not passed | no | none | resources | no | RDPDR backlog |
| `-r comport` | serial redirection | explicitly unsupported on this Win32 target | no | none | resources | no | request fails clearly |
| `-r lptport` | parallel redirection | explicitly unsupported on this Win32 target | no | none | resources | no | request fails clearly |
| `-r printer` | printer redirection | explicitly unsupported on this Win32 target | no | none | resources | no | request fails clearly |
| `-r clientname` | redirected-device client name | only useful with RDPDR | no | none | `rdpdr_clientname` | no | hidden until RDPDR is supported |
| `-r scard` / `-i` / `-o` | smart card | not compiled in current Win32 build | no | none | smart-card fields | no | unavailable feature |
| `-r lspci` | PCI channel | developer/special-purpose | no | none | legacy global | no | not a desktop-client option |
| `-0`, `--admin`, `/admin` | console/admin session | functional | yes | Advanced | `console_session` | yes | useful administrative option |
| `-4` / `-5` | RDP version | functional and required for NT4/XP profiles | yes | Advanced | `rdp_version` | yes | store per server |
| `-v` | verbose logger | functional CLI logging | no | none | logger state | no | GUI diagnostics is a separate bounded input log |
| `-h`, `--help`, `--version` | help/version | CLI-only frontend behavior | no | none | none | no | not a session setting |

## Functional gaps versus a modern desktop RDP client

Not advertised as working: printers, local drives, file clipboard, smart
cards, RDP Gateway, multi-monitor sessions, modern authentication, complete
cursor support, SeamlessRDP on Win32, and newer RDP protocol generations.

## Diagnostic environment audit

| Variable | Classification | Notes |
|---|---|---|
| `RDESKTOP_WIN32_INPUT_DEBUG` | KEEP, developer-only | input event diagnostics |
| `RDESKTOP_WIN32_SCALE_FILTER` | KEEP, advanced developer setting | scaling filter experiment |
| `RDESKTOP_WIN32_CLIPBOARD_DEBUG` | KEEP, developer-only | CLIPRDR adapter diagnostics |
| `RDESKTOP_WIN32_AUDIO_DEBUG` | KEEP, developer-only | WinMM/RDPSND diagnostics |
| `RDESKTOP_DEBUG` | KEEP | existing logger subject selection |

The user-facing Diagnostics tab should offer one bounded logging switch and a
normal/detailed level. It must not surface every environment variable.
