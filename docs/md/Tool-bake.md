# sumatrapdf-tool bake

> The command-line tools are provided by `sumatrapdf-tool`, which is installed next to `SumatraPDF.exe`. They only work after SumatraPDF has been installed.

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Usage:** `sumatrapdf-tool bake [options] input.pdf [output.pdf]`

PDF format has annotations and widgets (used in forms) as special kinds of objects.

`bake` is an advanced tool that converts annotations and widgets into regular PDF objects. It bakes PDF form and annotations into static content.

By default it bakes both annotations and form fields into the page content. Use `-A` to keep annotations as annotations (don't bake them) and `-F` to keep form fields interactive.

If you don't specify an output file, it writes to `out.pdf`.

## All options

```
Usage: SumatraPDF bake [options] input.pdf [output.pdf]
  -A      keep annotations
  -F      keep forms
  -O -    comma separated list of output options
```
