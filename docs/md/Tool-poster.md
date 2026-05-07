# SumatraPDF poster

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Usage:** `SumatraPDF poster [options] input.pdf [output.pdf]`

`poster` splits large PDF pages into many tiles.

Imagine you want to print PDF page in a larger size. `poster` allows you to create a PDF where each PDF page is enlarged and split into multiple pages, you can print separately and then stitch together.

To print a page that is twice as big in both x and y dimensions:

`SumatraPDF poster -x 2 -y 2 input.pdf output.pdf`

For every PDF page it'll create 4 pages you can print separately and assemble a bigger print from.

## All options

```
Usage: SumatraPDF poster [options] input.pdf [output.pdf]
  -p -    password
  -m -    margin (overlap) between pages (pts, or %)
  -x      x decimation factor
  -y      y decimation factor
  -r      split right-to-left
```
