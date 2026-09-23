# Customize external viewers

Open the current document in a different program from the `File` menu. SumatraPDF auto-detects some popular apps (Adobe, Foxit, PDF-XChange, etc.) and you can add your own with the `ExternalViewers` [advanced setting](https://www.sumatrapdfreader.org/settings/settings.html).

**Most people use it to open the PDF they're viewing in another reader, e.g. Adobe Reader.** At a glance:

- **Open in another program:** pick it in the `File` menu.
- **Add a viewer:** an entry in `ExternalViewers` with `CommandLine`, `Name`, `Filter`.
- **Keyboard shortcut (ver 3.6+):** `Key`.
- **Toolbar button (ver 3.7+):** `ToolbarText` or `ToolbarSvgIcon`.

## Open the document in another program

Use the `File` menu and pick the viewer. Viewers you add appear there under their `Name`.

## Add a custom viewer

1. Menu: **Settings → Open Advanced Settings File...** to open the configuration file.
2. Modify the `ExternalViewers` section.

Available fields:

```
ExternalViewers [
  [
    CommandLine =
    Name =
    Filter =

    // ver 3.6+:
    Key =

    // ver 3.7+:
    ToolbarText =
    ToolbarSvgIcon =
  ]
]
```

Example:

```
ExternalViewers [
  [
    CommandLine = "C:\Program Files\FoxitReader\FoxitReader.exe" /A page=%p "%1"
    Name = Foxit &Reader
    Filter = *.pdf
    Key = Ctrl + m
    ToolbarText = Foxit
  ]
]
```

## Add a shortcut or toolbar button

- `Key` (ver 3.6+) is an optional keyboard shortcut that invokes the viewer.
- `ToolbarText` (ver 3.7+) is optional and adds a toolbar button for the viewer.
- `ToolbarSvgIcon` (ver 3.7+) is optional and sets an SVG icon for that button. If both are set, the SVG icon is used. See [Customize toolbar](Customize-toolbar.md#using-svg-icons) for the icon format.

## Tips

- Always quote file / directory values (`"%1"`, `"%d"`) so paths with spaces work.
- Use `Filter = *` and `"%d"` to open the document's folder in a file manager.
- Use `%p` to open the viewer at the current page, if it supports page numbers.
- Use `%%` to pass a literal `%`, e.g. `sumatrapdf-tool draw -o page-%%d.png`.

## Reference

### `CommandLine`

Full path of the executable used to open a file, followed by its arguments. Arguments can use special values:

- `"%1"` : the full path of the current document
- `%p` : the current page number. Not all viewers support page numbers
- `"%d"` : the directory of the current document. Useful for launching file managers. Available in version **3.5** and later
- `%%` : a literal `%`. Use this to pass a `%` to the external program; for example, `%%d` reaches it as `%d` (handy for tools like `sumatrapdf-tool draw -o page-%d.png`). Available in version **3.7** and later

Use quotes around file / directory special values (i.e. `"%1"` and `"%d"`) to avoid issues with paths that have spaces in them.

### `Name`

Displayed in the `File` menu.

### `Filter`

Restricts which files the viewer can open:

- only PDF files: `*.pdf`
- PNG and JPEG files: `*.png;*.jpg;*.jpeg`
- all files: `*` (useful for file managers)

## See also

- [Customize toolbar](Customize-toolbar.md) — toolbar buttons and SVG icons
- [Customize keyboard shortcuts](Customize-keyboard-shortcuts.md) — key names and modifiers
- [Advanced settings](https://www.sumatrapdfreader.org/settings/settings.html) — all settings
