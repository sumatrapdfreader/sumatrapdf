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

### Printing part of a page

To print only a fragment of a page (a detail of a drawing, one table):

1. Select the area as a rectangle: hold `Ctrl` and drag with the left mouse
   button (or drag with the right mouse button).
2. Right-click the selection and choose **Print Selection...**, or press
   `Ctrl + P` and pick **Selection** under _Page Range_ in the print dialog.
3. To print the fragment at its real size choose **Actual size (1:1)**
   under _Advanced_; **Shrink** / **Fit** scale it to the paper instead.

The selection prints on one sheet, at the top-left (or centered with
**Center page horizontally**). Only rectangular selections can be printed;
a text selection is not offered. The Windows 11 print dialog can't print a
selection, so SumatraPDF uses the classic dialog for it.

### The Advanced options

The system print dialog has an **Advanced** tab (a second tab next to
_General_) with SumatraPDF's own options:

**Print range**

- **All selected pages** – print the chosen range as-is
- **Odd pages only** / **Even pages only** – print only odd/even pages of the
  range (useful for manual two-sided printing)

**Page scaling**

- **Shrink pages to printable area** – default; only scales down
  pages that are too big for the paper, leaves smaller pages at original size
- **Fit pages to printable area** – scale every page up or down so it fills the
  printable area, keeping the aspect ratio
- **Stretch pages to fill paper** – fill the paper in both
  dimensions, _not_ keeping the aspect ratio (the page is distorted to fit)
- **Actual size (1:1)** – print at 100%, no scaling
  (best for forms, labels, technical drawings and anything that must print at
  an exact size). For images the size comes from the resolution
  recorded in the file; see [Printing at actual size](#printing-at-actual-size-11)

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
new "modern" one that does **not** show application-provided tabs. SumatraPDF
puts the most used **Advanced** options in its **More settings** pane instead:
**Page scaling** (the same four choices as above), **Center page horizontally**
and **Rotate printout**. The paper-source options are not available there.

For the rest you have two options:

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
- `/p` — Adobe Reader alias for `-print-dialog`
- `/t <file> <printer>` — Adobe Reader silent print (`/t <printer>` if the file is
  already on the command line); optional driver/port args are ignored
- `-exit-when-done` — used with `-print-dialog` (and `-stress-test`); exit after
  the dialog is dismissed and the document printed

### Exit codes

For unattended/silent printing, the process exit code tells you _why_ a print
failed:

| Exit code | Meaning                                                  |
| --------- | -------------------------------------------------------- |
| `0`       | success                                                  |
| `2`       | couldn't open the file (not found or unsupported format) |
| `3`       | the document doesn't allow printing                      |
| `4`       | the printer (named, or default) doesn't exist            |
| `5`       | the printer driver / device failed                       |
| `6`       | printing is disabled by restriction policy               |

With several files, the code is `0` only if all printed, otherwise the category
of the first failure. Anything that goes wrong inside the spooler/driver _after_
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

| Option         | Meaning                                                      |
| -------------- | ------------------------------------------------------------ |
| `5`            | a single page                                                |
| `2-6`          | a page range                                                 |
| `10-8`         | a reversed range (prints 10, 9, 8)                           |
| `last`         | the last page                                                |
| `-1`, `-2`     | count from the end (`-1` = last page, `-2` = second-to-last) |
| `-3--1`        | a range using negatives (here, the last 3 pages)             |
| `even` / `odd` | only even / only odd pages of the selected range             |

**Scaling and placement**

| Option    | Meaning                                                                                                                                               |
| --------- | ----------------------------------------------------------------------------------------------------------------------------------------------------- |
| `noscale` | print at 100% (no scaling), i.e. actual size / 1:1                                                                                                    |
| `dpi=<n>` | resolution to assume for the document, e.g. `dpi=300` for a 300 dpi scan whose file says otherwise (or nothing); decides the size `noscale` prints at |
| `shrink`  | scale down only pages too big for the paper (default)                                                                                                 |
| `fit`     | scale every page to fill the printable area, keeping aspect ratio                                                                                     |
| `stretch` | fill the paper in both dimensions, ignoring aspect ratio                                                                                              |
| `center`  | center the page horizontally on the paper                                                                                                             |

**Orientation**

| Option                                    | Meaning                                                                                                                                                                                                        |
| ----------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `portrait` / `landscape`                  | rotate the _content_ 90° (this is content rotation, **not** the paper orientation, which is set by the printer/driver)                                                                                         |
| `disable-auto-rotation`                   | don't auto-rotate a wide page 90° to fit the paper; print it in its original orientation                                                                                                                       |
| `rotate=90` / `rotate=180` / `rotate=270` | rotate the printout by extra degrees, to fix a wrong orientation (e.g. `rotate=180` for upside-down output on virtual printers). In the window, use the **Rotate printout** dropdown on the Advanced print tab |

**Paper and tray**

| Option               | Meaning                                                                                                                                        |
| -------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| `paper=A4`           | standard size: `A2`, `A3`, `A4`, `A5`, `A6`, `letter`, `legal`, `tabloid`, `statement`, or a name the printer reports (e.g. `A3 297 x 420 mm`) |
| `paper=76mm x 130mm` | a custom paper size                                                                                                                            |
| `paper=auto`         | set the paper size from each page's own size (for mixed page sizes)                                                                            |
| `paperkind=<num>`    | paper size by Windows `DMPAPER_*` id; use when `paper=A3` doesn't match the driver's paper name                                                |
| `bin=<num or name>`  | select the input tray (by number or name)                                                                                                      |
| `bin=auto`           | let the printer pick the tray whose paper matches the page size                                                                                |

Use `SumatraPDF.exe -list-printers` to list installed printers, paper names, `paperkind=` IDs and tray names.

**Copies, sides and color**

| Option                  | Meaning                                                     |
| ----------------------- | ----------------------------------------------------------- |
| `3x`                    | number of copies (here, 3)                                  |
| `collate` / `nocollate` | collate copies (`1,2,3 / 1,2,3`) or not (`1,1 / 2,2 / 3,3`) |
| `simplex`               | one-sided                                                   |
| `duplex` / `duplexlong` | two-sided, flip on long edge                                |
| `duplexshort`           | two-sided, flip on short edge                               |
| `color`                 | force color                                                 |
| `monochrome`            | force grayscale/black-and-white                             |

**Output (advanced)**

| Option                      | Meaning                                                   |
| --------------------------- | --------------------------------------------------------- |
| `output=<file>`             | write to a file (for "print to file" style printers)      |
| `docname=<name>`            | set the print job name shown in the print queue           |
| `ignore-pdf-print-settings` | ignore the print defaults embedded in the PDF (see below) |

> If `paper=A4` doesn't take effect, the driver may report the size under a
> different name. List the exact names your printer accepts and use one of them,
> or fall back to `paperkind=<num>`.

## Print defaults embedded in a PDF

A PDF can carry print hints in its `ViewerPreferences` dictionary. When you
print a **PDF** from the command line, SumatraPDF reads them and uses them as
defaults:

| ViewerPreferences key | Effect                                                                                |
| --------------------- | ------------------------------------------------------------------------------------- |
| `PrintScaling`        | `/None` prints at original size (no scaling); `/AppDefault` uses SumatraPDF's default |
| `NumCopies`           | number of copies                                                                      |
| `Duplex`              | `Simplex`, `DuplexFlipShortEdge` or `DuplexFlipLongEdge`                              |
| `PickTrayByPDFSize`   | when true, pick the input tray by page size (same as `bin=auto`)                      |

These are **defaults only**. Anything you pass in `-print-settings` overrides the
PDF's value — e.g. `-print-settings "2x"` prints 2 copies even if the PDF asks
for 3. To ignore the PDF's embedded values completely, add
`ignore-pdf-print-settings`:

```
SumatraPDF.exe -print-to-default -print-settings "ignore-pdf-print-settings" document.pdf
```

This applies to command-line printing only; when you print from the window, the
print dialog's own values are used.

## Print dialog defaults

You can change some defaults used by the print dialog with the `PrinterDefaults`
advanced setting (in `Settings → Advanced Settings`):

```
PrinterDefaults [
	PrintScale = none
	Collate = collate
	PrintDpi = 0
]
```

- `PrintScale` — default page scaling. Values: `shrink` (default), `fit`,
  `stretch`, `none`.
- `Collate` — default for the print dialog's Collate checkbox. Values: `default`
  (leave the printer/driver default), `collate`, `nocollate`. You can still
  change it per print in the dialog. For command-line printing, use the
  `collate` / `nocollate` `-print-settings` tokens instead.
- `PrintDpi` — resolution to assume for the document when printing at original
  size; `0` (default) uses what the file says. The same as `dpi=<n>` in
  `-print-settings`, for printing from the window. See
  [Printing at actual size](#printing-at-actual-size-11).

## Printing at actual size (1:1)

"Actual size" needs two things: no scaling (**Actual size (1:1)** in the
dialog, `noscale` on the command line) and a correct idea of how big the
document is.

- **PDF, XPS, DjVu, EPUB...** have real page sizes, so no scaling is all it takes.
- **Images** (TIFF, PNG, JPEG, BMP, scans) are only pixels; their physical size is
  pixels divided by the resolution stored in the file (TIFF resolution tags,
  PNG `pHYs`, JPEG JFIF density, EXIF). SumatraPDF uses that, and assumes 96 dpi
  when the file has none. A 300 dpi A3 scan therefore prints as A3 only if the
  scanner wrote 300 dpi into the file; a file with no resolution prints about
  3x too large, and a file that claims 72 dpi comes out bigger still. The
  resolution SumatraPDF read is shown in Document Properties (`Ctrl + D`) as
  _DPI_.
- **Image folders and comic books** (CBZ, CBR, a directory of images) are always
  treated as 96 dpi, whatever the images say, so their pages fit a screen.

When the file's resolution is missing or wrong, tell SumatraPDF what it is:

- from the window: set `PrinterDefaults.PrintDpi` (Settings → Advanced Settings),
  e.g. `PrintDpi = 300`, then print with **Actual size (1:1)**
- from the command line: `-print-settings "noscale,dpi=300"`

The override only sets the size; the pixels are still sent at full resolution.

Printing a rectangular selection (see [Printing part of a page](#printing-part-of-a-page))
at actual size works the same way: choose **Actual size (1:1)**.

## Recipes for common tasks

**Print a PDF to the default printer, unattended**

```
SumatraPDF.exe -print-to-default -silent document.pdf
```

**Print at exact size (no scaling) — forms, labels, pre-printed stationery**

```
SumatraPDF.exe -print-to "Label Printer" -print-settings "noscale" label.pdf
```

**Print a scan at its real size when the file lacks (or lies about) its DPI**

```
SumatraPDF.exe -print-to "Plotter" -print-settings "noscale,dpi=300" drawing.tif
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

**Fix an upside-down printout (e.g. when printing to XPS / Print to PDF)**

```
SumatraPDF.exe -print-to "Microsoft Print to PDF" -print-settings "rotate=180" doc.pdf
```

**"Print" to a PDF file**

```
SumatraPDF.exe -print-to "Microsoft Print to PDF" -print-settings "output=C:\out\result.pdf" input.pdf
```

## Common command-line printing problems

### Paper size or tray ignored

Drivers often report paper sizes under non-obvious names. If `paper=A4` has no effect:

1. Print once from the Windows dialog and note the exact paper name in the driver.
2. Use that string: `-print-settings "paper=A3 297 x 420 mm"`.
3. Or use `paperkind=<num>` with the Windows `DMPAPER_*` id.
4. For mixed page sizes: `paper=auto,bin=auto`.

Combine with `ignore-pdf-print-settings` if the PDF embeds conflicting `ViewerPreferences`.

### Wrong orientation or margins

- Wide pages auto-rotate 90° to fit — add `disable-auto-rotation` to keep original orientation.
- Upside-down virtual printer output — try `rotate=180`.
- Content too small — use `fit` or `shrink` (default); exact size — `noscale`.

### Job prints to wrong printer

`-print-to` requires the **exact** printer name from Windows Settings → Printers. `-print-to-default` avoids name mismatches.

### Silent script reports failure

Check the [exit code](#exit-codes). Common results:

| Code | Check                                                                             |
| ---- | --------------------------------------------------------------------------------- |
| `4`  | Printer name typo or no default printer                                           |
| `5`  | Driver error — update driver, try printing to "Microsoft Print to PDF" to isolate |
| `6`  | `sumatrapdfrestrict.ini` has `PrinterAccess = 0`                                  |

Add `-silent` only after confirming the command works interactively (without `-silent`, error dialogs explain failures).

### Lines or barcodes missing

SumatraPDF renders each page to a bitmap before printing. Very fine lines, hairline barcodes, or light gray rules may disappear at printer resolution. Try `noscale` with a higher-DPI driver, or print from a vector-aware tool. See [Reporting printing bugs](Reporting-printing-bugs.md) — often driver/resolution, not SumatraPDF logic.

### Windows 11: no Advanced tab in print dialog

Use command-line printing (this section) or enable the [legacy print dialog](#windows-11-the-advanced-tab-is-missing) — CLI exposes every Advanced option on all Windows versions.

## See also

- [FAQ](FAQ.md) — printing quick answers
- [Command-line arguments](Command-line-arguments.md) — all command-line options
- [Reporting printing bugs](Reporting-printing-bugs.md) — what to include when a
  printout is wrong
