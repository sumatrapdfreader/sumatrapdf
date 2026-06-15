# Search text in PDF from command line

> The command-line tools are provided by `sumatrapdf-tool`, which is installed next to `SumatraPDF.exe`. They only work after SumatraPDF has been installed.

To search text in `file.pdf`:

`sumatrapdf-tool grep -i foo file.pdf`

`-i` ignores case i.e. does case-insensitive search, which is probably a good default.

Other options:

- `-a` ignore accents (diacritics)

## Search with a regular expression

`sumatrapdf-tool grep -i -G x.* file.pdf`

- `-G` search pattern is a regex
- `-i` you can combine it with case-insensitive flag
- `x.*` is a regular expression that says: every string that starts with `x`

For all options see [sumatrapdf-tool grep](Tool-grep.md).
