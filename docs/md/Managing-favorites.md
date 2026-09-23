# Managing favorites

Favorites save important places in documents so you can jump back to them later. You manage them from the **Favorites** menu, the favorites sidebar and the [Command Palette](Command-Palette.md).

**Press `Ctrl + B` to save the current place, then press `Ctrl + K` and type `$` to jump back.**

- **Add a favorite:** saves the current page and scroll position — `Ctrl + B`
- **Favorites sidebar:** lists favorites across documents — `Toggle Favorites`
- **Favorites in Command Palette:** searchable list — `Ctrl + K`, then `$`
- **Favorites button:** toolbar button for the same list — `CmdCommandPaletteFavorites`

## Add a favorite

The favorite stores the page **and** how far you had scrolled on it, so jumping back returns you to the same place on the page, not only the top.

- Menu: **Favorites → Add to favorites**
- Shortcut: `Ctrl + B`
- Command Palette: `Ctrl + K`, then `Add Favorite`

## Jump to a favorite from the Command Palette

Press `Ctrl + K`, then type `$`.

Favorites from the current document are listed first. Favorites from other documents follow and include the file name.

## Show or hide the favorites sidebar

The sidebar shows favorites saved across documents. Selecting one navigates to the saved place; if it's in another document, SumatraPDF opens that document.

- Menu: **Favorites → Show Favorites**
- Command Palette: `Ctrl + K`, then `Toggle Favorites`
- Shortcut: none by default; assign one to `CmdFavoriteToggle`

## Open favorites with a single key

Bind `CmdCommandPaletteFavorites` to a key in [advanced settings](Advanced-options-settings.md). Open **Settings → Open Advanced Settings File...**, find the `Shortcuts` section and add:

```
Shortcuts [
    [
        Cmd = CmdCommandPaletteFavorites
        Key = b
    ]
]
```

After saving, pressing `b` opens the Command Palette directly in favorites mode.

Note: `CmdCommandPalette` with a `$` mode argument also works, but in settings files a literal `$` must be escaped as `$$` (e.g. `Cmd = CmdCommandPalette $$`). Prefer `CmdCommandPaletteFavorites`.

## Add a favorites button to the toolbar

Add `ToolbarText` (or `ToolbarSvgIcon`) to the same shortcut entry. The button opens the same floating list as `Ctrl + K`, then `$`. See [Customize toolbar](Customize-toolbar.md).

```
Shortcuts [
    [
        Cmd = CmdCommandPaletteFavorites
        Name = Favorites
        ToolbarText = $
    ]
]
```

`Name` is the tooltip. The button is added after the built-in toolbar buttons.

## Tips

- Use `Ctrl + B` before scrolling away; the favorite keeps your scroll position, not just the page.
- Use `Ctrl + K`, `$` to reach favorites in other documents; they open the file for you.
- Bind `CmdCommandPaletteFavorites` instead of `CmdCommandPalette $$` to avoid escaping `$`.

## See also

- [Customize keyboard shortcuts](Customize-keyboard-shortcuts.md) — shortcut syntax details
- [Customize toolbar](Customize-toolbar.md) — toolbar buttons
- [Command Palette](Command-Palette.md) — `$` and other modes
