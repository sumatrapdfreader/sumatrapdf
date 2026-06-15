# sumatrapdf-tool pages

> The command-line tools are provided by `sumatrapdf-tool`, which is installed next to `SumatraPDF.exe`. They only work after SumatraPDF has been installed.

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

`Usage: sumatrapdf-tool pages [options] file.pdf [pages]`

`pages` lists the box dimensions (MediaBox, CropBox, TrimBox etc.), rotation and UserUnit for each page, in XML format. The output looks like:

```xml
<page pagenum="1">
<MediaBox l="0" b="0" r="595.28" t="841.89" />
<CropBox l="85.0394" b="109.134" r="510.241" t="732.756" />
<TrimBox l="113.386" b="137.48" r="481.894" t="704.41" />
<Rotate v="0" />
</page>
```

If you don't supply a `pages` list, all pages are shown.

## All options

```
Usage: SumatraPDF pages [options] file.pdf [pages]
  -p -    password for decryption
  pages   comma separated list of page numbers and ranges
```
