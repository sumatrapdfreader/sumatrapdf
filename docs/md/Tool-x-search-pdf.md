# Search text in PDF from command line

> Use `sumatrapdf-tool.exe` or [SumatraPDF.exe](Tools.md) with the same arguments.

To search text in `file.pdf`:

`sumatrapdf-tool grep -i foo file.pdf`

`-i` ignores case, i.e. performs a case-insensitive search, which is probably a good default.

Other options:

- `-a` ignore accents (diacritics)

## Search with a regular expression

`sumatrapdf-tool grep -i -G x.* file.pdf`

- `-G` specifies that the search pattern is a regular expression
- `-i` combines it with the case-insensitive flag
- `x.*` is a regular expression that matches every string that starts with `x`

For all options, see [sumatrapdf-tool grep](Tool-grep.md).
