# SumatraPDF audit

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

## SumatraPDF clean

Usage: `SumatraPDF audit [options] input.pdf [input2.pdf ...]`

See [all-options](#all-options) below.

Audit is an advanced option for inspecting the PDF file format.

It generates HTML file with information about objects in PDF file.

## Generate HTML report about PDF file

`SumatraPDF audit -o report.html .\foo.pdf`

You can then open `report.html` in a browser.

## All options

```
Usage: SumatraPDF audit [options] input.pdf [input2.pdf ...]
  -o -    output file
```
