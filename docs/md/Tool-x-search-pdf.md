# Search text in PDF from command line

To search text in `file.pdf`:

`SumatraPDF grep -i foo file.pdf`

`-i` ignores case i.e. does case-insensitive search, which is probably a good default.

Other options:

- `-a` ignore accents (diacritics)

## Search with a regular expression

`SumatraPDF grep -i -G x.* file.pdf`

- `-G` search pattern is a regex
- `-i` you can combine it with case-insensitive flag
- `x.*` is a regular expression that says: every string that starts with `x`

For all options see [SumatraPDF grep](Tool-grep.md).
