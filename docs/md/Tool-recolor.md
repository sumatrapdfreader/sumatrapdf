# sumatrapdf-tool recolor

> The command-line tools are provided by `sumatrapdf-tool`, which is installed next to `SumatraPDF.exe`. They only work after SumatraPDF has been installed.

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Usage:** `sumatrapdf-tool recolor [options] <input filename>`

A PDF can use several color spaces. You can use `recolor` to change the colorspace.

To convert a PDF to gray: `sumatrapdf-tool recolor -o output.pdf -c gray input.pdf`

The output colorspace (`-c`) can be `gray` (the default), `rgb` or `cmyk`. Use `-r` to also remove any [output intents](https://en.wikipedia.org/wiki/PDF/X) (embedded color profiles) from the file.

## All options

```
Usage: SumatraPDF recolor [options] <input filename>
  -c -    Output colorspace (gray(default), rgb, cmyk)
  -r      Remove OutputIntent(s)
  -o -    Output file
```
