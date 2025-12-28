# Customize toolbar

**Available in version 3.6 or later.**

You can add buttons to a toolbar using `Shortcuts` [advanced setting](Advanced-options-settings.md).

To customize toolbar:

- use `Settings` / `Advanced Options...` menu (or `Ctrl + K` Command Palette, type `adv` to narrow down and select `Advanced Options...` command)
- this opens default .txt editor with advanced settings file
- find `Shortcuts` array and add new shortcut definitions

Example of customization:

```
Shortcuts [
    [
        Cmd = CmdPrevTab
        ToolbarText = Prev Tab
    ]
    [
        Cmd = CmdNextTab
        ToolbarText = Next Tab
    ]
    [
        Name = Send By Mail
        Cmd = CmdSendByEmail
        Key = Shift + M
        ToolbarText = ✉
    ]
    [
        Cmd = CmdNavigateBack
        ToolbarText = ←
    ]
    [
        Cmd = CmdNavigateForward
        ToolbarText = →
    ]
]
```

Explanation:
- `CmdNextTab` is one of the [commands](Commands.md)
- `Next Tab` will be shown in the toolbar

If you provide `Name`, it'll be available in [Command Palette](Command-Palette.md).

See [customizing shortcuts](Customizing-keyboard-shortcuts.md) for more complete docs on `Shortcuts` [advanced setting](Advanced-options-settings.md).

## Using icons

Ideally we would allow SVG icons but that's not easy to implement.

Instead, you can use Unicode symbols supported by Windows' Segoe UI font: http://zuga.net/articles/unicode-all-characters-supported-by-the-font-segoe-ui/

To find a symbol you can search for example for `arrow` and then copy & paste the symbol (e.g. `→`) into settings file.
