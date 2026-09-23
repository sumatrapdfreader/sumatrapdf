# Command Palette

The Command Palette is a searchable list that runs any SumatraPDF command, and also opens files, switches tabs and jumps to pages, table of contents entries, favorites, annotations and settings. Open it with `Ctrl + K` or **View → Command Palette**.

**Available in version 3.4 or later.**

**Press `Ctrl + K`, type a few letters, press `Enter` to run a command.** Type a prefix character to switch what the list shows:

- **Commands:** all SumatraPDF functionality — the default view, or `>`
- **File history:** open a previously opened file — `#`
- **Tabs:** switch to another tab — `@`
- **Thumbnails:** navigate pages with thumbnails — `&`
- **Table of contents:** jump to an entry in the current document — `%`
- **Favorites:** jump to a favorite (current document's first, then others) — `$`
- **Annotations:** jump to an annotation in the current PDF — `*`
- **Settings:** change a setting — `=`
- **Combined view:** everything in one list — `:`

## Run a command

![Command Palette](img/command-palette-commands-423d.png)

- Press `Ctrl + K`, or use **View → Command Palette** (`CmdCommandPalette`).
- Enter text to narrow down the list of matches.
- `Up` / `Down` arrows navigate between matches.
- `Page Up` / `Page Down` jump a page of matches.
- `Home` / `End` go to the first / last match when the caret is already at the start / end of the query (`Ctrl + Home` / `Ctrl + End` always do).
- `Ctrl + A` selects the query.
- `Ctrl + C` copies the selected query text (`Ctrl + V` pastes into the query).
- `Enter` executes the selected match (or double-click it with the mouse).
- `Escape` closes the window (or click outside it).

A command can be listed under more than one name so that a different wording finds it: **Navigate Files in Folder...** is also **Browse Files In Folder...**, and **Advanced Settings...** is also **Advanced Options...**.

## Switch to another tab

Type `@` to switch between open tabs:

![Command Palette](img/command-palette-tabs-7b7d.png)

## Open a file from history

Type `#` to open a file from the list of previously opened files:

![Command Palette](img/command-palette-file-history-0f51.png)

## Jump to a table of contents entry

- Type `%`.
- Press `Shift + F12` (`CmdCommandPaletteTOC`).
- Add `CmdCommandPaletteTOC` as a [toolbar button](./Customize-toolbar.md#command-palette-on-the-toolbar).

The list shows the fully expanded table of contents of the current document, indented to reflect the tree hierarchy. The entry closest to the current page is pre-selected. Type to filter, then `Enter` (or double-click) to navigate to the selected entry.

## Jump to a page with thumbnails

Type `&` to open the thumbnail grid for the current document. The current page is selected initially. Use the arrow keys, `Home`, `End`, mouse, or the scrollbar to navigate; `Enter` opens the selected page and `Escape` closes the grid.

## Jump to a favorite

- Type `$`.
- Bind a keyboard shortcut to the `CmdCommandPaletteFavorites` [command](./Commands.md) to open the palette directly in favorites mode (see [Managing favorites](./Managing-favorites.md)).
- Add the same command as a [toolbar button](./Customize-toolbar.md#command-palette-on-the-toolbar).

Favorites (bookmarks) of the current document are listed first, followed by favorites of other documents (labeled with the file name). Type to filter, then `Enter` (or double-click) to navigate to the selected favorite. Picking a favorite in another document opens that document.

## Jump to an annotation

Type `*` to jump to an annotation in the current PDF. `* Annotations` appears in the mode row once annotations have been collected and the document has at least one. Collection runs in the background the first time the Command Palette or **Find Annotation** needs them, and the result is reused (including when there are none).

Rows match the **Find Annotation** list: type, contents, page number. Filter with the same syntax (`:t=text`, `:a=kjk`, `:c+`, plain words). `Enter` or double-click selects the annotation and jumps to it.

## Change a setting

Type `=` to change a setting without opening [Advanced Settings](./Advanced-options-settings.md).

- Every setting holding a single value is listed by its dotted name, with its current value on the right.
- A value that isn't the default is shown in bold, and those settings are listed first.
- Type to filter by name or by value.
- The selected setting's description, the same as in Advanced Settings, is shown under the list.

`Enter` on a `true` / `false` setting toggles it. Any other setting asks for a value: the query becomes `=<name> = <value>`, and

- a setting restricted to a fixed set of values (`Toolbar`, `Scrollbars`, `PrintScale`, ...) lists them; type to narrow the list, `Enter` picks one
- any other setting takes what you type, starting from the current value

The change takes effect immediately, exactly as if it had been saved from Advanced Settings - no restart. The palette stays open and shows the settings again with the same setting selected, so several can be changed in a row; `Esc` closes it. `Esc` while a value is being asked for goes back to the settings instead, also with that setting selected.

You can also type the whole thing at once, e.g. `=ZoomIncrement = 25`; the name can be the last part of a dotted setting (`Units` for `FixedPageUI.PageGrid.Units`) as long as only one setting ends with it.

Note: compact settings like `WindowMargin` are only editable in Advanced Settings; settings holding a list only in the settings file (**Open Advanced Settings File...**).

## Show everything in one list

Type `:` for a combined view (replicates 3.4 and 3.5 behavior):

![Command Palette](img/command-palette-all-8d8c.png)

## Tips

- Use the palette to run commands that have no default shortcut, e.g. `Toggle Free Pan`.
- Type `=` and a value to find settings by their current value.
- Change several settings in a row: the palette stays open after each change.
- Press `Shift + F12` to go straight to the table of contents.

## Replicate 3.4 and 3.5 behavior

In 3.6, we changed the default command palette view from combined (`:`) to commands only (`>`).

You can replicate pre-3.6 behavior by adding to [advanced settings](./Advanced-options-settings.md):

```
Shortcuts [
  [
    Key = Ctrl + K
    Command = CmdCommandPalette :
  ]
]
```

The default command for the `Ctrl + K` keyboard shortcut is [`CmdCommandPalette`](./Commands.md). This changes it to `CmdCommandPalette :`, which replicates 3.4/3.5 behavior.

## See also

- [Commands](./Commands.md) — every command id, for shortcuts and toolbar buttons
- [Customize toolbar](./Customize-toolbar.md#command-palette-on-the-toolbar) — palette buttons on the toolbar
- [Managing favorites](./Managing-favorites.md)
- [Advanced settings](./Advanced-options-settings.md)
