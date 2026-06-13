# SumatraPDF audit

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Usage:** `SumatraPDF audit [options] input.pdf [input2.pdf ...]`

See [all-options](#all-options) below.

Audit is an advanced option for inspecting the PDF file format.

It generates an HTML report of operator and space usage in a PDF file. The report has three parts:

- how much of the file is actual object data vs. the overhead of storing those objects (inside and outside of object streams)
- a breakdown of how space is used by the different kinds of objects
- how frequently each graphical operator is used in the content streams

The figures are indicative rather than 100% accurate, and the report assumes some familiarity with the PDF format.

## Generate HTML report about PDF file

`SumatraPDF audit -o report.html .\foo.pdf`

You can then open `report.html` in a browser.

## All options

```
Usage: SumatraPDF audit [options] input.pdf [input2.pdf ...]
  -o -    output file
```
