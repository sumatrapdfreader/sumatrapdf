# sumatrapdf-tool poster

or [SumatraPDF.exe poster](Tools.md)

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Usage:** `sumatrapdf-tool poster [options] input.pdf [output.pdf]`

`poster` splits large PDF pages into many tiles.

Suppose you want to print a PDF page at a larger size. `poster` creates a PDF in which each page is enlarged and split into multiple pages that you can print separately and then stitch together.

To print a page that is twice as big in both x and y dimensions:

`sumatrapdf-tool poster -x 2 -y 2 input.pdf output.pdf`

`-x` and `-y` are the number of pieces into which each page is divided horizontally and vertically, so `-x 2 -y 2` turns every PDF page into four pages that you can print separately and assemble into a larger print.

Use `-m` to add an overlap (in points or as a percentage) between adjacent tiles, which makes it easier to align and glue them together. If you don't specify an output file, it writes to `out.pdf`.

## All options

```
Usage: SumatraPDF poster [options] input.pdf [output.pdf]
  -p -    password
  -m -    margin (overlap) between pages (pts, or %)
  -x      x decimation factor
  -y      y decimation factor
  -r      split right-to-left
```
