# Command-line arguments

For command-line arguments for the installer see [this page](Installer-cmd-line-arguments.md).

You can launch Sumatra with additional command-line options: `SumatraPDF [argument ...] [filepath ...]`.

Most arguments start with dash (`-`). Printing options also accept Adobe Reader's slash form (`/p`, `/t`). Some arguments are followed by additional parameters.

Anything that is not recognized as a known option is interpreted as a file path so it's possible to mix file paths and command-line arguments.

## List of command line options

- `-presentation` : start in presentation view
- `-fullscreen` : start in full screen view
- `-new-window` : when opening a file, always open it in a new window, as opposed to in a tab (**ver 3.2+**)
- `-appdata <directory>` : set custom directory where we'll store `SumatraPDF-settings.txt` file and thumbnail cache
- `-restrict` : runs in restricted mode where you can disable features that require access to file system, registry and the internet. Useful for kiosk-like usage. See [Configure for restricted use](Configure-for-restricted-use.md).
- `-for-testing` : for ad-hoc testing by humans or agents. Always starts a new instance, doesn't restore a session (only loads files given on the command line) and doesn't save settings (**ver 3.7+**)
- `-dbg-control <named-pipe>` : starts a test control server on a named pipe. Used by automated tests through `cmd/control.ts`; combine with `-for-testing`. (**ver 3.7+**)
- `-pwd <password>` : use the given password to open password-protected documents. If the password is wrong, SumatraPDF falls back to default passwords and then asks interactively.

## Navigation options

- `-named-dest <destination-name>` : searches the first indicated file for a destination or a table-of-contents entry (or starting with version 3.1 also a page label) matching destination-name and scrolls the document to it. Combine with -reuse-instance if the document is already open.
- `-page <pageNo>` : scrolls the first indicated file to the indicated page. Combine with -reuse-instance if the document is already open.
- `-view <view-mode>` : set view mode for the first indicated file. Available view modes:
  - `"single page"`
  - `"continuous single page"`
  - `facing`
  - `"continuous facing"`
  - `"book view"`
  - `"continuous book view"`

  Notice that options with space have to be surrounded by "" quotes.

  Combine with `-reuse-instance` if the document is already open.

- `-zoom <zoom-level>` : Sets the zoom level for the first indicated file. Alternatives are "fit page", "fit width", "fit content" or any percentage value. Combine with -reuse-instance if the document is already open.
- `-scroll <x,y>` : Scrolls to the given coordinates for the first indicated file. Combine with `-reuse-instance` if the document is already open.
- `search <term>` : Start a search for a given term when opening a document e.g. `SumatraPDF -search "foo" bar.pdf`. **Ver 3.4+**

## Send DDE commands

- `-dde cmd` : send [DDE commands](DDE-Commands.md) to currently running instance e.g. `-dde '[Open("C:\Users\kjk\foo.pdf")]'`. Make sure to properly quote arguments. File paths must be absolute. **Ver 3.5+**.

## Printing options

For a detailed printing guide with examples for common tasks, see [Printing](Printing.md).

- `-print-to-default` : prints all files indicated on this command line to the system default printer. After printing, SumatraPDF exits immediately (check the error code for failure).
- `-print-to <printer-name>` : prints all files indicated on this command line to the named printer. After printing, SumatraPDF exits immediately (check the error code for failure). E.g. `-print-to "Microsoft XPS Document Writer"` prints all indicated files to the XPS virtual printer.
- `-print-settings <settings-list>`
  - used in combination with `-print-to` and `-print-to-default`. Allows to tweak some of the printing related settings without using the Print dialog. The settings-list is a comma separated list of page ranges and advanced options such as
    - page ranges: a single page (e.g. `5`), a range (e.g. `2-6`, or reversed `10-8`), `last` for the last page, or a negative number counting from the end (`-1` is the last page, `-2` the second-to-last; ranges may use negatives too, e.g. `-3--1` for the last 3 pages)
    - `even` or `odd`.
    - `portrait` or `landscape` : can provide 90 degree rotation of contents (NOT the rotation of paper which must be pre-set by the choice of printer defaults)
    - `disable-auto-rotation` : by default a page wider than it is tall is rotated 90 degrees to fit the paper; this prints the content in its original orientation instead (available since 3.5)
    - `rotate=<degrees>` : rotate the printout by an extra `90`, `180` or `270` degrees (on top of the automatic rotation). Useful to fix a wrong orientation, e.g. upside-down (`rotate=180`) output on virtual printers
    - `noscale`, `shrink`, `fit` and `stretch` (`stretch` fills the paper in both dimensions, ignoring the aspect ratio)
    - `center` : horizontally center the page on the paper. Useful with `noscale` when the page is smaller than the paper (e.g. envelopes or A5 stock fed through a tray that centers the paper)
    - `color` or `monochrome`
    - `collate` or `nocollate` : when printing multiple copies, collate (1,2,3,1,2,3) or don't (1,1,2,2,3,3)
    - `duplex`, `duplexshort`, `duplexlong` and `simplex`
    - `bin=<num or name>` : select tray to print to. Use `bin=auto` to let the printer pick the input tray whose paper matches the document page size (like Adobe's "Choose paper source by PDF page size")
    - `paper=<page size>` : page size is `A2`, `A3`, `A4`, `A5`, `A6`, `letter`, `legal`, `tabloid`, `statement`, or a name reported by the printer (e.g. `A3 297 x 420 mm`). Custom dimensions: `paper=76mm x 130mm`. Use `paper=auto` to set the paper size from each page's own size, for documents with mixed page sizes (combine with `bin=auto` to also pick the matching tray)
    - `paperkind=<num>` : paper size by Windows `DMPAPER_*` id (from `SumatraPDF.exe -list-printers`); use when `paper=A3` does not match the driver's paper name
    - `ignore-pdf-print-settings` : don't apply the print defaults embedded in a PDF's `ViewerPreferences` (see below)
  - e.g. `-print-settings "1-3,5,10-8,odd,fit,bin=2"` prints pages 1, 3, 5, 9 (i.e. the odd pages from the ranges 1-3, 5-5 and 10-8) and scales them so that they fit into the printable area of the paper.
  - `-print-settings "3x"` : prints the document 3 times
  - For PDF files, print defaults embedded in the document's `ViewerPreferences` are applied automatically: `PrintScaling` (`/None` means no scaling), `NumCopies`, `Duplex` (`Simplex` / `DuplexFlipShortEdge` / `DuplexFlipLongEdge`) and `PickTrayByPDFSize` (pick the tray by page size). Any explicit value in `-print-settings` overrides the PDF's default; add `ignore-pdf-print-settings` to ignore the PDF's values entirely.
- `-silent` : used in combination with `-print-to` and `-print-to-default`. Silences any error messages related to command line printing.
- `-print-dialog` : displays the Print dialog for all the files indicated on this command line.
- `/p` : Adobe Reader compatibility alias for `-print-dialog`.
- `/t <file> <printer>` : Adobe Reader compatibility for silent printing (same as `-print-to <printer> <file>`). Optional driver and port arguments are accepted and ignored. If the file is already on the command line, `/t <printer>` is enough.
- `-exit-when-done` : used in combination with `-print-dialog` (and `-stress-test`). Exits SumatraPDF after the Print dialog has been dismissed and the document printed.

Adobe Reader flags that are **not** supported because they conflict with SumatraPDF options: `/h` (help, not hidden mode), `/n` (stress-test parallelism, not new instance), `/s` (silent printing errors, not suppress splash).

When printing with `-print-to` / `-print-to-default`, the process exit code tells you why printing failed (useful for unattended/silent printing):

| Exit code | Meaning |
| --- | --- |
| `0` | success |
| `2` | couldn't open the file (not found or unsupported format) |
| `3` | the document doesn't allow printing |
| `4` | the printer (named, or default) doesn't exist |
| `5` | the printer driver / device failed |
| `6` | printing is disabled by restriction policy |

With multiple files, the exit code is `0` only if all printed; otherwise it's the category of the first failure. Note: failures that happen inside the print spooler/driver *after* the job is submitted (out of paper, offline, etc.) can't be reported this way.

## Options related to forward/inverse search (for LaTeX editors)

- `-forward-search "<sourcepath>" <line> "<pdfpath>"`: performs a forward search from a LaTeX source file to a loaded PDF document (using PdfSync or SyncTeX). This is an alternative to the ForwardSearch DDE command. E.g. `-forward-search "/path/to/main.tex" 123 "/path/to/main.pdf"` highlights all text related to line 123 in main.tex.
- `-reuse-instance` : tells an already open SumatraPDF to load the indicated files. If there are several running instances, behavior is undefined. Only needed when communicating with SumatraPDF through DDE (use the ReuseInstance setting instead otherwise).
- `-inverse-search <command-line>` : sets the command line to be used for performing an inverse search from a PDF document (usually back to a LaTeX source file). The inverse search command line can also be set from the Setting dialog. Use the variable %f for the current filename and %l for the current line.
  [Deprecated]: This setting is exposed in the Options dialog after the first PDF document with corresponding .synctex or .pdfsync file has been loaded. Alternatively, use the corresponding advanced setting instead.
- `-fwdsearch-offset <offset> -fwdsearch-width <width> -fwdsearch-color <hexcolor> -fwdsearch-permanent <flag>` : allows to customize the forward search highlight. Set the offset to a positive number to change the highlight style to a rectangle at the left of the page (instead of rectangles over the whole text). The flag for `-fwdsearch-permanent` can be 0 (make the highlight fade away, default) or 1.
  [Deprecated]: Use the corresponding advanced settings instead.

## Developer options

- `-console` : Opens a console window alongside SumatraPDF for accessing (MuPDF) debug output.
- `-list-printers` : prints installed printers, default settings, paper sizes and input trays, then exits. This is useful when choosing `-print-to`, `paper=`, `paperkind=` or `bin=` values for command-line printing.
- `-stress-test <path> [file-filter] [range] [cycle-count]`
  : Renders all pages of the indicated file/directory for stability and performance testing. E.g.:

  ```
  -stress-test file1.pdf 25x
  -stress-test file2.pdf 1-3
  -stress-test dir *.pdf;*.xps 15- 3x
  ```

  renders file1.pdf 25 times, renders pages 1 to 3 of file2.pdf and renders all but the first 14 PDF and XPS files from dir 3 times.

- `-bench <filepath> [page-range]` : Renders all pages (or just the indicated ones) for the given file and then outputs the required rendering times for performance testing and comparisons. Often used together with `-console`.

## Deprecated options

The following options just set values in the settings file and may be removed in any future version:

- `-bg-color <hexcolor>` : changes the yellow background color to a different color. See e.g. [html-color-codes.info](https://html-color-codes.info/) for a way to generate the hexcode for a color. E.g. `-bg-color #999999` changes the color to gray. Deprecated: use the `MainWindowBackground` setting instead.
- `-esc-to-exit` : enables the Escape key to quit SumatraPDF. Deprecated: use the `EscToExit` setting instead.
- `-set-color-range <text-hexcolor> <background-hexcolor>` : uses the given two colors for foreground and background and maps all other colors used in a document in between these two. E.g. `-set-color-range #dddddd #333333` displays soft white text on a dark gray background. Deprecated: use the `FixedPageUI.TextColor` and `FixedPageUI.BackgroundColor` settings instead.
- `-lang <language-code>` : sets the UI language. See [Translation languages](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/src/TranslationLangs.cpp) for the list of available language codes. E.g. `-lang de`. Deprecated: use the `UiLanguage` setting instead.
- `-manga-mode <mode>` : enables or disables "Manga mode" for reading (mainly Japanese) comic books from right to left. Mode must be "true" or 1 for enabling and "false" or 0 for disabling this feature. Deprecated: use the `ComicBookUI.CbxMangaMode` setting instead.
- `-invert-colors` : temporarily swaps fixed-page document text and background colors for the current run. The older alias `-invertcolors` is also accepted.
