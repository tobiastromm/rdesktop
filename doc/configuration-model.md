# Shared configuration model

`RDESKTOP_CONFIG` is the source-neutral session configuration model. The CLI
and native Win32 GUI populate the same structure, use common validation, and
hand it to the same session runner. Neither frontend duplicates protocol code.

```text
cli_main.c ---------\
                     -> rdesktop_frontend_run() -> shared RDP core
win32_gui_main.c ----/
```

Defaults live in `rdesktop_config_init_defaults()` and validation in
`rdesktop_config_validate_message()`. Parsing of server endpoints and geometry
also belongs to this layer. `rdesktop_config_apply_to_legacy()` is the explicit
compatibility adapter for upstream globals that the historical core still
consumes.

The model preserves the complete upstream geometry representation, colour
depths, keyboard identifiers, credentials, server endpoint, and RDP version.
Win32 adds a source-neutral window-mode enum for client-size, fill-available,
and fullscreen placement. The GUI position supplies only a monitor-selection
hint; protocol code does not know which frontend produced the configuration.

Passwords are runtime-only. They are never part of the Win32 profile schema,
and `rdesktop_config_clear_password()` clears the model after a session or user
close. The versioned profile store persists at most five case-insensitive
server entries containing only non-secret settings.
