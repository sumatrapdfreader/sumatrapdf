# Customize external viewers

SumatraPDF makes it easy for you to open current document in a different program.

For example, if you also have Adobe Reader installed, you can use `File` menu to easily open PDF document you're viewing in Adobe Reader. We auto-detect a bunch of popular apps (Adobe, FoxIt, PDF X-Change etc.).

You can also add custom readers using [advanced settings](https://www.sumatrapdfreader.org/settings/settings.html).

To configure an external reader:

- use `Settings / Advanced Settings...` menu to open configuration file
- modify`ExternalViewers` section

Here are the relevant settings:

```
ExternalViewers [
  [
    CommandLine =
    Name =
    Filter =
  ]
]
```

An example could be:

```
ExternalViewers [
	[
		CommandLine = "C:\Program Files\FoxitReader\FoxitReader.exe" /A page=%p "%1"
		Name = Foxit &Basic 3D view or Review Text
		Filter = *.pdf
	]
]
```

`CommandLine` is a full path of executable that opens a given type of the file.

You can use special values in `CommandLine`:

- `"%1"` : will be replaced with a full path of the current document
- `%p` : replaced with current page number. Not all viewers support page number
- `"%d"` : replaced with directory of the current document. Useful for launching file managers. Available in version **3.5** and later

Please make sure to use quotes around file / directory special values (i.e. `"%1"` and `"%d"`) to avoid issues with file paths that have spaces in them.

`Name` will be displayed in `File` menu.

`Filter` restricts which files can be opened by the reader. For example:

- to only activate it for PDF files, use `*.pdf`
- to activate for PNG and JPEG files, use `*.png;*.jpg;*.jpeg`
- to allow all files, use `*` (useful for file managers)

You can configure up to 10 external viewers.