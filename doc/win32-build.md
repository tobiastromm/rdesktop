# Win32 build and packaging

The supported Windows target is `win64-modern-mingw64`, built with the MSYS2
MinGW-w64 x64 toolchain. With that toolchain on `PATH`, run from the repository
root:

```powershell
mingw32-make -f Makefile.win32 clean
mingw32-make -f Makefile.win32
mingw32-make -f Makefile.win32 tests
mingw32-make -f Makefile.win32 package
```

The build tree is `build/win64-modern-mingw64/`: `obj/` contains compiler and
test output, `bin/` contains the two runnable frontends and runtime resources,
and `package/` contains only the distribution ZIP.

`rdesktop.exe` is the traditional CLI frontend. `rdesktop-gui.exe` is a native
Windows GUI-subsystem frontend and does not launch the CLI or synthesize a
command line. Both links reuse the same configuration, session, protocol, and
platform-backend objects.

Packaging stages the non-system MinGW runtime DLL closure, keymaps, and UTF-8
language catalogs. A release check extracts the ZIP into a fresh directory and
runs both frontends with a `PATH` that does not expose the build toolchain.
Generated output is ignored by Git.
