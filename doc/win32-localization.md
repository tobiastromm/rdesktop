# Win32 GUI localization

The GUI reads UTF-8 `key=value` catalogs from `resources/lang/`. Built-in
English (`en-US`) is the fallback; catalogs override known keys without
changing configuration semantics.

Supported application locales are `en-US`, `en-GB`, `pt-BR`, and `pt-PT`.
Changing language relabels the live window, tabs, controls, combo entries, and
messages in the same process. The selected tab, control values, password, and
expanded state remain intact. Catalog strings are converted once to UTF-16 and
passed to wide Win32 APIs.

Application language is a global GUI preference under
`HKCU\Software\rdesktop\Win32 GUI`. Keyboard layout is independent, uses the
technical upstream keymap ID, and is stored per server profile. The GUI exposes
`pt-br`, `en-us`, `en-us-intl`, `en-gb`, and `pt`.

The four catalogs are copied beneath `resources/lang/` in both the runnable
binary tree and distribution ZIP. No branding artwork is required by the
current GUI.
