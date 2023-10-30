package main

// ##### setting definitions for SumatraPDF #####

var (
	windowPos = []*Field{
		mkField("X", Int, 0, "y coordinate"),
		mkField("Y", Int, 0, "y coordinate"),
		mkField("Dx", Int, 0, "width"),
		mkField("Dy", Int, 0, "height"),
	}

	keyboardShortcut = []*Field{
		mkField("Cmd", String, "", "command"),
		mkField("Key", String, "", "keyboard shortcut (e.g. Ctrl-Alt-F)"),
	}

	scrollPos = []*Field{
		mkField("X", Float, 0, "x coordinate"),
		mkField("Y", Float, 0, "y coordinate"),
	}

	fileTime = []*Field{
		mkField("DwHighDateTime", Int, 0, ""),
		mkField("DwLowDateTime", Int, 0, ""),
	}

	printerDefaults = []*Field{
		mkField("PrintScale", String, "shrink", "default value for scaling (shrink, fit, none)"),
	}

	forwardSearch = []*Field{
		mkField("HighlightOffset", Int, 0,
			"when set to a positive value, the forward search highlight style will "+
				"be changed to a rectangle at the left of the page (with the indicated "+
				"amount of margin from the page margin)"),
		mkField("HighlightWidth", Int, 15,
			"width of the highlight rectangle (if HighlightOffset is > 0)"),
		mkField("HighlightColor", Color, mkRGB(0x65, 0x81, 0xFF),
			"color used for the forward search highlight"),
		mkField("HighlightPermanent", Bool, false,
			"if true, highlight remains visible until the next mouse click "+
				"(instead of fading away immediately)"),
	}

	windowMarginFixedPageUI = []*Field{
		mkField("Top", Int, 2, "size of the top margin between window and document"),
		mkField("Right", Int, 4, "size of the right margin between window and document"),
		mkField("Bottom", Int, 2, "size of the bottom margin between window and document"),
		mkField("Left", Int, 4, "size of the left margin between window and document"),
	}

	windowMarginComicBookUI = []*Field{
		mkField("Top", Int, 0, "size of the top margin between window and document"),
		mkField("Right", Int, 0, "size of the right margin between window and document"),
		mkField("Bottom", Int, 0, "size of the bottom margin between window and document"),
		mkField("Left", Int, 0, "size of the left margin between window and document"),
	}

	pageSpacing = []*Field{
		mkField("Dx", Int, 4, "horizontal difference"),
		mkField("Dy", Int, 4, "vertical difference"),
	}

	fixedPageUI = []*Field{
		mkField("TextColor", Color, mkRGB(0x00, 0x00, 0x00),
			"color value with which black (text) will be substituted"),
		mkField("BackgroundColor", Color, mkRGB(0xFF, 0xFF, 0xFF),
			"color value with which white (background) will be substituted"),
		mkField("SelectionColor", Color, mkRGB(0xF5, 0xFC, 0x0C),
			"color value for the text selection rectangle (also used to highlight found text)").setVersion("2.4"),
		mkCompactStruct("WindowMargin", windowMarginFixedPageUI,
			"top, right, bottom and left margin (in that order) between window and document"),
		mkCompactStruct("PageSpacing", pageSpacing,
			"horizontal and vertical distance between two pages in facing and book view modes").setStructName("Size"),
		mkCompactArray("GradientColors", Color, nil, // "#2828aa #28aa28 #aa2828",
			"colors to use for the gradient from top to bottom (stops will be inserted "+
				"at regular intervals throughout the document); currently only up to three "+
				"colors are supported; the idea behind this experimental feature is that the "+
				"background might allow to subconsciously determine reading progress; "+
				"suggested values: #2828aa #28aa28 #aa2828"),
		mkField("InvertColors", Bool, false,
			"if true, TextColor and BackgroundColor of the document will be swapped"),
		mkField("HideScrollbars", Bool, false,
			"if true, hides the scrollbars but retains ability to scroll"),
	}

	comicBookUI = []*Field{
		mkCompactStruct("WindowMargin", windowMarginComicBookUI,
			"top, right, bottom and left margin (in that order) between window and document"),
		mkCompactStruct("PageSpacing", pageSpacing,
			"horizontal and vertical distance between two pages in facing and book view modes").setStructName("Size"),
		mkField("CbxMangaMode", Bool, false,
			"if true, default to displaying Comic Book files in manga mode (from right to left if showing 2 pages at a time)"),
	}

	chmUI = []*Field{
		mkField("UseFixedPageUI", Bool, false,
			"if true, the UI used for PDF documents will be used for CHM documents as well"),
	}

	externalViewer = []*Field{
		mkField("CommandLine", String, nil,
			"command line with which to call the external viewer, may contain "+
				"%p for page number and \"%1\" for the file name (add quotation "+
				"marks around paths containing spaces)"),
		mkField("Name", String, nil,
			"name of the external viewer to be shown in the menu (implied by CommandLine if missing)"),
		mkField("Filter", String, nil,
			"optional filter for which file types the menu item is to be shown; separate multiple entries using ';' and don't include any spaces (e.g. *.pdf;*.xps for all PDF and XPS documents)"),
	}

	selectionHandler = []*Field{
		mkField("URL", String, nil, "url to invoke for the selection. ${selection} will be replaced with current selection and ${userlang} with language code for current UI (e.g. 'de' for German)"),
		mkField("Name", String, nil, "name shown in context menu"),
		mkField("CmdID", Int, 0, "").setInternal(),
	}

	annotations = []*Field{
		// yellow
		mkField("HighlightColor", Color, mkRGB(0xFF, 0xFF, 0x0),
			"highlight annotation color"),
		// blue
		mkField("UnderlineColor", Color, mkRGB(0x00, 0xFF, 0x0),
			"underline annotation color"),
		// fuchsia (pinkish), the default in mupdf
		mkField("SquigglyColor", Color, mkRGB(0xff, 0x00, 0xff),
			"squiggly annotation color").setVersion("3.5"),
		// red
		mkField("StrikeOutColor", Color, mkRGB(0xff, 0x00, 0x00),
			"strike out annotation color").setVersion("3.5"),
		// yellow
		mkField("FreeTextColor", Color, "", "color of free text annotation").setVersion("3.5"),
		mkField("FreeTextSize", Int, 12, "size of free text annotation").setVersion("3.5"),
		mkField("FreeTextBorderWidth", Int, 1, "width of free text annotation border").setVersion("3.5"),
		mkField("TextIconColor", Color, "", "text icon annotation color"),
		mkField("TextIconType", String, "", "type of text annotation icon: comment, help, insert, key, new paragraph, note, paragraph. If not set: note."),
		mkField("DefaultAuthor", String, "", "default author for created annotations, use (none) to not add an author at all. If not set will use Windows user name").setVersion("3.4"),
	}

	favorite = []*Field{
		mkField("Name", String, nil,
			"name of this favorite as shown in the menu"),
		mkField("PageNo", Int, 0,
			"number of the bookmarked page"),
		mkField("PageLabel", String, nil,
			"label for this page (only present if logical and physical page numbers are not the same)"),
		mkField("MenuId", Int, 0,
			"id of this favorite in the menu (assigned by AppendFavMenuItems)").setInternal(),
	}

	fileSettings = []*Field{
		mkField("FilePath", String, nil,
			"path of the document"),
		mkArray("Favorites", favorite,
			"Values which are persisted for bookmarks/favorites"),
		mkField("IsPinned", Bool, false,
			"a document can be \"pinned\" to the Frequently Read list so that it "+
				"isn't displaced by recently opened documents"),
		mkField("IsMissing", Bool, false,
			"if a document can no longer be found but we still remember valuable state, "+
				"it's classified as missing so that it can be hidden instead of removed").setDoc("if true, the file is considered missing and won't be shown in any list"),
		mkField("OpenCount", Int, 0,
			"in order to prevent documents that haven't been opened for a while "+
				"but used to be opened very frequently constantly remain in top positions, "+
				"the openCount will be cut in half after every week, so that the "+
				"Frequently Read list hopefully better reflects the currently relevant documents").setDoc("number of times this document has been opened recently"),
		mkField("DecryptionKey", String, nil,
			"Hex encoded MD5 fingerprint of file content (32 chars) followed by "+
				"crypt key (64 chars) - only applies for PDF documents").setDoc("data required to open a password protected document without having to " +
			"ask for the password again"),
		mkField("UseDefaultState", Bool, false,
			"if true, we use global defaults when opening this file (instead of "+
				"the values below)"),
		// NOTE: fields below UseDefaultState aren't serialized if UseDefaultState is true!
		mkField("DisplayMode", String, "automatic",
			"how pages should be laid out for this document, needs to be synchronized with "+
				"DefaultDisplayMode after deserialization and before serialization").setDoc("layout of pages. valid values: automatic, single page, facing, book view, " +
			"continuous, continuous facing, continuous book view"),
		mkCompactStruct("ScrollPos", scrollPos,
			"how far this document has been scrolled (in x and y direction)").setStructName("PointF"),
		mkField("PageNo", Int, 1,
			"number of the last read page"),
		mkField("Zoom", String, "fit page",
			"zoom (in %) or one of those values: fit page, fit width, fit content"),
		mkField("Rotation", Int, 0,
			"how far pages have been rotated as a multiple of 90 degrees"),
		mkField("WindowState", Int, 0,
			"state of the window. 1 is normal, 2 is maximized, "+
				"3 is fullscreen, 4 is minimized"),
		mkCompactStruct("WindowPos", windowPos,
			"default position (can be on any monitor)").setStructName("Rect"),
		mkField("ShowToc", Bool, true,
			"if true, we show table of contents (Bookmarks) sidebar if it's present "+
				"in the document"),
		mkField("SidebarDx", Int, 0,
			"width of the left sidebar panel containing the table of contents"),
		mkField("DisplayR2L", Bool, false,
			"if true, the document is displayed right-to-left in facing and book view modes "+
				"(only used for comic book documents)"),
		mkField("ReparseIdx", Int, 0,
			"index into an ebook's HTML data from which reparsing has to happen "+
				"in order to restore the last viewed page (i.e. the equivalent of PageNo for the ebook UI)").setDoc("data required to restore the last read page in the ebook UI"),
		mkCompactArray("TocState", Int, nil,
			"tocState is an array of ids for ToC items that have been toggled by "+
				"the user (i.e. aren't in their default expansion state). - "+
				"Note: We intentionally track toggle state as opposed to expansion state "+
				"so that we only have to save a diff instead of all states for the whole "+
				"tree (which can be quite large) (internal)").setDoc("data required to determine which parts of the table of contents have been expanded"),
		// NOTE: fields below UseDefaultState aren't serialized if UseDefaultState is true!
		mkField("Thumbnail", &Type{"", "RenderedBitmap *"}, "NULL",
			"thumbnails are saved as PNG files in sumatrapdfcache directory").setInternal(),
		mkField("Index", &Type{"", "size_t"}, "0",
			"temporary value needed for FileHistory::cmpOpenCount").setInternal(),
	}

	// list of fields which aren't serialized when UseDefaultState is set
	rememberedDisplayState = []string{"DisplayMode", "ScrollPos", "PageNo", "Zoom", "Rotation", "WindowState", "WindowPos", "ShowToc", "SidebarDx", "DisplayR2L", "ReparseIdx", "TocState"}

	tabState = []*Field{
		mkField("FilePath", String, nil,
			"path of the document"),
		mkField("DisplayMode", String, "automatic",
			"same as FileStates -> DisplayMode"),
		mkField("PageNo", Int, 1,
			"number of the last read page"),
		mkField("Zoom", String, "fit page",
			"same as FileStates -> Zoom"),
		mkField("Rotation", Int, 0,
			"same as FileStates -> Rotation"),
		mkCompactStruct("ScrollPos", scrollPos,
			"how far this document has been scrolled (in x and y direction)").setStructName("PointF"),
		mkField("ShowToc", Bool, true,
			"if true, the table of contents was shown when the document was closed"),
		mkCompactArray("TocState", Int, nil,
			"same as FileStates -> TocState"),
	}

	sessionData = []*Field{
		mkArray("TabStates", tabState,
			"a subset of FileState required for restoring the state of a single tab "+
				"(required for handling documents being opened twice)").setDoc("data required for restoring the view state of a single tab"),
		mkField("TabIndex", Int, 1, "index of the currently selected tab (1-based)"),
		mkField("WindowState", Int, 0, "same as FileState -> WindowState"),
		mkCompactStruct("WindowPos", windowPos, "default position (can be on any monitor)").setStructName("Rect"),
		mkField("SidebarDx", Int, 0, "width of favorites/bookmarks sidebar (if shown)"),
	}

	globalPrefs = []*Field{
		mkComment(""),
		mkField("Theme", String, "", "the name of the theme to use").setDoc("Valid themes: light, dark, darker").setVersion("3.5"),
		mkStruct("FixedPageUI", fixedPageUI,
			"customization options for PDF, XPS, DjVu and PostScript UI").setExpert(),
		mkStruct("ComicBookUI", comicBookUI,
			"customization options for Comic Book and images UI").setExpert(),
		mkStruct("ChmUI", chmUI,
			"customization options for CHM UI. If UseFixedPageUI is true, FixedPageUI settings apply instead").setExpert(),
		mkEmptyLine(),

		mkArray("SelectionHandlers", selectionHandler, "list of handlers for selected text, shown in context menu when text selection is active. See [docs for more information](https://www.sumatrapdfreader.org/docs/Customize-search-translation-services)"),
		mkArray("ExternalViewers", externalViewer,
			"list of additional external viewers for various file types. "+
				"See [docs for more information](https://www.sumatrapdfreader.org/docs/Customize-external-viewers)").setExpert(),
		mkEmptyLine(),

		//the below prefs don't apply to EbookUI (so far)
		mkCompactArray("ZoomLevels", Float, "8.33 12.5 18 25 33.33 50 66.67 75 100 125 150 200 300 400 600 800 1000 1200 1600 2000 2400 3200 4800 6400",
			"zoom levels which zooming steps through in addition to Fit Page, Fit Width and "+
				"the minimum and maximum allowed values (8.33 and 6400)").setExpert().setDoc("sequence of zoom levels when zooming in/out; all values must lie between 8.33 and 6400"),
		mkField("ZoomIncrement", Float, 0,
			"zoom step size in percents relative to the current zoom level. "+
				"if zero or negative, the values from ZoomLevels are used instead").setExpert(),
		mkEmptyLine(),

		// the below prefs apply only to FixedPageUI and ComicBookUI (so far)
		mkStruct("PrinterDefaults", printerDefaults,
			"these override the default settings in the Print dialog").setExpert(),
		mkStruct("ForwardSearch", forwardSearch,
			"customization options for how we show forward search results (used from "+
				"LaTeX editors)").setExpert(),
		mkStruct("Annotations", annotations,
			"default values for annotations in PDF documents").setExpert().setVersion("3.3"),
		mkCompactArray("DefaultPasswords", String, nil,
			"passwords to try when opening a password protected document").setDoc("a whitespace separated list of passwords to try when opening a password protected document " +
			"(passwords containing spaces must be quoted)").setExpert().setVersion("2.4"),
		mkEmptyLine(),

		mkField("RememberOpenedFiles", Bool, true,
			"if true, we remember which files we opened and their display settings"),
		mkField("RememberStatePerDocument", Bool, true,
			"if true, we store display settings for each document separately (i.e. everything "+
				"after UseDefaultState in FileStates)"),
		mkField("RestoreSession", Bool, true,
			"if true and SessionData isn't empty, that session will be restored at startup").setExpert(),
		mkField("LazyLoading", Bool, true, "when restoring session, delay loading of documents until their tab is selected").setVersion("3.6"),
		mkField("UiLanguage", String, nil,
			"ISO code of the current UI language").setDoc("[ISO code](langs.html) of the current UI language"),
		mkField("InverseSearchCmdLine", String, nil,
			"pattern used to launch the LaTeX editor when doing inverse search"),
		mkField("EnableTeXEnhancements", Bool, false,
			"if true, we expose the SyncTeX inverse search command line in Settings -> Options"),
		mkField("DefaultDisplayMode", String, "automatic",
			"how pages should be laid out by default, needs to be synchronized with "+
				"DefaultDisplayMode after deserialization and before serialization").setDoc("default layout of pages. valid values: automatic, single page, facing, " +
			"book view, continuous, continuous facing, continuous book view"),
		mkField("DefaultZoom", String, "fit page",
			"default zoom (in %) or one of those values: fit page, fit width, fit content"),
		mkArray("Shortcuts", keyboardShortcut, "custom keyboard shortcuts"),
		mkField("EscToExit", Bool, false,
			"if true, Esc key closes SumatraPDF").setExpert(),
		mkField("ReuseInstance", Bool, true,
			"if true, we'll always open files using existing SumatraPDF process").setExpert(),
		mkField("ReloadModifiedDocuments", Bool, true,
			"if true, a document will be reloaded automatically whenever it's changed "+
				"(currently doesn't work for documents shown in the ebook UI)").setExpert().setVersion("2.5"),
		mkEmptyLine(),

		mkField("MainWindowBackground", Color, mkRGBA(0xFF, 0xF2, 0x00, 0x80),
			"background color of the non-document windows, traditionally yellow").setExpert(),
		mkField("FullPathInTitle", Bool, false,
			"if true, we show the full path to a file in the title bar").setExpert().setVersion("3.0"),
		mkField("ShowMenubar", Bool, true,
			"if false, the menu bar will be hidden for all newly opened windows "+
				"(use F9 to show it until the window closes or Alt to show it just briefly), only applies if UseTabs is false").setExpert().setVersion("2.5"),
		mkField("ShowToolbar", Bool, true,
			"if true, we show the toolbar at the top of the window"),
		mkField("ShowFavorites", Bool, false,
			"if true, we show the Favorites sidebar"),
		mkField("ShowToc", Bool, true,
			"if true, we show table of contents (Bookmarks) sidebar if it's present "+
				"in the document"),
		mkField("NoHomeTab", Bool, false, "if true, doesn't open Home tab"),
		mkField("ShowLinks", Bool, false, "if true we draw a blue border around links in the document").setVersion("3.6"),
		mkField("TocDy", Int, 0,
			"if both favorites and bookmarks parts of sidebar are visible, this is "+
				"the height of bookmarks (table of contents) part"),
		mkField("SidebarDx", Int, 0,
			"width of favorites/bookmarks sidebar (if shown)"),
		mkField("ToolbarSize", Int, 18, "height of toolbar").setVersion("3.4"),
		mkField("TabWidth", Int, 300,
			"maximum width of a single tab"),
		mkField("TreeFontSize", Int, 0,
			"font size for bookmarks and favorites tree views. 0 means Windows default").setVersion("3.3"),
		mkField("TreeFontWeightOffset", Int, 0,
			"font weight offset for bookmarks and favorites tree views."),
		mkField("TreeFontName", String, "automatic",
			"font name for bookmarks and favorites tree views. automatic means Windows default"),
		mkField("SmoothScroll", Bool, false,
			"if true, implements smooth scrolling").setExpert(),
		mkField("ShowStartPage", Bool, true,
			"if true, we show a list of frequently read documents when no document is loaded"),
		mkField("CheckForUpdates", Bool, true,
			"if true, we check once a day if an update is available"),
		mkField("VersionToSkip", String, nil,
			"we won't ask again to update to this version"),
		mkField("WindowState", Int, 1,
			"default state of new windows (same as the last closed)").setDoc("default state of the window. 1 is normal, 2 is maximized, " +
			"3 is fullscreen, 4 is minimized"),
		mkCompactStruct("WindowPos", windowPos,
			"default position (can be on any monitor)").setStructName("Rect").setDoc("default position (x, y) and size (width, height) of the window"),
		mkField("UseTabs", Bool, true,
			"if true, documents are opened in tabs instead of new windows").setVersion("3.0"),
		mkField("UseSysColors", Bool, false,
			"if true, we use Windows system colors for background/text color. Over-rides other settings").setExpert(),
		mkField("CustomScreenDPI", Int, 0,
			"actual resolution of the main screen in DPI (if this value "+
				"isn't positive, the system's UI setting is used)").setExpert().setVersion("2.5"),
		mkEmptyLine(),

		// file history and favorites
		mkArray("FileStates", fileSettings,
			"information about opened files (in most recently used order)"),
		mkArray("SessionData", sessionData,
			"state of the last session, usage depends on RestoreSession").setVersion("3.1"),
		mkCompactArray("ReopenOnce", String, nil,
			"a list of paths for files to be reopened at the next start "+
				"or the string \"SessionData\" if this data is saved in SessionData "+
				"(needed for auto-updating)").setDoc("data required for reloading documents after an auto-update").setVersion("3.0"),
		mkCompactStruct("TimeOfLastUpdateCheck", fileTime,
			"timestamp of the last update check").setStructName("FILETIME").setDoc("data required to determine when SumatraPDF last checked for updates"),
		mkField("OpenCountWeek", Int, 0,
			"week count since 2011-01-01 needed to \"age\" openCount values in file history").setDoc("value required to determine recency for the OpenCount value in FileStates"),
		// non-serialized fields
		mkCompactStruct("LastPrefUpdate", fileTime,
			"modification time of the preferences file when it was last read").setStructName("FILETIME").setInternal(),
		mkField("DefaultDisplayModeEnum", &Type{"", "DisplayMode"}, "DM_AUTOMATIC",
			"value of DefaultDisplayMode for internal usage").setInternal(),
		mkField("DefaultZoomFloat", Float, -1,
			"value of DefaultZoom for internal usage").setInternal(),
		mkEmptyLine(),
		mkComment("Settings below are not recognized by the current version"),
	}

	globalPrefsStruct = mkStruct("GlobalPrefs", globalPrefs,
		"Most values on this structure can be updated through the UI and are persisted "+
			"in SumatraPDF-settings.txt")
)
