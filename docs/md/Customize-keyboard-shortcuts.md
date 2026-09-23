# Customize keyboard shortcuts

Add new keyboard shortcuts, reassign existing ones to a different [command](Commands.md), or disable them, using the `Shortcuts` [advanced setting](Advanced-options-settings.md).

**Available in version 3.4 or later.**

**Most people use it to rebind a key to a different command.** At a glance:

- **Rebind a key:** `Cmd` + `Key` in the `Shortcuts` array.
- **Disable a built-in shortcut:** bind the key to `CmdNone`.
- **Command with an argument (ver 3.6+):** e.g. `CmdCreateAnnotHighlight #00ff00`.
- **Command Palette entry (ver 3.6+):** add `Name`.
- **Global shortcut (ver 3.7+):** prefix `Key` with `Global ` to work when SumatraPDF isn't focused.
- **Toolbar button:** add `ToolbarText`, see [Customize toolbar](Customize-toolbar.md).

## Add or change a shortcut

1. Open the settings file:
   - Menu: **Settings → Open Advanced Settings File...**
   - [Command Palette](Command-Palette.md): `Ctrl + K`, then `Open Advanced Settings File...`
2. It opens in your default text editor. Find the `Shortcuts` array and add definitions.
3. Save. Changes apply immediately, no restart needed.

Example:

```
Shortcuts [
    [
        Cmd = CmdOpenFile
        Key = Alt + o
    ]
    [
        Cmd = CmdNone
        Key = q
    ]
    [
        Name = Create green highlight
        Cmd = CmdCreateAnnotHighlight #00ff00
        Key = a
    ]
    [
        Cmd = CmdNextTab
        ToolbarText = Next Tab
    ]
]
```

- by default, `Ctrl + O` runs `CmdOpenFile` (open a file). This changes it to `Alt + O`
- by default, `q` closes the document. Binding it to `CmdNone` disables that built-in shortcut
- **ver 3.6+:** `CmdCreateAnnotHighlight` takes a color argument (`#00ff00` is green). This reassigns `a` to create a green highlight annotation (instead of the default yellow)
- **ver 3.6+:** `Name` is optional. If provided, the command appears in the command palette (`Ctrl + K`)

## Add a global shortcut

**Ver 3.7+:** prefix `Key` with `Global ` to register a system-wide shortcut that works even when SumatraPDF does not have focus:

```
Shortcuts [
    [
        Cmd = CmdGoToNextPage
        Key = Global PageDown
    ]
    [
        Cmd = CmdScreenshot
        Key = Global Alt+PrtSc
    ]
]
```

- To avoid hotkey conflicts, global shortcuts are only registered by the first running SumatraPDF instance.
- Commands that need a window or document go to the most recently activated open window. If it's closed, the previously active open window is used. The shortcut does not steal focus or bring background windows to the foreground.
- If registering fails (e.g. another program already registered it), a warning notification is shown.

## Restore pre-3.6 Ctrl+Tab (no Smart Tab Switch popup)

**Ver 3.6+** binds `Ctrl + Tab` / `Ctrl + Shift + Tab` to **Smart Tab Switch** (`CmdNextTabSmart` / `CmdPrevTabSmart`), which shows a tab list while Ctrl is held. In 3.5 those keys switched tabs immediately in strip order (`CmdNextTab` / `CmdPrevTab`).

**Ver 3.7+:** simplest is `CtrlTabSimple = true` in advanced settings. Or rebind the keys:

```
Shortcuts [
    [
        Cmd = CmdNextTab
        Key = Ctrl + Tab
    ]
    [
        Cmd = CmdPrevTab
        Key = Ctrl + Shift + Tab
    ]
]
```

`Ctrl + PageDown` / `Ctrl + PageUp` already run next/prev tab without the popup. More detail: [Tabs and windows](Tabs-and-windows.md#restore-pre-36-ctrltab-no-switcher-popup).

## Tips

- Test changes without restarting: they apply as soon as you save the settings file.
- If a shortcut doesn't work, check the command name and arguments; parse failures are logged, so [check the logs](Debugging-Sumatra.md#getting-logs).
- Use `CmdNone` to free a key before reusing it elsewhere.
- Bind `CmdCommandPaletteFavorites` / `CmdCommandPaletteTOC`, not `CmdCommandPalette $` / `CmdCommandPalette %` (see escaping below).

## Format of the `Key` value

- just a key (such as `a`, `Z`, or `5`), i.e. the letters `a` to `z` and `A` to `Z`, and the numbers `0` to `9`
- modifiers + key. Modifiers are `Shift`, `Alt`, `Ctrl`, and `AltGr` (also `RAlt` / `RightAlt`), e.g. `Alt + F1`, `Ctrl + Shift + Y`, or `AltGr + Return`. On Windows, `AltGr` is the same as `Ctrl + Alt`
- special keys (e.g. `Alt + F3`):
  - `F1` - `F24`
  - `numpad0` - `numpad9` : `0` to `9` on a numerical keyboard
  - `Delete`, `Backspace`, `Insert`, `Home`, `End`, `Escape`
  - `Left`, `Right`, `Up`, `Down` for arrow keys
  - full list of [special keys](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/src/Accelerators.cpp#L14)
- without modifiers, case matters, i.e. `a` and `A` are different
- with modifiers, use `Shift` to select uppercase, i.e. `Alt + a` is the same as `Alt + A`; use `Alt + Shift + A` for uppercase `A`

## Escaping in settings values

String values in the advanced settings file use `$` as an escape character. Write a literal `$` as `$$`. A lone `$` at the end of a value is a trailing-whitespace marker, not a dollar sign.

This matters for `CmdCommandPalette` mode arguments: use `CmdCommandPaletteFavorites` (or `CmdCommandPaletteTOC` for table of contents) instead of `CmdCommandPalette $` / `CmdCommandPalette %` when binding shortcuts.

## Commands

See the [full list of commands](Commands.md) ([or in the source code](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/src/Commands.h#L9)).

## See also

- [Commands](Commands.md) — command ids to bind
- [Customize toolbar](Customize-toolbar.md) — buttons via `ToolbarText` / `ToolbarSvgIcon`
- [Command Palette](Command-Palette.md) — where `Name`d shortcuts show up
- [Tabs and windows](Tabs-and-windows.md) — tab switching keys
- [Advanced settings](Advanced-options-settings.md) — all settings
