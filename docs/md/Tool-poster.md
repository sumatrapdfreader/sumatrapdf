# sumatrapdf-tool poster

> The command-line tools are provided by `sumatrapdf-tool`, which is installed next to `SumatraPDF.exe`. They only work after SumatraPDF has been installed.

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Usage:** `sumatrapdf-tool poster [options] input.pdf [output.pdf]`

`poster` splits large PDF pages into many tiles.

Imagine you want to print PDF page in a larger size. `poster` allows you to create a PDF where each PDF page is enlarged and split into multiple pages, you can print separately and then stitch together.

To print a page that is twice as big in both x and y dimensions:

`sumatrapdf-tool poster -x 2 -y 2 input.pdf output.pdf`

`-x` and `-y` are the number of pieces to divide each page into horizontally and vertically, so `-x 2 -y 2` turns every PDF page into 4 pages you can print separately and assemble into a bigger print.

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
