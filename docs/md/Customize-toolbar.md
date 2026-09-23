# Customize toolbar

Choose which buttons the toolbar shows, how it is displayed, and add your own buttons for any [command](Commands.md) or external viewer. Everything is set in the [advanced settings](Advanced-options-settings.md) file.

**Available in version 3.6 or later.**

**Most people use `ToolbarCustomLayout` to trim the toolbar to the buttons they use.** At a glance:

- **Button layout (ver 3.7+):** `ToolbarCustomLayout` picks the built-in buttons and their order.
- **Display mode (ver 3.7+):** `Toolbar = show / overlay / hide`; `F8` cycles through them.
- **Custom buttons:** `ToolbarText` or `ToolbarSvgIcon` in the `Shortcuts` setting adds a button for any command.
- **External viewer buttons:** `ToolbarText` / `ToolbarSvgIcon` in `ExternalViewers`.

## Open the settings file

- Menu: **Settings → Open Advanced Settings File...**
- [Command Palette](Command-Palette.md): `Ctrl + K`, then `Open Advanced Settings File...`

It opens in your default text editor.

## Choose which buttons are on the toolbar, and in what order

**Ver 3.7+:** `ToolbarCustomLayout` lists the built-in buttons you want, in the order you want them:

```
ToolbarCustomLayout = CmdFindFirst | PageInfo CmdGoToPrevPage CmdGoToNextPage | CmdZoomOut CmdZoomIn
```

- entries are [command ids](Commands.md), separated by spaces (commas and semicolons work too)
- `|` (or `Separator`) inserts a separator
- `PageInfo` is the page number box (`Page: 3 / 120`); leave it out and it isn't shown at all
- **leaving a button out is how you hide it** — keep the toolbar and fill it with the buttons you actually use
- an empty value (the default) means the standard layout
- names that aren't built-in toolbar buttons are ignored (see the log with `-log`)

The standard toolbar written out, a starting point to edit down:

```
ToolbarCustomLayout = CmdOpenFile CmdPrint | PageInfo CmdGoToPrevPage CmdGoToNextPage | CmdNavigateBack CmdNavigateForward | CmdToggleReadAloud | CmdZoomFitWidthAndContinuous CmdZoomFitPageAndSinglePage CmdRotateLeft CmdRotateRight CmdZoomOut CmdZoomIn | CmdFindFirst | CmdToggleEditPDF
```

Note: some buttons only show when they apply, so a listed button may stay hidden: Read Aloud needs `ToolbarShowReadAloud`, Find needs a searchable document, rotate needs a rotatable document, Edit PDF needs an editable PDF.

**Edit PDF** toggles a second row of annotation tools. Highlight, underline, squiggly and strike out are enabled only while text is selected.

Some buttons have a drop-down: Zoom In / Zoom Out list the zoom levels, Save in Edit PDF lists the ways to end the session, Read Aloud has voice and speed. Rest the mouse on the button to open it after a short delay; right-click opens it at once if it is not already shown.

## Show, hide or overlay the toolbar

**Ver 3.7+:** the `Toolbar` advanced setting controls how the toolbar is shown:

- `show` : pinned at the top of the window (the default)
- `hide` : no toolbar
- `overlay` : floats over the page, sized to its natural width and horizontally centered. Hidden until the mouse moves near the top of the page

```
Toolbar = overlay
```

To cycle show → overlay → hide:

- Keyboard: `F8`
- Menu: **View → Show Toolbar**
- [Command Palette](Command-Palette.md): `Ctrl + K`, then `Toggle Toolbar` (`CmdToggleToolbar`)

## Add a button for a command

In the `Shortcuts` array, add a definition with `ToolbarText` (a text label):

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

- `CmdNextTab` is one of the [commands](Commands.md)
- `Next Tab` is shown in the toolbar
- if you provide `Name`, it is available in the [Command Palette](Command-Palette.md)

Buttons you add (`ToolbarText` / `ToolbarSvgIcon`) always come after the built-in ones. See [customize shortcuts](Customize-keyboard-shortcuts.md) for full docs on the `Shortcuts` setting.

## Command palette on the toolbar

`Ctrl + K` then `$` opens the [command palette](Command-Palette.md) in favorites mode (a floating list that closes when it loses focus). For a toolbar button, add a shortcut with `ToolbarText` or `ToolbarSvgIcon`:

```
Shortcuts [
    [
        Cmd = CmdCommandPaletteFavorites
        Name = Favorites
        ToolbarText = $
    ]
]
```

The table of contents (document bookmarks) is `Ctrl + K` then `%`, or `Shift + F12`. For a toolbar button use `CmdCommandPaletteTOC` the same way.

## Add a button for an external viewer

See [customize external viewers](Customize-external-viewers.md).

## Using SVG icons

**Ver 3.7+:** set `ToolbarSvgIcon` to an SVG icon. It is rendered like built-in toolbar icons (theme text/background colors are applied automatically). If both `ToolbarSvgIcon` and `ToolbarText` are set, the icon is used.

Use [Tabler Icons](https://tabler.io/icons) SVGs (24×24, `stroke="currentColor"`, `fill="none"`) — the built-in toolbar icons use the same format.

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

`Name` is the button's tooltip. If omitted, the command's default name is used.

## Using Unicode symbols

As an alternative to SVG, use Unicode symbols in `ToolbarText` — they are drawn as button labels using the UI font.

Symbols supported by Windows' Segoe UI font: http://zuga.net/articles/unicode-all-characters-supported-by-the-font-segoe-ui/

To find one, search for a term such as `arrow`, then copy and paste the symbol (e.g. `→`) into the settings file.

## Tips

- Start from the standard `ToolbarCustomLayout` above and delete what you don't use.
- Hiding a button doesn't disable the command: it's still in the menu, the [command palette](Command-Palette.md) and on its keyboard shortcut.
- Use `Toolbar = overlay` to get the vertical space back and still reach the toolbar from the top of the page.
- Set `Name` on a custom button to get a readable tooltip and a Command Palette entry.
- If a `ToolbarCustomLayout` entry has no effect, run with `-log`: unknown names are logged.

## See also

- [Customize keyboard shortcuts](Customize-keyboard-shortcuts.md) — the `Shortcuts` setting in full
- [Customize external viewers](Customize-external-viewers.md) — toolbar buttons for other programs
- [Commands](Commands.md) — command ids to use in buttons
- [Advanced settings](Advanced-options-settings.md) — all settings
