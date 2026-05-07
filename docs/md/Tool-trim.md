# SumatraPDF trim

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Usage:** `SumatraPDF trim [options] <input filename>`

A PDF page has a fixed dimension. It can also have MediaBox, CropBox, BleedBox, TrimeBox and ArtBox which define rectangular area within the page.

You can see what kind of boxes a PDF page has using [SumatraPDF pages](Tool-pages.md).

Using `trim` you can trim the area outside of a given box i.e. if there is content outside of the box, it'll be removed.

With `-e` it'll do the opposite i.e. remove the content inside the box.

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
