# Managing favorites

Favorites let you save important places in documents and jump back to them later.

## Add a favorite

Press `Ctrl + B` to add the current page to favorites.

You can also use `Ctrl + K` command palette and run `Add Favorite`.

## Favorites sidebar

The favorites sidebar shows favorites saved across documents.

To show or hide it:

- use `Ctrl + K` command palette and run `Toggle Favorites`
- or assign a shortcut to `CmdFavoriteToggle`

When the sidebar is visible, selecting a favorite navigates to that saved place. If the favorite is in another document, SumatraPDF opens that document.

## Show favorites from command palette

Press `Ctrl + K`, then type `$` to show favorites in the command palette.

Favorites from the current document are listed first. Favorites from other documents are shown after them and include the file name.

## Bind favorites command palette to `b`

You can bind `CmdCommandPaletteFavorites` to a key in [advanced settings](Advanced-options-settings.md).

Open `Settings` / `Advanced Options...`, find the `Shortcuts` section and add:

```
Shortcuts [
    [
        Cmd = CmdCommandPaletteFavorites
        Key = b
    ]
]
```

After saving, pressing `b` opens the command palette directly in favorites mode.

You can also use `CmdCommandPalette` with a `$` mode argument, but in settings
files a literal `$` must be escaped as `$$` (e.g. `Cmd = CmdCommandPalette $$`).
Prefer `CmdCommandPaletteFavorites` to avoid that.

See [customize keyboard shortcuts](Customize-keyboard-shortcuts.md) for details.
