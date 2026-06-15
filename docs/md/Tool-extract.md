# sumatrapdf-tool extract

> The command-line tools are provided by `sumatrapdf-tool`, which is installed next to `SumatraPDF.exe`. They only work after SumatraPDF has been installed.

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Usage:** `sumatrapdf-tool extract [options] file.pdf [object numbers]`

`extract` saves embedded images and font files from a PDF to disk. The files are written to the folder the PDF is in.

You can use [`sumatrapdf-tool info`](Tool-info.md) to get a list of image, font etc. objects present in a PDF file.

You can then use `sumatrapdf-tool extract` to save image objects to a file.

For example `sumatrapdf-tool info -I` returned:

```
Images (73):
        1       (8 0 R):        [ DCT ] 1280x1781 8bpc ICC (3 0 R)
```

`(8 0 R)` is object number of the page with the image.

`(3 0 R)` is object number of the image.

`sumatrapdf-tool extract file.pdf 3` will save image with object number 3 as `image-3.jpg`.

If you don't give any object numbers, it extracts all images and fonts from the file.

## All options

```
Usage: SumatraPDF extract [options] file.pdf [object numbers]
  -p      password
  -r      convert images to rgb
  -a      embed SMasks as alpha channel
  -N      do not use ICC color conversions
```
