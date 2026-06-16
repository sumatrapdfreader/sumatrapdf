# Printing

This page collects everything about printing in SumatraPDF: printing from the
window, printing from the command line, every `-print-settings` option and what
it's good for, and recipes for common printing tasks.

If a printout looks wrong (banding, wrong colors, stalls), see
[Reporting printing bugs](Reporting-printing-bugs.md) first — most such problems
come from the printer driver, not SumatraPDF.

## How SumatraPDF prints

SumatraPDF renders each page to an image and sends that image to the printer.
This is reliable across document types (PDF, XPS, CBZ, etc.) but means:

- the print spool can be large and printing can be slower than text-based printers
- print quality follows the printer resolution, not a vector path
- screen-only settings (anti-aliasing, color range, background color) do **not**
  change the printout

## Printing from the window

Press `Ctrl + P` (or toolbar / menu) to open the system print dialog. There you
pick the printer, number of copies, and page range.

### The Advanced options

The system print dialog has an **Advanced** tab (a second tab next to
*General*) with SumatraPDF's own options:

**Print range**

- **All selected pages** – print the chosen range as-is
- **Odd pages only** / **Even pages only** – print only odd/even pages of the
  range (useful for manual two-sided printing)

**Page scaling**

- **Shrink pages to printable area (if necessary)** – default; only scales down
  pages that are too big for the paper, leaves smaller pages at original size
- **Fit pages to printable area** – scale every page up or down so it fills the
  printable area, keeping the aspect ratio
- **Stretch pages to fill paper (ignore aspect ratio)** – fill the paper in both
  dimensions, *not* keeping the aspect ratio (the page is distorted to fit)
- **Use original page sizes** – print at 100%, no scaling (best for forms,
  labels and anything that must print at an exact size)

**Other**

- **Center page horizontally on the paper** – center a page that is smaller than
  the paper; otherwise such a page is aligned to the top-left corner. Useful for
  envelopes or smaller stock fed through a tray that centers the paper
- **Choose paper source by document page size** – let the printer pick the input
  tray whose paper matches the page size (the same idea as Adobe's "Choose paper
  source by PDF page size")
- **Print each page at its document page size (mixed sizes)** – for documents
  whose pages have different sizes, set the paper size per page so each page goes
  to the right paper/tray instead of all pages using the first page's size

### Windows 11: the "Advanced" tab is missing

On Windows 11 (22H2 and later) Windows replaced the classic print dialog with a
new "modern" one that does **not** show application-provided tabs. As a result
SumatraPDF's **Advanced** options (and print preview) don't appear, even though
SumatraPDF still asks for them.

You have two options:

1. **Use command-line printing** (below) — it doesn't depend on the dialog and
   exposes every Advanced option.
2. **Restore the legacy print dialog** for your user account. This affects all
   apps, not just SumatraPDF:

   ```
   reg add "HKCU\Software\Microsoft\Print\UnifiedPrintDialog" /v PreferLegacyPrintDialog /t REG_DWORD /d 1 /f
   ```

   Then sign out and back in. To undo, delete that value.

## Command-line printing

Command-line printing never shows a dialog (unless you ask for one) and exposes
every option, so it works the same on every Windows version. After printing,
SumatraPDF exits; check the process exit code for success/failure.

- `-print-to-default` — print to the system default printer
- `-print-to "<printer-name>"` — print to a named printer, e.g.
  `-print-to "Microsoft Print to PDF"`
- `-print-settings "<list>"` — tweak printing without the dialog (see below)
- `-silent` — suppress error message boxes (for unattended/background printing)
- `-print-dialog` — show the print dialog instead of printing silently
- `-exit-when-done` — used with `-print-dialog` (and `-stress-test`); exit after
  the dialog is dismissed and the document printed

### Exit codes

For unattended/silent printing, the process exit code tells you *why* a print
failed:

| Exit code | Meaning |
| --- | --- |
| `0` | success |
| `2` | couldn't open the file (not found or unsupported format) |
| `3` | the document doesn't allow printing |
| `4` | the printer (named, or default) doesn't exist |
| `5` | the printer driver / device failed |
| `6` | printing is disabled by restriction policy |

With several files, the code is `0` only if all printed, otherwise the category
of the first failure. Anything that goes wrong inside the spooler/driver *after*
the job is submitted (out of paper, printer offline, jam) can't be reported —
SumatraPDF only knows whether the job was handed off.

You can print several files in one command; the settings apply to all of them:

```
SumatraPDF.exe -print-to "HP LaserJet" -print-settings "fit" a.pdf b.pdf c.pdf
```

### `-print-settings` options

The list is comma-separated, e.g. `-print-settings "1-5,odd,fit,monochrome"`.
Order doesn't matter. Available tokens:

**Which pages**

| Option | Meaning |
| --- | --- |
| `5` | a single page |
| `2-6` | a page range |
| `10-8` | a reversed range (prints 10, 9, 8) |
| `last` | the last page |
| `-1`, `-2` | count from the end (`-1` = last page, `-2` = second-to-last) |
| `-3--1` | a range using negatives (here, the last 3 pages) |
| `even` / `odd` | only even / only odd pages of the selected range |

**Scaling and placement**

| Option | Meaning |
| --- | --- |
| `noscale` | print at 100% (no scaling) |
| `shrink` | scale down only pages too big for the paper (default) |
| `fit` | scale every page to fill the printable area, keeping aspect ratio |
| `stretch` | fill the paper in both dimensions, ignoring aspect ratio |
| `center` | center the page horizontally on the paper |

**Orientation**

| Option | Meaning |
| --- | --- |
| `portrait` / `landscape` | rotate the *content* 90° (this is content rotation, **not** the paper orientation, which is set by the printer/driver) |
| `disable-auto-rotation` | don't auto-rotate a wide page 90° to fit the paper; print it in its original orientation |

**Paper and tray**

| Option | Meaning |
| --- | --- |
| `paper=A4` | standard size: `A2`, `A3`, `A4`, `A5`, `A6`, `letter`, `legal`, `tabloid`, `statement`, or a name the printer reports (e.g. `A3 297 x 420 mm`) |
| `paper=76mm x 130mm` | a custom paper size |
| `paper=auto` | set the paper size from each page's own size (for mixed page sizes) |
| `paperkind=<num>` | paper size by Windows `DMPAPER_*` id; use when `paper=A3` doesn't match the driver's paper name |
| `bin=<num or name>` | select the input tray (by number or name) |
| `bin=auto` | let the printer pick the tray whose paper matches the page size |

**Copies, sides and color**

| Option | Meaning |
| --- | --- |
| `3x` | number of copies (here, 3) |
| `simplex` | one-sided |
| `duplex` / `duplexlong` | two-sided, flip on long edge |
| `duplexshort` | two-sided, flip on short edge |
| `color` | force color |
| `monochrome` | force grayscale/black-and-white |

**Output (advanced)**

| Option | Meaning |
| --- | --- |
| `output=<file>` | write to a file (for "print to file" style printers) |
| `docname=<name>` | set the print job name shown in the print queue |
| `ignore-pdf-print-settings` | ignore the print defaults embedded in the PDF (see below) |

> If `paper=A4` doesn't take effect, the driver may report the size under a
> different name. List the exact names your printer accepts and use one of them,
> or fall back to `paperkind=<num>`.

## Print defaults embedded in a PDF

A PDF can carry print hints in its `ViewerPreferences` dictionary. When you
print a **PDF** from the command line, SumatraPDF reads them and uses them as
defaults:

| ViewerPreferences key | Effect |
| --- | --- |
| `PrintScaling` | `/None` prints at original size (no scaling); `/AppDefault` uses SumatraPDF's default |
| `NumCopies` | number of copies |
| `Duplex` | `Simplex`, `DuplexFlipShortEdge` or `DuplexFlipLongEdge` |
| `PickTrayByPDFSize` | when true, pick the input tray by page size (same as `bin=auto`) |

These are **defaults only**. Anything you pass in `-print-settings` overrides the
PDF's value — e.g. `-print-settings "2x"` prints 2 copies even if the PDF asks
for 3. To ignore the PDF's embedded values completely, add
`ignore-pdf-print-settings`:

```
SumatraPDF.exe -print-to-default -print-settings "ignore-pdf-print-settings" document.pdf
```

This applies to command-line printing only; when you print from the window, the
print dialog's own values are used.

## Setting a default page scaling

The page scaling defaults to *shrink*. To change the default used by the print
dialog and command-line printing, set the `PrinterDefaults` advanced setting (in
`Settings → Advanced Options`):

```
PrinterDefaults [
	PrintScale = none
]
```

Accepted values: `shrink` (default), `fit`, `stretch`, `none`.

## Recipes for common tasks

**Print a PDF to the default printer, unattended**

```
SumatraPDF.exe -print-to-default -silent document.pdf
```

**Print at exact size (no scaling) — forms, labels, pre-printed stationery**

```
SumatraPDF.exe -print-to "Label Printer" -print-settings "noscale" label.pdf
```

**Print a small page centered on larger paper (e.g. an envelope)**

```
SumatraPDF.exe -print-to "HP LaserJet" -print-settings "noscale,center" envelope.pdf
```

**Fit every page to the paper, in grayscale, 2 copies**

```
SumatraPDF.exe -print-to "Office Printer" -print-settings "fit,monochrome,2x" report.pdf
```

**Two-sided, long-edge binding, fit to page**

```
SumatraPDF.exe -print-to "Office Printer" -print-settings "fit,duplexlong" report.pdf
```

**Manual two-sided on a one-sided printer** (print odd pages, flip the stack,
then print even pages)

```
SumatraPDF.exe -print-to "Office Printer" -print-settings "odd,fit" report.pdf
SumatraPDF.exe -print-to "Office Printer" -print-settings "even,fit" report.pdf
```

**Print only the last 3 pages**

```
SumatraPDF.exe -print-to-default -print-settings "-3--1" document.pdf
```

**Print a specific paper size from a specific tray**

```
SumatraPDF.exe -print-to "Office Printer" -print-settings "paper=A5,bin=2,fit" booklet.pdf
```

**Print a document with mixed page sizes to matching paper/trays**

```
SumatraPDF.exe -print-to "Multi-tray Printer" -print-settings "paper=auto,bin=auto" mixed.pdf
```

**Print a wide (landscape) page in its original orientation (no 90° rotation)**

```
SumatraPDF.exe -print-to "Receipt Printer" -print-settings "disable-auto-rotation" wide.pdf
```

**"Print" to a PDF file**

```
SumatraPDF.exe -print-to "Microsoft Print to PDF" -print-settings "output=C:\out\result.pdf" input.pdf
```

## See also

- [Command-line arguments](Command-line-arguments.md) — all command-line options
- [Reporting printing bugs](Reporting-printing-bugs.md) — what to include when a
  printout is wrong
