# Win32 performance

The Win32 backend keeps a 32-bit backstore and presents exact-size dirty
rectangles with `BitBlt`; scaled viewports use the separate `StretchBlt` path.
Cached bitmaps retain a selected source DC, with correct object restoration at
destruction. The event loop uses a bounded blocking timeout and contains no
idle spin.

The configuration GUI creates no network or RDP session state before Connect.
During title-bar movement it captures one client-area snapshot, temporarily
parks the child-control HWNDs under an invisible window, and moves a single
surface. `WM_EXITSIZEMOVE` reparents every control, destroys the parking window
and snapshot, then performs one redraw. It does not poll, access the Registry,
rebuild configuration, or relayout controls for each `WM_MOVE`.

Developer diagnostics are opt-in and bounded. Input, clipboard, and WinMM
logging cache their environment switches; disabled hot paths do not format log
records. Performance changes must preserve simultaneous RDP4/8-bpp and
RDP5/16-bpp graphics, input, clipboard, and audio gates.
