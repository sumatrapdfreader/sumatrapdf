package main

import (
	"fmt"
	"path/filepath"
	"strings"

	"github.com/kjk/u"
)

// generates C code from struct definitions

// Type represents a type definition
type Type struct {
	Name  string
	Ctype string
}

var (
	// Bool defines a primitive bool type
	Bool = &Type{"Bool", "bool"}
	// Color defines a primitive color type
	Color = &Type{"Color", "COLORREF"}
	// Float defines a float type
	Float = &Type{"Float", "float"}
	// Int defines an int
	Int = &Type{"Int", "int"}
	// String defines a UNICODE string
	String = &Type{"String", "WCHAR*"}
	// Utf8String defines a utf8 string
	Utf8String = &Type{"Utf8String", "char*"}
	// Comment defines a comment
	Comment = &Type{"Comment", ""}
)

// Field defines a field in a struct
type Field struct {
	Name       string
	Type       *Type
	Default    interface{}
	Comment    string
	Internal   bool
	CName      string
	Expert     bool // expert prefs are not exposed by the UI
	DocComment string
	Version    string // version in which this setting was introduced
	PreRelease bool   // prefs which aren't written out in release builds

	StructName string // only valid for some types
}

func (f *Field) setExpert() *Field {
	f.Expert = true
	return f
}

func (f *Field) setInternal() *Field {
	f.Internal = true
	return f
}

func (f *Field) setPreRelease() *Field {
	f.PreRelease = true
	if f.Type.Name == "Struct" {
		f.Type.Name = "Prerelease"
	}
	return f
}

func (f *Field) setVersion(v string) *Field {
	f.Version = v
	return f
}

func (f *Field) setDoc(s string) *Field {
	f.DocComment = s
	return f
}

func (f *Field) isComment() bool {
	return f.Type.Name == "Comment"
}

func (f *Field) cdefault(built map[string]int) string {
	if f.Type == Bool {
		// "true" or "false", happens to be the same in C++ as in Go
		return fmt.Sprintf("%v", f.Default)
	}
	if f.Type == Color {
		return fmt.Sprintf("0x%06x", f.Default)
	}
	if f.Type == Float {
		// converting float to intptr_t rounds the value
		return fmt.Sprintf(`(intptr_t)"%v"`, f.Default)
	}
	if f.Type == Int {
		return fmt.Sprintf("%d", f.Default)
	}
	if f.Type == String {
		if f.Default == nil {
			return "0"
		}
		return fmt.Sprintf(`(intptr_t)L"%s"`, f.Default)
	}
	if f.Type == Utf8String {
		if f.Default == nil {
			return "0"
		}
		return fmt.Sprintf(`(intptr_t)"%s"`, f.Default)
	}
	typeName := f.Type.Name
	switch typeName {
	case "Struct", "Array", "Compact", "Prerelease":
		idStr := ""
		id := built[f.StructName]
		if id > 0 {
			idStr = fmt.Sprintf("_%d_", id)
		}
		return fmt.Sprintf("(intptr_t)&g%s%sInfo", f.StructName, idStr)
	}
	switch typeName {
	case "ColorArray", "FloatArray", "IntArray":
		if f.Default == nil {
			return "0"
		}
		return fmt.Sprintf(`(intptr_t)"%s"`, f.Default)
	}
	if typeName == "StringArray" {
		if f.Default == nil {
			return "0"
		}
		return fmt.Sprintf(`(intptr_t)"%s"`, f.Default)
	}
	if typeName == "Comment" {
		if f.Comment == "" {
			return "0"
		}
		return fmt.Sprintf(`(intptr_t)"%s"`, f.Comment)
	}
	panicIf(true)
	return ""
}

func (f *Field) initDefault() string {
	commentChar := ""
	if f.Type == Bool {
		// "true" or "false", happens to be the same in C++ as in Go
		return fmt.Sprintf("%s = %v", f.Name, f.Default)
	}
	if f.Type == Color {
		col := f.Default.(uint32)
		return fmt.Sprintf("%s = #%02x%02x%02x", f.Name, col&0xFF, (col>>8)&0xFF, (col>>16)&0xFF)
	}
	if f.Type == Float {
		// converting float to intptr_t rounds the value
		return fmt.Sprintf(`%s = %v`, f.Name, f.Default)
	}
	if f.Type == Int {
		return fmt.Sprintf("%s = %d", f.Name, f.Default)
	}
	if f.Type == String {
		if f.Default != nil {
			return fmt.Sprintf(`%s = %s`, f.Name, f.Default)
		}
		return fmt.Sprintf(`%s %s =`, commentChar, f.Name)
	}
	if f.Type == Utf8String {
		if f.Default != nil {
			return fmt.Sprintf(`%s = %s`, f.Name, f.Default)
		}
		return fmt.Sprintf(`%s %s =`, commentChar, f.Name)
	}
	typeName := f.Type.Name
	if typeName == "Compact" {
		fields := f.Default.([]*Field)
		var vals []string
		for _, field := range fields {
			v := field.initDefault()
			parts := strings.SplitN(v, " = ", 2)
			vals = append(vals, parts[1])
		}
		v := strings.Join(vals, " ")
		return fmt.Sprintf("%s = %s", f.Name, v)
	}
	switch typeName {
	case "ColorArray", "FloatArray", "IntArray":
		if f.Default != nil {
			return fmt.Sprintf("%s = %v", f.Name, f.Default)
		}
		return fmt.Sprintf("%s %s =", commentChar, f.Name)
	}
	if typeName == "StringArray" {
		if f.Default != nil {
			return fmt.Sprintf("%s = %v", f.Name, f.Default)
		}
		return fmt.Sprintf("%s %s =", commentChar, f.Name)
	}
	panicIf(true)
	return ""
}

func toCName(s string) string {
	return s
}

func mkField(name string, typ *Type, def interface{}, comment string) *Field {
	res := &Field{
		Name:       name,
		Type:       typ,
		Default:    def,
		Comment:    comment,
		DocComment: comment,
		Version:    "2.3",
	}
	if name != "" {
		res.CName = strings.ToLower(name[0:1]) + name[1:]
	}
	return res
}

func (f *Field) setStructName(structName string) *Field {
	f.StructName = structName
	if f.Type.Name == "Array" {
		ctype := fmt.Sprintf("Vec<%s*>*", structName)
		f.Type.Ctype = ctype
	}
	if f.Type.Name == "Struct" {
		f.Type.Ctype = structName
	}
	if f.Type.Name == "Compact" {
		f.Type.Ctype = structName
	}
	return f
}

func mkStruct(name string, fields []*Field, comment string) *Field {
	structName := name
	typ := &Type{"Struct", structName}
	res := mkField(name, typ, fields, comment)
	res.StructName = structName
	return res
}

func mkCompactStruct(name string, fields []*Field, comment string) *Field {
	res := mkStruct(name, fields, comment)
	res.Type.Name = "Compact"
	return res
}

func mkArray(name string, fields []*Field, comment string) *Field {
	structName := name
	structName = strings.TrimSuffix(structName, "s")
	ctype := fmt.Sprintf("Vec<%s*>*", structName)
	typ := &Type{"Array", ctype}
	res := mkField(name, typ, fields, comment)
	res.StructName = structName
	return res
}

func mkCompactArray(name string, typ *Type, def interface{}, comment string) *Field {
	typ2Name := fmt.Sprintf("%sArray", typ.Name)
	typ2CType := fmt.Sprintf("Vec<%s>*", typ.Ctype)
	typ2 := &Type{typ2Name, typ2CType}
	res := mkField(name, typ2, def, comment)
	return res
}

func mkComment(comment string) *Field {
	return mkField("", Comment, nil, comment)
}

func mkEmptyLine() *Field {
	return mkComment("")
}

func mkRGBA(r uint32, g uint32, b uint32, a uint32) uint32 {
	return r | (g << 8) | (b << 16) | (a << 24)
}

func mkRGB(r uint32, g uint32, b uint32) uint32 {
	return mkRGBA(r, g, b, 0)
}

// ##### setting definitions for SumatraPDF #####

var (
	windowPos = []*Field{
		mkField("X", Int, 0, "y coordinate"),
		mkField("Y", Int, 0, "y coordinate"),
		mkField("Dx", Int, 0, "width"),
		mkField("Dy", Int, 0, "height"),
	}

	scrollPos = []*Field{
		mkField("X", Int, 0, "x coordinate"),
		mkField("Y", Int, 0, "y coordinate"),
	}

	fileTime = []*Field{
		mkField("DwHighDateTime", Int, 0, ""),
		mkField("DwLowDateTime", Int, 0, ""),
	}

	printerDefaults = []*Field{
		mkField("PrintScale", Utf8String, "shrink", "default value for scaling (shrink, fit, none)"),
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
			"if true, TextColor and BackgroundColor will be temporarily swapped").setInternal(),
		mkField("HideScrollbars", Bool, false,
			"if true, hides the scrollbars but retain ability to scroll").setInternal(),
	}

	ebookUI = []*Field{
		// default serif font, a different font is used for monospaced text (currently always "Courier New")
		mkField("FontName", String, "Georgia", "name of the font. takes effect after re-opening the document"),
		mkField("FontSize", Float, 12.5, "size of the font. takes effect after re-opening the document"),
		mkField("TextColor", Color, mkRGB(0x5F, 0x4B, 0x32), "color for text"),
		mkField("BackgroundColor", Color, mkRGB(0xFB, 0xF0, 0xD9), "color of the background (page)"),
		mkField("UseFixedPageUI", Bool, false,
			"if true, the UI used for PDF documents will be used for ebooks as well "+
				"(enables printing and searching, disables automatic reflow)"),
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

	annotationDefaults = []*Field{
		mkField("HighlightColor", Color, mkRGB(0xFF, 0xFF, 0x60),
			"color used for the highlight tool (in prerelease builds, the current selection "+
				"can be converted into a highlight annotation by pressing the 'h' key)"),
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
		mkField("DecryptionKey", Utf8String, nil,
			"Hex encoded MD5 fingerprint of file content (32 chars) followed by "+
				"crypt key (64 chars) - only applies for PDF documents").setDoc("data required to open a password protected document without having to " +
			"ask for the password again"),
		mkField("UseDefaultState", Bool, false,
			"if true, we use global defaults when opening this file (instead of "+
				"the values below)"),
		// NOTE: fields below UseDefaultState aren't serialized if UseDefaultState is true!
		mkField("DisplayMode", Utf8String, "automatic",
			"how pages should be laid out for this document, needs to be synchronized with "+
				"DefaultDisplayMode after deserialization and before serialization").setDoc("layout of pages. valid values: automatic, single page, facing, book view, " +
			"continuous, continuous facing, continuous book view"),
		mkCompactStruct("ScrollPos", scrollPos,
			"how far this document has been scrolled (in x and y direction)").setStructName("Point"),
		mkField("PageNo", Int, 1,
			"number of the last read page"),
		mkField("Zoom", Utf8String, "fit page",
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
		mkField("FilePath", Utf8String, nil,
			"path of the document"),
		mkField("DisplayMode", Utf8String, "automatic",
			"same as FileStates -> DisplayMode"),
		mkField("PageNo", Int, 1,
			"number of the last read page"),
		mkField("Zoom", Utf8String, "fit page",
			"same as FileStates -> Zoom"),
		mkField("Rotation", Int, 0,
			"same as FileStates -> Rotation"),
		mkCompactStruct("ScrollPos", scrollPos,
			"how far this document has been scrolled (in x and y direction)").setStructName("Point"),
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
		mkEmptyLine(),

		mkField("MainWindowBackground", Color, mkRGBA(0xFF, 0xF2, 0x00, 0x80),
			"background color of the non-document windows, traditionally yellow").setExpert(),

		//MkField("ThemeName", Utf8String, "light", "the name of the theme to use"),

		mkField("EscToExit", Bool, false,
			"if true, Esc key closes SumatraPDF").setExpert(),
		mkField("ReuseInstance", Bool, false,
			"if true, we'll always open files using existing SumatraPDF process").setExpert(),
		mkField("UseSysColors", Bool, false,
			"if true, we use Windows system colors for background/text color. Over-rides other settings").setExpert(),
		mkField("RestoreSession", Bool, true,
			"if true and SessionData isn't empty, that session will be restored at startup").setExpert(),
		mkField("TabWidth", Int, 300,
			"maximum width of a single tab"),
		mkEmptyLine(),

		mkStruct("FixedPageUI", fixedPageUI,
			"customization options for PDF, XPS, DjVu and PostScript UI").setExpert(),
		mkStruct("EbookUI", ebookUI,
			"customization options for eBooks (EPUB, Mobi, FictionBook) UI. If UseFixedPageUI is true, FixedPageUI settings apply instead").setExpert(),
		mkStruct("ComicBookUI", comicBookUI,
			"customization options for Comic Book and images UI").setExpert(),
		mkStruct("ChmUI", chmUI,
			"customization options for CHM UI. If UseFixedPageUI is true, FixedPageUI settings apply instead").setExpert(),
		mkArray("ExternalViewers", externalViewer,
			"list of additional external viewers for various file types "+
				"(can have multiple entries for the same format)").setExpert(),
		mkField("ShowMenubar", Bool, true,
			"if false, the menu bar will be hidden for all newly opened windows "+
				"(use F9 to show it until the window closes or Alt to show it just briefly), only applies if UseTabs is false").setExpert().setVersion("2.5"),
		mkField("ReloadModifiedDocuments", Bool, true,
			"if true, a document will be reloaded automatically whenever it's changed "+
				"(currently doesn't work for documents shown in the ebook UI)").setExpert().setVersion("2.5"),
		mkField("FullPathInTitle", Bool, false,
			"if true, we show the full path to a file in the title bar").setExpert().setVersion("3.0"),
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
		mkStruct("AnnotationDefaults", annotationDefaults,
			"default values for user added annotations in FixedPageUI documents "+
				"(preliminary and still subject to change)").setExpert().setPreRelease(),
		mkCompactArray("DefaultPasswords", String, nil,
			"passwords to try when opening a password protected document").setDoc("a whitespace separated list of passwords to try when opening a password protected document " +
			"(passwords containing spaces must be quoted)").setExpert().setVersion("2.4"),
		mkField("CustomScreenDPI", Int, 0,
			"actual resolution of the main screen in DPI (if this value "+
				"isn't positive, the system's UI setting is used)").setExpert().setVersion("2.5"),
		mkEmptyLine(),

		mkField("RememberStatePerDocument", Bool, true,
			"if true, we store display settings for each document separately (i.e. everything "+
				"after UseDefaultState in FileStates)"),
		mkField("UiLanguage", Utf8String, nil,
			"ISO code of the current UI language").setDoc("[ISO code](langs.html) of the current UI language"),
		mkField("ShowToolbar", Bool, true,
			"if true, we show the toolbar at the top of the window"),
		mkField("ShowFavorites", Bool, false,
			"if true, we show the Favorites sidebar"),
		mkField("AssociatedExtensions", String, nil,
			"a list of extensions that SumatraPDF has associated itself with and will "+
				"reassociate if a different application takes over (e.g. \".pdf .xps .epub\")"),
		mkField("AssociateSilently", Bool, false,
			"whether file associations should be fixed silently or only after user feedback"),
		mkField("CheckForUpdates", Bool, true,
			"if true, we check once a day if an update is available"),
		mkField("VersionToSkip", String, nil,
			"we won't ask again to update to this version"),
		mkField("RememberOpenedFiles", Bool, true,
			"if true, we remember which files we opened and their display settings"),
		mkField("InverseSearchCmdLine", String, nil,
			"pattern used to launch the LaTeX editor when doing inverse search"),
		mkField("EnableTeXEnhancements", Bool, false,
			"if true, we expose the SyncTeX inverse search command line in Settings -> Options"),
		mkField("DefaultDisplayMode", Utf8String, "automatic",
			"how pages should be laid out by default, needs to be synchronized with "+
				"DefaultDisplayMode after deserialization and before serialization").setDoc("default layout of pages. valid values: automatic, single page, facing, " +
			"book view, continuous, continuous facing, continuous book view"),
		mkField("DefaultZoom", Utf8String, "fit page",
			"default zoom (in %) or one of those values: fit page, fit width, fit content"),
		mkField("WindowState", Int, 1,
			"default state of new windows (same as the last closed)").setDoc("default state of the window. 1 is normal, 2 is maximized, " +
			"3 is fullscreen, 4 is minimized"),
		mkCompactStruct("WindowPos", windowPos,
			"default position (can be on any monitor)").setStructName("Rect").setDoc("default position (x, y) and size (width, height) of the window"),
		mkField("ShowToc", Bool, true,
			"if true, we show table of contents (Bookmarks) sidebar if it's present "+
				"in the document"),
		mkField("SidebarDx", Int, 0,
			"width of favorites/bookmarks sidebar (if shown)"),
		mkField("TocDy", Int, 0,
			"if both favorites and bookmarks parts of sidebar are visible, this is "+
				"the height of bookmarks (table of contents) part"),
		mkField("TreeFontSize", Int, 0,
			"font size for bookmarks and favorites tree views. 0 means Windows default").setVersion("3.3"),
		mkField("ShowStartPage", Bool, true,
			"if true, we show a list of frequently read documents when no document is loaded"),
		mkField("UseTabs", Bool, true,
			"if true, documents are opened in tabs instead of new windows").setVersion("3.0"),
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
		mkComment("Settings after this line have not been recognized by the current version"),
	}

	globalPrefsStruct = mkStruct("GlobalPrefs", globalPrefs,
		"Most values on this structure can be updated through the UI and are persisted "+
			"in SumatraPDF-settings.txt")
)

// limit comment lines to 72 chars
func formatComment(comment string, start string) []string {
	var lines []string
	parts := strings.Split(comment, " ")
	line := start
	for _, part := range parts {
		if len(line)+len(part) > 71 {
			lines = append(lines, line)
			line = start
		}
		line += " " + part
	}
	if line != start {
		lines = append(lines, line)
	}
	return lines
}

func formatArrayLines(data [][]string) []string {
	var lines []string
	for _, ld := range data {
		s := fmt.Sprintf("\t{ %s, %s, %s },", ld[0], ld[1], ld[2])
		lines = append(lines, s)
	}
	return lines
}

func buildStruct(struc *Field, built map[string]int) string {
	lines := []string{}
	required := []string{}
	var s string
	if struc.Comment != "" {
		comments := formatComment(struc.Comment, "//")
		lines = append(lines, comments...)
	}
	s = fmt.Sprintf("struct %s {", struc.StructName)
	lines = append(lines, s)
	fields := struc.Default.([]*Field)
	for _, field := range fields {
		if field.isComment() {
			continue
		}
		comments := formatComment(field.Comment, "\t//")
		lines = append(lines, comments...)
		s = fmt.Sprintf("\t%s %s;", field.Type.Ctype, field.CName)
		lines = append(lines, s)
		switch field.Type.Name {
		case "Struct", "Compact", "Array", "Prerelease":
			name := field.Name
			if name == field.StructName || name == field.StructName+"s" {
				if _, ok := built[name]; !ok {
					s = buildStruct(field, built)
					required = append(required, s)
					required = append(required, "")
					built[name]++
				}
			}
		}
	}
	lines = append(lines, "};")
	lines = append(lines, "")
	s1 := strings.Join(required, "\n")
	s2 := strings.Join(lines, "\n")
	return s1 + s2
}

func buildMetaData(struc *Field, built map[string]int) string {
	var lines, names []string
	var data [][]string
	suffix := ""
	n := built[struc.StructName]
	if n > 0 {
		suffix = fmt.Sprintf("_%d_", n)
	}
	fullName := struc.StructName + suffix
	fields := struc.Default.([]*Field)
	var s string
	for _, field := range fields {
		if field.Internal {
			continue
		}
		dataLine := []string{}
		s = fmt.Sprintf("offsetof(%s, %s)", struc.StructName, field.CName)
		dataLine = append(dataLine, s)

		tpName := field.Type.Name
		s = fmt.Sprintf("SettingType::%s", tpName)
		dataLine = append(dataLine, s)
		s = field.cdefault(built)
		dataLine = append(dataLine, s)

		names = append(names, field.Name)
		switch field.Type.Name {
		case "Struct", "Prerelease", "Compact", "Array":
			sublines := buildMetaData(field, built) // TODO: pass built?
			lines = append(lines, sublines)
			lines = append(lines, "")
			built[field.StructName]++
		case "Comment":
			// replace "offsetof(%s, %s)" with "(size_t)-1"
			dataLine[0] = "(size_t)-1"
		}
		data = append(data, dataLine)
	}
	s = fmt.Sprintf("static const FieldInfo g%sFields[] = {", fullName)
	lines = append(lines, s)
	dataLines := formatArrayLines(data)
	lines = append(lines, dataLines...)
	lines = append(lines, "};")
	// gFileStateInfo isn't const so that the number of fields can be changed at runtime (cf. UseDefaultState)
	constStr := ""
	if fullName != "FileState" {
		constStr = "const "
	}
	namesStr := strings.Join(names, "\\0")
	s = fmt.Sprintf("static %sStructInfo g%sInfo = { sizeof(%s), %d, g%sFields, \"%s\" };", constStr, fullName, struc.StructName, len(names), fullName, namesStr)
	lines = append(lines, s)
	return strings.Join(lines, "\n")
}

const settingsStructsHeader = `// !!!!! This file is auto-generated by do/gen_settings_structs.go

/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 (see COPYING) */

class RenderedBitmap;

{{structDef}}

#ifdef INCLUDE_SETTINGSSTRUCTS_METADATA

{{structMetadata}}

#endif
`

func genSettingsStruct() string {
	built := map[string]int{}
	structDef := buildStruct(globalPrefsStruct, built)
	structMetaData := buildMetaData(globalPrefsStruct, map[string]int{})

	content := settingsStructsHeader
	content = strings.Replace(content, "{{structDef}}", structDef, -1)
	content = strings.Replace(content, "{{structMetadata}}", structMetaData, -1)
	return content
}

func genAndSaveSettingsStructs() {
	helpURI := fmt.Sprintf("For documentation, see https://www.sumatrapdfreader.org/settings/settings%s.html", extractSumatraVersionMust())

	globalPrefs[0].Comment = helpURI

	s := genSettingsStruct()

	// TODO: port this
	/*
		beforeUseDefaultState = true
		for field in FileSettings:
			if field.name == "UseDefaultState":
				beforeUseDefaultState = false
			elif beforeUseDefaultState:
				assert field.name not in rememberedDisplayState, "%s shouldn't be serialized when UseDefaultState is true" % field.name
			else:
				assert field.name in rememberedDisplayState or field.internal, "%s won't be serialized when UseDefaultState is true" % field.name
	*/

	//fmt.Printf("%s\n", s)
	s = strings.Replace(s, "\n", "\r\n", -1)
	s = strings.Replace(s, "\t", "    ", -1)
	path := filepath.Join("src", "SettingsStructs.h")
	u.WriteFileMust(path, []byte(s))
	detectClangFormat()
	clangFormatFile(path)
	fmt.Printf("Wrote '%s'\n", path)

	genSettingsHTML()
	genLangsHTML()
}
