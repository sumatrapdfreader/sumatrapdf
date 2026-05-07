# SumatraPDF trace

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Usage:** `SumatraPDF trace [options] file [pages]`

Trace is advanced option for inspecting the structure of

It prints detailed information about how PDF would be drawn, in XML format.

The output is very large and can look like:

```xml
<?xml version="1.0"?>
<document filename=".\bug-5476-discussion-Andre.Leroy.Dictionnaire.de.pomologie.pdf">
<page number="1" mediabox="0 0 409 684">
<set_default_colorspaces gray="DeviceGray" rgb="DeviceRGB" cmyk="DeviceCMYK" oi="None"/>
<group bbox="0 0 409 684" isolated="1" knockout="0" blendmode="Normal" alpha="1">
    <ignore_text transform=".17986 0 0 -.17986 0 684">
        <span font="Courier" wmode="0" bidi="0" trm="231.11 0 0 231.11">
            <g unicode="P" glyph="P" x="688" y="3173" adv=".6"/>
            <g unicode="O" glyph="O" x="826.666" y="3173" adv=".6"/>
            <g unicode="I" glyph="I" x="965.33206" y="3173" adv=".6"/>
            <g unicode="R" glyph="R" x="1103.998" y="3173" adv=".6"/>
            <g unicode="E" glyph="E" x="1242.6641" y="3173" adv=".6"/>
            <g unicode="S" glyph="S" x="1381.3301" y="3173" adv=".6"/>
        </span>
    </ignore_text>
...
```

## All options

```
Usage: SumatraPDF trace [options] file [pages]
  -p -    password

  -b -    use named page box (MediaBox, CropBox, BleedBox, TrimBox, or ArtBox)

  -W -    page width for EPUB layout
  -H -    page height for EPUB layout
  -S -    font size for EPUB layout
  -U -    file name of user stylesheet for EPUB layout
  -X      disable document styles for EPUB layout

  -d      use display list

  pages   comma separated list of page numbers and ranges
```
