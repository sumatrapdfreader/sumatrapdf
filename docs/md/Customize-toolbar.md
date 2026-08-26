# Customize toolbar

**Available in version 3.6 or later.**

You can add buttons to the toolbar using the [`Shortcuts` advanced setting](Advanced-options-settings.md). You can also add toolbar buttons for custom external viewers using `ExternalViewers`.

## Choose which buttons are on the toolbar, and in what order

**Ver 3.7+:** `ToolbarCustomLayout` [advanced setting](Advanced-options-settings.md) lists the built-in buttons you want, in the order you want them:

```
ToolbarCustomLayout = CmdFindFirst | PageInfo CmdGoToPrevPage CmdGoToNextPage | CmdZoomOut CmdZoomIn
```

- entries are [command ids](Commands.md), separated by spaces (commas and semicolons work too)
- `|` (or `Separator`) inserts a separator
- `PageInfo` is the page number box (`Page: 3 / 120`); leave it out and it isn't shown at all
- **leaving a button out is how you hide it** — that's the point of the setting: keep the toolbar and fill it with the buttons you actually use
- an empty value (the default) means the standard layout
- names that aren't built-in toolbar buttons are ignored (see the log with `-log`)

Hiding a button doesn't disable the command: it's still available from the menu, the [command palette](Command-Palette.md) and its keyboard shortcut. Buttons you add yourself (`ToolbarText` / `ToolbarSvgIcon`, below) always come after the built-in ones.

This is the standard toolbar written out, a convenient starting point to edit down:

```
ToolbarCustomLayout = CmdOpenFile CmdPrint | PageInfo CmdGoToPrevPage CmdGoToNextPage | CmdNavigateBack CmdNavigateForward | CmdReadAloud | CmdZoomFitWidthAndContinuous CmdZoomFitPageAndSinglePage CmdRotateLeft CmdRotateRight CmdZoomOut CmdZoomIn | CmdFindFirst | CmdToggleEditPDF
```

Some buttons only show when they apply (the Read Aloud button needs `ToolbarShowReadAloud`, Find needs a document that can be searched, rotate needs a document that can be rotated, and Edit PDF needs an editable PDF), so a button you list may still stay hidden. **Edit PDF** toggles a second row of annotation tools. Highlight, underline, squiggly, and strike out are enabled only while text is selected.

## Show, hide or overlay the toolbar

**Ver 3.7+:** the `Toolbar` [advanced setting](Advanced-options-settings.md) controls how the toolbar is shown:

- `show` : toolbar is pinned at the top of the window (the default)
- `hide` : no toolbar
- `overlay` : the toolbar floats over the page, sized to its natural width and horizontally centered. It's hidden by default and only revealed when the mouse moves near the top of the page

Press `F8` (the **Toggle Toolbar** command) to cycle through the modes: show → overlay → hide.

```
Toolbar = overlay
```

To customize the toolbar:

- use the `Settings` / `Advanced Options...` menu (or open the Command Palette with `Ctrl + K`, type `adv` to narrow the results, and select the `Advanced Options...` command)
- this opens the advanced settings file in your default text editor
- find the `Shortcuts` array and add new shortcut definitions

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

If you provide `Name`, it will be available in the [Command Palette](Command-Palette.md).

See [customize shortcuts](Customize-keyboard-shortcuts.md) for more complete docs on `Shortcuts` [advanced setting](Advanced-options-settings.md).

## Command palette on the toolbar

`Ctrl + K` then `$` opens the [command palette](Command-Palette.md) in favorites
mode (a floating list that closes when it loses focus). To put that on the
toolbar, add a shortcut with `ToolbarText` or `ToolbarSvgIcon`:

```
Shortcuts [
    [
        Cmd = CmdCommandPaletteFavorites
        Name = Favorites
        ToolbarText = $
    ]
]
```

The table of contents (document bookmarks) is `Ctrl + K` then `%`, or
`Shift + F12`. For a toolbar button use `CmdCommandPaletteTOC` the same way.

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

To find a symbol, you can search for a term such as `arrow`, then copy and paste the symbol (e.g. `→`) into the settings file.
