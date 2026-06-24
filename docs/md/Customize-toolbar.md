# Customize toolbar

**Available in version 3.6 or later.**

You can add buttons to a toolbar using `Shortcuts` [advanced setting](Advanced-options-settings.md). You can also add toolbar buttons for custom external viewers using `ExternalViewers`.

To customize toolbar:

- use `Settings` / `Advanced Options...` menu (or `Ctrl + K` Command Palette, type `adv` to narrow down and select `Advanced Options...` command)
- this opens default .txt editor with advanced settings file
- find `Shortcuts` array and add new shortcut definitions

Example using text labels (`ToolbarText`):

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

See [customize shortcuts](Customize-keyboard-shortcuts.md) for more complete docs on `Shortcuts` [advanced setting](Advanced-options-settings.md).

See [customize external viewers](Customize-external-viewers.md) for adding external-viewer toolbar buttons.

## Using SVG icons

**Ver 3.7+:** set `ToolbarSvgIcon` to an SVG icon. The icon is rendered like built-in toolbar icons (theme text/background colors are applied automatically).

If both `ToolbarSvgIcon` and `ToolbarText` are set, the icon is used.

Use [Tabler Icons](https://tabler.io/icons) SVGs (24×24, `stroke="currentColor"`, `fill="none"`) for best results — the built-in toolbar icons use the same format.

Example:

```
Shortcuts [
    [
        Cmd = CmdNavigateBack
        Name = Back
        ToolbarSvgIcon = <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round"><rect x="0" y="0" width="24" height="24" stroke="none"></rect><path d="M9 14l-4 -4l4 -4" /><path d="M5 10h11a4 4 0 1 1 0 8h-1" /></svg>
    ]
    [
        Cmd = CmdNavigateForward
        Name = Forward
        ToolbarSvgIcon = <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round"><rect x="0" y="0" width="24" height="24" stroke="none"></rect><path d="M15 14l4 -4l-4 -4" /><path d="M19 10h-11a4 4 0 1 0 0 8h1" /></svg>
    ]
]
```

`Name` is used as the toolbar button tooltip. If omitted, the command's default name is used.

## Using Unicode symbols

As an alternative to SVG, you can use Unicode symbols in `ToolbarText` — they are drawn as button labels using the UI font.

Symbols supported by Windows' Segoe UI font: http://zuga.net/articles/unicode-all-characters-supported-by-the-font-segoe-ui/

To find a symbol you can search for example for `arrow` and then copy & paste the symbol (e.g. `→`) into settings file.
