# sumatrapdf-tool trim

or [SumatraPDF.exe trim](Tools.md)

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Usage:** `sumatrapdf-tool trim [options] <input filename>`

A PDF page has fixed dimensions. It can also have MediaBox, CropBox, BleedBox, TrimBox, and ArtBox values that define rectangular areas within the page.

You can see what kind of boxes a PDF page has using [sumatrapdf-tool pages](Tool-pages.md).

Using `trim`, you can trim the area outside a given box, i.e. remove any content outside the box.

With `-e`, it does the opposite, i.e. removes the content inside the box.

The `-m` margin can be given as a single value applied to all sides, two values `<V>,<H>` (vertical, horizontal), or four values `<T>,<R>,<B>,<L>` (top, right, bottom, left). Positive values move inward; negative values move outward.

## Examples

Trim to the MediaBox with a 200 point margin inwards:

`sumatrapdf-tool trim -b mediabox -m 200 -o out.pdf in.pdf`

Trim with 20 points off the top & bottom and 30 points off the left & right:

`sumatrapdf-tool trim -b mediabox -m 20,30 -o out.pdf in.pdf`

Trim with separate margins for each side (top, right, bottom, left):

`sumatrapdf-tool trim -b mediabox -m 10,10,50,20 -o out.pdf in.pdf`

Remove the contents of the ArtBox instead of keeping them:

`sumatrapdf-tool trim -b artbox -e -o out.pdf in.pdf`

## All options

```
Usage: SumatraPDF trim [options] <input filename>
  -b -    Which box to trim to (MediaBox(default), CropBox, BleedBox, TrimBox, ArtBox)
  -m -    Add margins to box (+ve for inwards, -ve outwards).
                  <All> or <V>,<H> or <T>,<R>,<B>,<L>
  -e      Exclude contents of box, rather than include them
  -f      Fallback to mediabox if specified box not available
  -o -    Output file
```
