# SumatraPDF pages

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

`Usage: SumatraPDF pages [options] file.pdf [pages]`

`pages` shows information about pages in XML format. The output looks like:

```xml
<page pagenum="1">
<MediaBox l="0" b="0" r="595.28" t="841.89" />
<CropBox l="85.0394" b="109.134" r="510.241" t="732.756" />
<TrimBox l="113.386" b="137.48" r="481.894" t="704.41" />
<Rotate v="0" />
</page>
```

## All options

```
Usage: SumatraPDF pages [options] file.pdf [pages]
  -p -    password for decryption
  pages   comma separated list of page numbers and ranges
```
