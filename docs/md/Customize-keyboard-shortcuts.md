# Customize keyboard shortcuts

**Available in version 3.4 or later.**

You can add new keyboard shortcuts or reassign an existing shortcut to a different [command](Commands.md).

To customize keyboard shortcuts:

- use the `Settings` / `Advanced Options...` menu (or open the Command Palette with `Ctrl + K`, type `adv` to narrow the results, and select the `Advanced Options...` command)
- this opens the advanced settings file in Notepad
- find the `Shortcuts` array and add new shortcut definitions

An example of customization:

```
Shortcuts [
    [
        Cmd = CmdOpen
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

### Restore pre-3.6 Ctrl+Tab (no Smart Tab Switch popup)

**Ver 3.6+** binds `Ctrl + Tab` / `Ctrl + Shift + Tab` to **Smart Tab Switch** (`CmdNextTabSmart` / `CmdPrevTabSmart`), which shows a tab list while Ctrl is held. In 3.5 those keys switched tabs immediately in strip order (`CmdNextTab` / `CmdPrevTab`).

**Ver 3.7+:** the simplest way to get the old behavior back is setting `CtrlTabSimple = true` in advanced settings. You can also rebind the keys:

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

Explanation of the first example:

- by default, SumatraPDF uses the `Ctrl + O` shortcut for the `CmdOpen` (open a file) command. This changes the shortcut to `Alt + O`
- by default, `q` closes the document. Binding it to `CmdNone` disables that built-in shortcut
- **ver 3.6+:** `CmdCreateAnnotHighlight` takes a color argument (`#00ff00` is green). We reassign `a` to create a green highlight annotation (instead of the default yellow)
- **ver 3.6+:** `Name` is optional. If provided, the command will appear in the command palette (`Ctrl + K`)

## Format of the `Key` section

- just a key (such as `a`, `Z`, or `5`), i.e. the letters `a` to `z` and `A` to `Z`, and the numbers `0` to `9`
- modifiers + key. Modifiers are `Shift`, `Alt`, `Ctrl`, and `AltGr` (also `RAlt` / `RightAlt`), e.g. `Alt + F1`, `Ctrl + Shift + Y`, or `AltGr + Return`. On Windows, `AltGr` is the same as `Ctrl + Alt`
- there are some special keys (e.g. `Alt + F3`)
  - `F1` - `F24`
  - `numpad0` - `numpad9` : `0` to `9` but on a numerical keyboard
  - `Delete`, `Backspace`, `Insert`, `Home`, `End`, `Escape`
  - `Left`, `Right`, `Up`, `Down` for arrow keys
  - full list of [special keys](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/src/Accelerators.cpp#L14)
- without modifiers, case matters, i.e. `a` and `A` are different
- with modifiers, use `Shift` to select uppercase, i.e. `Alt + a` is the same as `Alt + A`; use `Alt + Shift + A` to select uppercase `A`

## Global shortcuts

**Ver 3.7+:** prefix `Key` with `Global ` to register a system-wide global shortcut that works even when SumatraPDF does not have focus:

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
- For commands that require a window or document, the command is dispatched to the most recently activated open window. If that window is closed, it falls back to the previously active open window. Triggering the global shortcut does not steal focus or bring background windows to the foreground.
- If SumatraPDF fails to register a global shortcut (e.g. if another program has already registered it), a warning notification is shown.

## Commands

You can see a [full list of commands](Commands.md) ([or view them in the source code](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/src/Commands.h#L9)).

## Escaping in settings values

String values in the advanced settings file use `$` as an escape character.
A literal `$` must be written as `$$`. A lone `$` at the end of a value is
treated as a trailing-whitespace marker, not a dollar sign.

This matters for `CmdCommandPalette` mode arguments: use
`CmdCommandPaletteFavorites` (or `CmdCommandPaletteTOC` for table of contents)
instead of `CmdCommandPalette $` / `CmdCommandPalette %` when binding shortcuts.

## Notes

The changes are applied as soon as you save the settings file, so you can test them without restarting SumatraPDF.

If a custom `Shortcut` doesn't work, it could be caused by an invalid command name or invalid command arguments.

We log information about unsuccessful shortcut parsing, so [check the logs](Debugging-Sumatra.md#getting-logs) if things don't work as expected.
