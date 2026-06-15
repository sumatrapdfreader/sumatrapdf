# sumatrapdf-tool info

> The command-line tools are provided by `sumatrapdf-tool`, which is installed next to `SumatraPDF.exe`. They only work after SumatraPDF has been installed.

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Usage:** `sumatrapdf-tool info [options] file.pdf [pages]`

PDF file can contain embedded fonts, images, patterns etc. Each object has an object number.

`sumatrapdf-tool info foo.pdf` shows list of objects in the PDF file.

By default it shows all supported types of objects.

Objects are reported per page: the page's object number followed by the matching objects used on it. If you don't supply a `pages` list, all pages are inspected.

You can narrow down what it prints with options:

- `-F` list fonts
- `-I` list images

## All options

```
Usage: SumatraPDF info [options] file.pdf [pages]
  -p -    password for decryption
  -F      list fonts
  -I      list images
  -M      list dimensions
  -P      list patterns
  -S      list shadings
  -X      list form and postscript xobjects
  -Z      list ZUGFeRD info
  pages   comma separated list of page numbers and ranges
```
