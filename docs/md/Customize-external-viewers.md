# Customize external viewers

SumatraPDF makes it easy to open the current document in a different program.

For example, if you also have Adobe Reader installed, you can use the `File` menu to open the PDF document you're viewing in Adobe Reader. We auto-detect some popular apps (Adobe, Foxit, PDF-XChange, etc.).

You can add custom viewers using [advanced settings](https://www.sumatrapdfreader.org/settings/settings.html).

To configure an external viewer:

- use `Settings / Advanced Settings...` menu to open configuration file
- modify `ExternalViewers` section

Relevant settings:

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

`CommandLine` is the full path of the executable used to open a file, followed by its arguments.

Arguments can use special values:

- `"%1"` : will be replaced with the full path of the current document
- `%p` : will be replaced with the current page number. Not all viewers support page numbers
- `"%d"` : will be replaced with the directory of the current document. Useful for launching file managers. Available in version **3.5** and later
- `%%` : will be replaced with a literal `%`. Use this to pass a `%` to the external program; for example, `%%d` reaches it as `%d` (handy for tools like `sumatrapdf-tool draw -o page-%d.png`). Available in version **3.7** and later

Please make sure to use quotes around file / directory special values (i.e. `"%1"` and `"%d"`) to avoid issues with file paths that have spaces in them.

`Name` will be displayed in the `File` menu.

`Filter` restricts which files can be opened by the viewer. For example:

- to only activate it for PDF files, use `*.pdf`
- to activate for PNG and JPEG files, use `*.png;*.jpg;*.jpeg`
- to allow all files, use `*` (useful for file managers)

`Key` is optional and is a keyboard shortcut to invoke that viewer. Available in **3.6** and later.

`ToolbarText` is optional and adds a toolbar button for that viewer. `ToolbarSvgIcon` is optional and sets an SVG icon for that toolbar button. If both are set, the SVG icon is used. Available in **3.7** and later.
