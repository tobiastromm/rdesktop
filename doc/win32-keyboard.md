# Win32 keyboard and input

The native Win32 frontend sends the hardware scan byte and E0 flag from the
Windows key message as RDP scancode input. `MapVirtualKey` is used only when a
message does not provide a scan byte.

Left and right Shift, Ctrl and Alt are kept distinct. Extended navigation,
keypad Divide, keypad Enter, AltGr and the Windows keys retain their E0
semantics. LWIN and RWIN are captured only while the session window is in the
foreground. Losing focus releases any tracked Shift, Ctrl, Alt/AltGr or Windows
key, preventing a missing key-up from leaving a remote modifier pressed.

The upstream `-N` option synchronizes local NumLock, CapsLock and ScrollLock
through RDP Synchronize input events during activation, focus regain and lock
key release. Without `-N`, the historical scancode behavior is preserved.

The deterministic Win32 tests cover representative letters, digits, left/right
modifiers, navigation, keypad keys, Windows keys and lock-state flag mapping.
