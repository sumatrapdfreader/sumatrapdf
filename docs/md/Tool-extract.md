# SumatraPDF extract

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Usage:** `SumatraPDF extract [options] file.pdf [object numbers]`

You can use [`SumatraPDF info`](Tool-info.md) to get a list of image, font etc. objects present in a PDF file.

You can then use `SumatraPDF extract` to save image objects to a file.

For example `SumatraPDF info -I` returned:

```
Images (73):
        1       (8 0 R):        [ DCT ] 1280x1781 8bpc ICC (3 0 R)
```

`(8 0 R)` is object number of the page with the image.

`(3 0 R)` is object number of the image.

`SumatraPDF extract file.pdf 3` will save image with object number 3 as `image-3.jpg`.

## All options

```
Usage: SumatraPDF extract [options] file.pdf [object numbers]
  -p      password
  -r      convert images to rgb
  -a      embed SMasks as alpha channel
  -N      do not use ICC color conversions
```
