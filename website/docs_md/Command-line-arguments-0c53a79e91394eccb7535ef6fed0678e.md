# [Documentation](/docs/) : Command-line arguments

You can launch Sumatra with additional command-line options: `SumatraPDF.exe [argument ...] [filepath ...]` .

All arguments start with dash (-). Some arguments are followed by additional parameter.

Anything that is not recognized as a known option is interpreted as a file path so it's possible to mix file paths and command-line arguments.

## List of command line options

-  `-register-for-pdf` : registers SumatraPDF as the default PDF reader for the current user. If the user is administrator, it registers it for all users
-  `-restrict` : runs in restricted mode where you can disable features that require access to file system, registry and the internet. Useful for kiosk-like usage. Read more detailed documentation.
-  `-presentation` : display in presentation view
-  `-fullscreen` : display in fullscreen view
-  `-invert-colors` : TODO: remove this option?

## Navigation options

-  `-named-dest <destination-name> ` : Searches the first indicated file for a destination or a table-of-contents entry (or starting with version 3.1 also a page label) matching destination-name and scrolls the document to it. Combine with -reuse-instance if the document is already open.
-  `-page <pageNo> ` : Scrolls the first indicated file to the indicated page. Combine with -reuse-instance if the document is already open.
-  `-view <view-mode>` : Sets the view mode for the first indicated file. Available view modes:
	- "single page"
	- "continuous single page"
	- facing
	- "continuous facing"
	- "book view"
	- "continuous book view"

	 Notice that options with space have to be surrounded by "" quotes.

	Combine with `-reuse-instance` if the document is already open.

-  `-zoom <zoom-level> ` : Sets the zoom level for the first indicated file. Alternatives are "fit page", "fit width", "fit content" or any percentage value. Combine with -reuse-instance if the document is already open.
-  `-scroll <x,y>` : Scrolls to the given coordinates for the first indicated file. Combine with -reuse-instance if the document is already open.

## Printing options

-  `-print-to-default` : Prints all files indicated on this command line to the system default printer. After printing, SumatraPDF exits immediately (check the error code for failure).
-  `-print-to <printer-name>` : Prints all files indicated on this command line to the named printer. After printing, SumatraPDF exits immediately (check the error code for failure). E.g. `-print-to "Microsoft XPS Document Writer" ` prints all indicated files to the XPS virtual printer.
-  `-print-settings <settings-list>`
	- Used in combination with `-print-to` and `-print-to-default` . Allows to tweak some of the printing related settings without using the Print dialog. The settings-list is a comma separated list of page ranges and advanced options such as
		-  `even` or `odd`
		-  `noscale` , `shrink` and `fit`
		-  `color` or `monochrome`
		-  `duplex` , `duplexshort` , `duplexlong` and `simplex`
		-  `bin=<num or name>` : select tray to print to
	- E.g. ` -print-settings "1-3,5,10-8,odd,fit,bin=2"` prints pages 1, 3, 5, 9 (i.e. the odd pages from the ranges 1-3, 5-5 and 10-8) and scales them so that they fit into the printable area of the paper.
	-  `-print-settings "3x"` : prints the document 3 times
-  `-silent` : Used in combination with ` -print-to` and `-print-to-default` . Silences any error messages related to command line printing.
-  `-print-dialog ` : Displays the Print dialog for all the files indicated on this command line.
-  `-exit-when-done ` : Used in combination with ` -print-dialog` (and `-stress-test` ). Exits SumatraPDF after the Print dialog has been dismissed and the document printed.

## Options related to forward/inverse search (for LaTeX editors)

-  `-forward-search <sourcepath> <line>` : Performs a forward search from a LaTeX source file to a loaded PDF document (using PdfSync or SyncTeX). This is an alternative to the ForwardSearch DDE command. E.g. -forward-search main.tex 123 highlights all text related to line 123 in main.tex.
-  `-reuse-instance` : Tells an already open SumatraPDF to load the indicated files. If there are several running instances, behavior is undefined. Only needed when communicating with SumatraPDF through DDE (use the ReuseInstance setting instead otherwise).
-  `-inverse-search <command-line>` : Sets the command line to be used for performing an inverse search from a PDF document (usually back to a LaTeX source file). The inverse search command line can also be set from the Setting dialog. Use the variable %f for the current filename and %l for the current line.
[Deprecated]: This setting is exposed in the Options dialog after the first PDF document with corresponding .synctex or .pdfsync file has been loaded. Alternatively, use the corresponding advanced setting instead.
-  `-fwdsearch-offset <offset> -fwdsearch-width <width> -fwdsearch-color <hexcolor> -fwdsearch-permanent <flag>` : Allows to customize the forward search highlight. Set the offset to a positive number to change the highlight style to a rectangle at the left of the page (instead of rectangles over the whole text). The flag for -fwdsearch-permanent can be 0 (make the highlight fade away, default) or 1.
[Deprecated]: Use the corresponding advanced settings instead.

## Developer options

-  `-console` : Opens a console window alongside SumatraPDF for accessing (MuPDF) debug output.
-  `-stress-test <path> [file-filter] [range] [cycle-count]`
	- Renders all pages of the indicated file/directory for stability and performance testing. E.g.:

		-stress-test file1.pdf 25x
		-stress-test file2.pdf 1-3
		-stress-test dir *.pdf;*.xps 15- 3x

	renders file1.pdf 25 times, renders pages 1 to 3 of file2.pdf and renders all but the first 14 PDF and XPS files from dir 3 times.

-  `-bench <filepath> [page-range] ` : Renders all pages (or just the indicated ones) for the given file and then outputs the required rendering times for performance testing and comparisons. Often used together with `-console` .

## Deprecated options

The following options just set values in the settings file and may be removed in any future version:

-  `-bg-color <hexcolor>` : Changes the yellow background color to a different color. See e.g. [html-color-codes.info](http://html-color-codes.info/) for a way to generate the hexcode for a color. E.g. `-bg-color #999999` changes the color to gray.
[Deprecated]: Use [MainWindowBackground](https://www.sumatrapdfreader.org/settings.html#MainWindowBackground) setting instead.
-  `-esc-to-exit` : Enables the Escape key to quit SumatraPDF. Deprecated: Use the [EscToExit](https://www.sumatrapdfreader.org/settings.html#EscToExit) setting instead.
-  `-set-color-range <text-hexcolor> <background-hexcolor>` : Uses the given two colors for foreground and background and maps all other colors used in a document in between these two. E.g. `-set-color-range #dddddd #333333` displays soft white text on a dark gray background. [Deprecated]: Use the TextColor and BackgroundColor settings for FixedPageUI instead.
-  `-lang <language-code> ` : Sets the UI language. See [/scripts/trans_langs.py] (https://github.com/sumatrapdfreader/sumatrapdf/blob/master/scripts/trans_langs.py) for the list of available language codes. E.g. -lang de. [Deprecated]: Use the UiLanguage setting instead.
-  `-manga-mode <mode> ` : Enables or disables "Manga mode" for reading (mainly Japanese) comic books from right to left. Mode must be "true" or 1 for enabling and "false" or 0 for disabling this feature.

Deprecated: Use the [CbxMangaMode](https://www.sumatrapdfreader.org/settings.html#ComicBookUI_CbxMangaMode) setting for ComicBookUI instead.