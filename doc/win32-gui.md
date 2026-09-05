# Native Win32 GUI

## Frontend process model

`rdesktop.exe` is linked as PE subsystem `Windows CUI` and enters through
`main()` in `cli_main.c`. `rdesktop-gui.exe` is linked separately with
`-mwindows`, is PE subsystem `Windows GUI`, and enters through `WinMain()` in
`win32_gui_main.c`. It does not create and subsequently hide a console.

The only executable-specific objects are the two entrypoints. Configuration,
validation and session/core objects are identical below them. Before Connect,
the GUI initializes only `RDESKTOP_CONFIG`, creates native controls and runs a
blocking Win32 `GetMessage`/`TranslateMessage`/`DispatchMessage` loop. Socket,
MCS, security, RDPSND, CLIPRDR session, framebuffer and RDP processing begin
only after the validated configuration is handed to the shared runner.

The original faulty split had no numeric baseline instrumentation; its observed
baseline was a visible console and multi-second grey/unresponsive behavior.
After correcting the subsystem and entrypoint, the bounded measurements are:

| Measurement | Result |
|---|---:|
| Internal entry to interactive first paint | 47 ms |
| Config defaults | 0 ms timer resolution |
| Window creation | 0 ms timer resolution |
| Show to first paint | 32 ms |
| 20-process startup average | 117.2 ms |
| 20-process startup p95 | 217.7 ms |
| 20-process startup maximum | 248.3 ms |
| Five-second movement test | 162 moves, 32.5 moves/s |
| Movement-test CPU time | 140.6 ms over 5.0 s |
| Idle CPU time | 0 ms over 5.0 s |

The optional startup/movement counters are enabled only with
`RDESKTOP_WIN32_GUI_PERF=1`; normal GUI idle performs no diagnostic polling.

`rdesktop-gui.exe` is the native Windows configuration frontend.
`rdesktop.exe` remains the traditional command-line frontend. The GUI does not
construct a command line, call `CreateProcess`, or relaunch `rdesktop.exe`.
Both executables link the same configuration, session, protocol and backend
objects; only `cli_main.o` and `win32_gui_main.o` differ.

The initial GUI exposes server, port, username, masked password, domain,
resolution, fullscreen, windowed placement mode, 8/15/16/24/32-bpp colour depth, explicit RDP4/RDP5,
keyboard layout, NumLock synchronization, text clipboard, and disabled/local
WinMM audio. Drives, printers, ports and file clipboard are not displayed as
working features.

The configuration frontend has a compact Quick Connect state and a persisted
expanded state. The expanded state uses native Win32 tabs: General, Display,
Local Resources, Experience, Advanced, and Diagnostics. Port and RDP version
are Advanced settings; display, keyboard/audio/clipboard, and real RDP5
experience presets are kept on their corresponding pages.

Fullscreen optionally creates an always-visible child connection bar with the
server name, Minimize, Restore, and Disconnect. As a child/tool control it has
no independent taskbar button. `Ctrl+Alt+Enter` exits fullscreen when the bar
is disabled. Restore clears fullscreen/maximized placement and proportionally
fits the negotiated framebuffer into the selected monitor work area.

## Initial session-window geometry

Windowed sessions use the work area (`MONITORINFO.rcWork`) of the monitor on
which the configuration GUI was located, so the taskbar remains visible even
when it is docked on the top, left, right, or bottom. `Client size` requests an
exact remote-sized client area when it fits and otherwise clamps it to the
work area. `Fill available area` uses native `SW_MAXIMIZE`, rather than a
simulated nearly-work-sized rectangle. Thus `IsZoomed()` is true, the caption
button becomes Restore, and Restore returns to the normal centered placement.

The GUI has one mutually-exclusive selector: `Client size`, `Fill available
area`, or `Full screen`. Fullscreen uses `MONITORINFO.rcMonitor` and may cover
the taskbar. CLI `-f` selects the same fullscreen state.

An XP/RDP5 runtime gate confirmed a real Fill Available session with
`IsZoomed() == TRUE`. Its extended frame was `(-8,-8)-(1374,736)` on the
1366-wide test desktop; that extension is the normal invisible DWM resize
frame around a maximized window, not unused client-area margin.

When the client area and remote desktop differ, the existing viewport scales
the framebuffer while preserving aspect ratio and centers the resulting
letterbox. Exact-size clients keep the existing 1:1 presentation fast path.
This choice controls initial geometry only; subsequent manual resize behavior
is unchanged. The pure geometry calculation is covered by
`mingw32-make -f Makefile.win32 test-window-geometry`, including oversized
desktops, all four taskbar edges, and monitors with negative coordinates.

Launching `rdesktop-gui.exe` opens the form. Launching `rdesktop.exe` without a
server prints CLI usage and returns `EX_USAGE`; existing CLI switches retain
their traditional syntax.

Validation errors are shown before connecting. The password is never traced or
persisted. Each of the five case-insensitive MRU entries now has a versioned
server profile containing display, protocol, keyboard, resource and supported
advanced settings. Selecting an MRU entry applies its profile to the controls
and explicitly empties the password. Reusing a server moves its profile to the
top.

Language switching has explicit user-close, connect, and language-reload
outcomes. Reload destroys and reconstructs the controls in an iterative loop
inside the same process, reapplies the current in-memory configuration, and
does not clear the runtime-only password merely because the locale changed.
Passwords are still cleared on user close and after a session.

Validated gates:

- CLI NT4: RDP4, 8 bpp, 1024x768, pt-BR — PASS.
- CLI XP: RDP5, 16 bpp, 1024x768, pt-BR, WinMM — PASS.
- GUI NT4 with the equivalent settings — PASS.
- GUI XP with clipboard and WinMM — PASS.

