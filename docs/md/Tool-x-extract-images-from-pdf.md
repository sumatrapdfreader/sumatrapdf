# Extract images from a PDF

> The command-line tools are provided by `sumatrapdf-tool`, which is installed next to `SumatraPDF.exe`. They only work after SumatraPDF has been installed.

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

You can use SumatraPDF from command-line to extract images embedded in a PDF file.

To extract all images: `sumatrapdf-tool extract input.pdf`

It'll save each image in a file with name `image-<object-number>.png`.

Image extension depends on the type of the file i.e. jpeg images will be saved as `.jpg` file.

## To extract specific images

You can extract only specific images. In a PDF file each image has an object number.

First, you need to find out object numbers of embedded images: `sumatrapdf-tool info -I input.pdf`

The output looks like:

```
Images (9):
        1       (1189 0 R):     [ Flate ] 400x300 8bpc DevRGB (3 0 R)
        1       (1189 0 R):     [ Flate ] 400x300 8bpc DevRGB (6 0 R)
        1       (1189 0 R):     [ Flate ] 602x250 8bpc DevRGB (10 0 R)
        1       (1189 0 R):     [ Flate ] 278x288 8bpc DevRGB (15 0 R)
```

`3`, `6`, `10` and `15` are object numbers.

To extract image 3 and 15: `sumatrapdf-tool extract input.pdf 3 15`

Reference:

- [sumatrapdf-tool extract](Tool-extract.md)
- [sumatrapdf-tool info](Tool-info.md)
