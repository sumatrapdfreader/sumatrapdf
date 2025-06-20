# Customizing keyboard shortcuts

**Available in version 3.4 or later.**

You can add new keyboard shortcuts or re-assign existing shortcut to a different [command](Commands.md).

To customize keyboard shortcuts:

- use `Settings` / `Advanced Options...` menu (or `Ctrl + K` Command Palette, type `adv` to narrow down and select `Advanced Options...` command)
- this opens a notepad with advanced settings file
- find `Shortcuts` array and add new shortcut definitions

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
        Cmd = CmdCreateAnnotHighlight #00ff00 openedit
        Key = a
    ]
    [
        Cmd = CmdNextTab
        ToolbarText = Next Tab
    ]
]
```

Explanation:

- by default SumatraPDF has `Ctrl + O` shortcut for `CmdOpen` (open a file) command. This changes the shortcut to `Alt + o`
- by default `q` closes the document. By binding it to `CmdNone` we can disable a built-in shortcut
- **ver 3.6+:**: `CmdCreateAnnotHighlight` takes a color argument (`#00ff00` is green) and boolean `openedit` argument. We re-assign `a` to create a highlight annotation with green color (different from default yellow) and to open annotations edit window (`openedit` boolean argument)
- **ver 3.6+:**: `Name` is optional. If given, the command will show up in command palette (`Ctrl + K`)

## Format of `Key` section:

- just a key (like `a`, `Z`, `5`) i.e. letters `a` to `z`, `A` to `Z`, and numbers `0` to `9`
- modifiers + key. Modifiers are: `Shift`, `Alt`, `Ctrl` e.g. `Alt + F1`, `Ctrl + Shift + Y`
- there are some special keys (e.g. `Alt + F3`)
  - `F1` - `F24`
  - `numpad0` - `numpad9` : `0` to `9` but on a numerical keyboard
  - `Delete`, `Backspace`, `Insert`, `Home`, `End`, `Escape`
  - `Left`, `Right`, `Up`, `Down` for arrow keys
  - full list of [special keys](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/src/Accelerators.cpp#L14)
- without modifiers, case do matter i.e. `a` and `A` are different
- with modifiers, use `Shift` to select upper-case i.e. `Alt + a` is the same as `Alt + A` , use `Alt + Shift + A` to select the upper-case `A`

## Commands

You can see a [full list of commands](Commands.md) ([or in the source code](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/src/Commands.h#L9))

## Notes

The changes are applied right after you save settings file so that you can test changes without restarting SumatraPDF.

If a custom `Shortcut` doesn't work it could be caused by invalid command name or invalid command arguments.

We log information about unsuccessful parsing of a shortcut so [check the logs](Debugging-Sumatra.md#getting-logs) if things don't work as expected.
