# Customize external viewers

SumatraPDF makes it easy for you to open current document in a different program.

For example, if you also have Adobe Reader installed, you can use `File` menu to open PDF document you're viewing in Adobe Reader. We auto-detect some popular apps (Adobe, Foxit, PDF X-Change etc.).

You can add custom viewers using [advanced settings](https://www.sumatrapdfreader.org/settings/settings.html).

To configure an external viewer:

- use `Settings / Advanced Settings...` menu to open configuration file
- modify`ExternalViewers` section

Relevant settings:

```
ExternalViewers [
  [
    CommandLine =
    Name =
    Filter =

    // ver 3.6+:
    Key = 
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
  ]
]
```

`CommandLine` is a full path of executable to open a file with arguments.

Arguments can use special values:

- `"%1"` : will be replaced with a full path of the current document
- `%p` : will be replaced with current page number. Not all viewers support page numbers
- `"%d"` : will be replaced with directory of the current document. Useful for launching file managers. Available in version **3.5** and later

Please make sure to use quotes around file / directory special values (i.e. `"%1"` and `"%d"`) to avoid issues with file paths that have spaces in them.

`Name` will be displayed in `File` menu.

`Filter` restricts which files can be opened by the reader. For example:

- to only activate it for PDF files, use `*.pdf`
- to activate for PNG and JPEG files, use `*.png;*.jpg;*.jpeg`
- to allow all files, use `*` (useful for file managers)

`Key` is optional and is a keyboard shortcut to invoke that viewer. Available in **3.6** and later.
