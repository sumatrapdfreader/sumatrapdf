package main

import (
	"fmt"
	"path/filepath"
	"strings"

	"github.com/kjk/u"
)

// generates C code from struct definitions

type Type struct {
	Name  string
	Ctype string
}

var (
	Bool       = &Type{"Bool", "bool"}
	Color      = &Type{"Color", "COLORREF"}
	Float      = &Type{"Float", "float"}
	Int        = &Type{"Int", "int"}
	String     = &Type{"String", "WCHAR*"}
	Utf8String = &Type{"Utf8String", "char*"}
	Comment    = &Type{"Comment", ""}
)

const (
	// TODO: remove, python compat
	False = false
	True  = true
)

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

func (f *Field) SetExpert() *Field {
	f.Expert = true
	return f
}

func (f *Field) SetInternal() *Field {
	f.Internal = true
	return f
}

func (f *Field) SetPreRelease() *Field {
	f.PreRelease = true
	if f.Type.Name == "Struct" {
		f.Type.Name = "Prerelease"
	}
	return f
}

func (f *Field) SetVersion(v string) *Field {
	f.Version = v
	return f
}

func (f *Field) SetDoc(s string) *Field {
	f.DocComment = s
	return f
}

func (f *Field) isComment() bool {
	return f.Type.Name == "Comment"
}

func (self *Field) cdefault(built map[string]int) string {
	if self.Type == Bool {
		// "true" or "false", happens to be the same in C++ as in Go
		return fmt.Sprintf("%v", self.Default)
	}

	if self.Type == Color {
		return fmt.Sprintf("0x%06x", self.Default)
	}

	if self.Type == Float {
		// converting float to intptr_t rounds the value
		return fmt.Sprintf(`(intptr_t)"%v"`, self.Default)
	}

	if self.Type == Int {
		return fmt.Sprintf("%d", self.Default)
	}

	if self.Type == String {
		if self.Default == nil {
			return "0"
		}
		return fmt.Sprintf(`(intptr_t)L"%s"`, self.Default)
	}

	if self.Type == Utf8String {
		if self.Default == nil {
			return "0"
		}
		return fmt.Sprintf(`(intptr_t)"%s"`, self.Default)
	}

	typeName := self.Type.Name
	switch typeName {
	case "Struct", "Array", "Compact", "Prerelease":
		idStr := ""
		id := built[self.StructName]
		if id > 0 {
			idStr = fmt.Sprintf("_%d_", id)
		}
		return fmt.Sprintf("(intptr_t)&g%s%sInfo", self.StructName, idStr)
	}

	switch typeName {
	case "ColorArray", "FloatArray", "IntArray":
		if self.Default == nil {
			return "0"
		}
		return fmt.Sprintf(`(intptr_t)"%s"`, self.Default)
	}

	if typeName == "StringArray" {
		if self.Default == nil {
			return "0"
		}
		return fmt.Sprintf(`(intptr_t)"%s"`, self.Default)
	}

	if typeName == "Comment" {
		if self.Comment == "" {
			return "0"
		}
		return fmt.Sprintf(`(intptr_t)"%s"`, self.Comment)
	}
	return ""
}

func toCName(s string) string {
	return s
}

func MkField(name string, typ *Type, def interface{}, comment string) *Field {
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

func (f *Field) SetStructName(structName string) *Field {
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

func MkStruct(name string, fields []*Field, comment string) *Field {
	structName := name
	typ := &Type{"Struct", structName}
	res := MkField(name, typ, fields, comment)
	res.StructName = structName
	return res
}

func MkCompactStruct(name string, fields []*Field, comment string) *Field {
	res := MkStruct(name, fields, comment)
	res.Type.Name = "Compact"
	return res
}

func MkArray(name string, fields []*Field, comment string) *Field {
	structName := name
	structName = strings.TrimSuffix(structName, "s")
	ctype := fmt.Sprintf("Vec<%s*>*", structName)
	typ := &Type{"Array", ctype}
	res := MkField(name, typ, fields, comment)
	res.StructName = structName
	return res
}

func MkCompactArray(name string, typ *Type, def interface{}, comment string) *Field {
	typ2Name := fmt.Sprintf("%sArray", typ.Name)
	typ2CType := fmt.Sprintf("Vec<%s>*", typ.Ctype)
	typ2 := &Type{typ2Name, typ2CType}
	res := MkField(name, typ2, def, comment)
	return res
}

func MkComment(comment string) *Field {
	return MkField("", Comment, nil, comment)
}

func EmptyLine() *Field {
	return MkComment("")
}

func RGBA(r uint32, g uint32, b uint32, a uint32) uint32 {
	return r | (g << 8) | (b << 16) | (a << 24)
}

func RGB(r uint32, g uint32, b uint32) uint32 {
	return RGBA(r, g, b, 0)
}

// ##### setting definitions for SumatraPDF #####

var (
	WindowPos = []*Field{
		MkField("X", Int, 0, "y coordinate"),
		MkField("Y", Int, 0, "y coordinate"),
		MkField("Dx", Int, 0, "width"),
		MkField("Dy", Int, 0, "height"),
	}

	ScrollPos = []*Field{
		MkField("X", Int, 0, "x coordinate"),
		MkField("Y", Int, 0, "y coordinate"),
	}

	FileTime = []*Field{
		MkField("DwHighDateTime", Int, 0, ""),
		MkField("DwLowDateTime", Int, 0, ""),
	}

	PrinterDefaults = []*Field{
		MkField("PrintScale", Utf8String, "shrink", "default value for scaling (shrink, fit, none)"),
	}

	ForwardSearch = []*Field{
		MkField("HighlightOffset", Int, 0,
			"when set to a positive value, the forward search highlight style will "+
				"be changed to a rectangle at the left of the page (with the indicated "+
				"amount of margin from the page margin)"),
		MkField("HighlightWidth", Int, 15,
			"width of the highlight rectangle (if HighlightOffset is > 0)"),
		MkField("HighlightColor", Color, RGB(0x65, 0x81, 0xFF),
			"color used for the forward search highlight"),
		MkField("HighlightPermanent", Bool, false,
			"if true, highlight remains visible until the next mouse click "+
				"(instead of fading away immediately)"),
	}

	WindowMargin_FixedPageUI = []*Field{
		MkField("Top", Int, 2, "size of the top margin between window and document"),
		MkField("Right", Int, 4, "size of the right margin between window and document"),
		MkField("Bottom", Int, 2, "size of the bottom margin between window and document"),
		MkField("Left", Int, 4, "size of the left margin between window and document"),
	}

	WindowMargin_ComicBookUI = []*Field{
		MkField("Top", Int, 0, "size of the top margin between window and document"),
		MkField("Right", Int, 0, "size of the right margin between window and document"),
		MkField("Bottom", Int, 0, "size of the bottom margin between window and document"),
		MkField("Left", Int, 0, "size of the left margin between window and document"),
	}

	PageSpacing = []*Field{
		MkField("Dx", Int, 4, "horizontal difference"),
		MkField("Dy", Int, 4, "vertical difference"),
	}

	FixedPageUI = []*Field{
		MkField("TextColor", Color, RGB(0x00, 0x00, 0x00),
			"color value with which black (text) will be substituted"),
		MkField("BackgroundColor", Color, RGB(0xFF, 0xFF, 0xFF),
			"color value with which white (background) will be substituted"),
		MkField("SelectionColor", Color, RGB(0xF5, 0xFC, 0x0C),
			"color value for the text selection rectangle (also used to highlight found text)").SetVersion("2.4"),
		MkCompactStruct("WindowMargin", WindowMargin_FixedPageUI,
			"top, right, bottom and left margin (in that order) between window and document"),
		MkCompactStruct("PageSpacing", PageSpacing,
			"horizontal and vertical distance between two pages in facing and book view modes").SetStructName("SizeI"),
		MkCompactArray("GradientColors", Color, nil, // "#2828aa #28aa28 #aa2828",
			"colors to use for the gradient from top to bottom (stops will be inserted "+
				"at regular intervals throughout the document); currently only up to three "+
				"colors are supported; the idea behind this experimental feature is that the "+
				"background might allow to subconsciously determine reading progress; "+
				"suggested values: #2828aa #28aa28 #aa2828"),
		MkField("InvertColors", Bool, False,
			"if true, TextColor and BackgroundColor will be temporarily swapped").SetInternal(),
	}

	EbookUI = []*Field{
		// default serif font, a different font is used for monospaced text (currently always "Courier New")
		MkField("FontName", String, "Georgia", "name of the font. takes effect after re-opening the document"),
		MkField("FontSize", Float, 12.5, "size of the font. takes effect after re-opening the document"),
		MkField("TextColor", Color, RGB(0x5F, 0x4B, 0x32), "color for text"),
		MkField("BackgroundColor", Color, RGB(0xFB, 0xF0, 0xD9), "color of the background (page)"),
		MkField("UseFixedPageUI", Bool, False,
			"if true, the UI used for PDF documents will be used for ebooks as well "+
				"(enables printing and searching, disables automatic reflow)"),
	}

	ComicBookUI = []*Field{
		MkCompactStruct("WindowMargin", WindowMargin_ComicBookUI,
			"top, right, bottom and left margin (in that order) between window and document"),
		MkCompactStruct("PageSpacing", PageSpacing,
			"horizontal and vertical distance between two pages in facing and book view modes").SetStructName("SizeI"),
		MkField("CbxMangaMode", Bool, False,
			"if true, default to displaying Comic Book files in manga mode (from right to left if showing 2 pages at a time)"),
	}

	ChmUI = []*Field{
		MkField("UseFixedPageUI", Bool, False,
			"if true, the UI used for PDF documents will be used for CHM documents as well"),
	}

	ExternalViewer = []*Field{
		MkField("CommandLine", String, nil,
			"command line with which to call the external viewer, may contain "+
				"%p for page number and \"%1\" for the file name (add quotation "+
				"marks around paths containing spaces)"),
		MkField("Name", String, nil,
			"name of the external viewer to be shown in the menu (implied by CommandLine if missing)"),
		MkField("Filter", String, nil,
			"optional filter for which file types the menu item is to be shown; separate multiple entries using ';' and don't include any spaces (e.g. *.pdf;*.xps for all PDF and XPS documents)"),
	}

	AnnotationDefaults = []*Field{
		MkField("HighlightColor", Color, RGB(0xFF, 0xFF, 0x60),
			"color used for the highlight tool (in prerelease builds, the current selection "+
				"can be converted into a highlight annotation by pressing the 'h' key)"),
		MkField("SaveIntoDocument", Bool, True,
			"if true, annotations are appended to PDF documents, "+
				"else they're always saved to an external .smx file"),
	}

	Favorite = []*Field{
		MkField("Name", String, nil,
			"name of this favorite as shown in the menu"),
		MkField("PageNo", Int, 0,
			"number of the bookmarked page"),
		MkField("PageLabel", String, nil,
			"label for this page (only present if logical and physical page numbers are not the same)"),
		MkField("MenuId", Int, 0,
			"id of this favorite in the menu (assigned by AppendFavMenuItems)").SetInternal(),
	}

	FileSettings = []*Field{
		MkField("FilePath", String, nil,
			"path of the document"),
		MkArray("Favorites", Favorite,
			"Values which are persisted for bookmarks/favorites"),
		MkField("IsPinned", Bool, False,
			"a document can be \"pinned\" to the Frequently Read list so that it "+
				"isn't displaced by recently opened documents"),
		MkField("IsMissing", Bool, False,
			"if a document can no longer be found but we still remember valuable state, "+
				"it's classified as missing so that it can be hidden instead of removed").SetDoc("if true, the file is considered missing and won't be shown in any list"),
		MkField("OpenCount", Int, 0,
			"in order to prevent documents that haven't been opened for a while "+
				"but used to be opened very frequently constantly remain in top positions, "+
				"the openCount will be cut in half after every week, so that the "+
				"Frequently Read list hopefully better reflects the currently relevant documents").SetDoc("number of times this document has been opened recently"),
		MkField("DecryptionKey", Utf8String, nil,
			"Hex encoded MD5 fingerprint of file content (32 chars) followed by "+
				"crypt key (64 chars) - only applies for PDF documents").SetDoc("data required to open a password protected document without having to " +
			"ask for the password again"),
		MkField("UseDefaultState", Bool, False,
			"if true, we use global defaults when opening this file (instead of "+
				"the values below)"),
		// NOTE: fields below UseDefaultState aren't serialized if UseDefaultState is true!
		MkField("DisplayMode", String, "automatic",
			"how pages should be laid out for this document, needs to be synchronized with "+
				"DefaultDisplayMode after deserialization and before serialization").SetDoc("layout of pages. valid values: automatic, single page, facing, book view, " +
			"continuous, continuous facing, continuous book view"),
		MkCompactStruct("ScrollPos", ScrollPos,
			"how far this document has been scrolled (in x and y direction)").SetStructName("PointI"),
		MkField("PageNo", Int, 1,
			"number of the last read page"),
		MkField("Zoom", Utf8String, "fit page",
			"zoom (in %) or one of those values: fit page, fit width, fit content"),
		MkField("Rotation", Int, 0,
			"how far pages have been rotated as a multiple of 90 degrees"),
		MkField("WindowState", Int, 0,
			"state of the window. 1 is normal, 2 is maximized, "+
				"3 is fullscreen, 4 is minimized"),
		MkCompactStruct("WindowPos", WindowPos,
			"default position (can be on any monitor)").SetStructName("RectI"),
		MkField("ShowToc", Bool, True,
			"if true, we show table of contents (Bookmarks) sidebar if it's present "+
				"in the document"),
		MkField("SidebarDx", Int, 0,
			"width of the left sidebar panel containing the table of contents"),
		MkField("DisplayR2L", Bool, False,
			"if true, the document is displayed right-to-left in facing and book view modes "+
				"(only used for comic book documents)"),
		MkField("ReparseIdx", Int, 0,
			"index into an ebook's HTML data from which reparsing has to happen "+
				"in order to restore the last viewed page (i.e. the equivalent of PageNo for the ebook UI)").SetDoc("data required to restore the last read page in the ebook UI"),
		MkCompactArray("TocState", Int, nil,
			"tocState is an array of ids for ToC items that have been toggled by "+
				"the user (i.e. aren't in their default expansion state). - "+
				"Note: We intentionally track toggle state as opposed to expansion state "+
				"so that we only have to save a diff instead of all states for the whole "+
				"tree (which can be quite large) (internal)").SetDoc("data required to determine which parts of the table of contents have been expanded"),
		// NOTE: fields below UseDefaultState aren't serialized if UseDefaultState is true!
		MkField("Thumbnail", &Type{"", "RenderedBitmap *"}, "NULL",
			"thumbnails are saved as PNG files in sumatrapdfcache directory").SetInternal(),
		MkField("Index", &Type{"", "size_t"}, "0",
			"temporary value needed for FileHistory::cmpOpenCount").SetInternal(),
	}

	// list of fields which aren't serialized when UseDefaultState is set
	rememberedDisplayState = []string{"DisplayMode", "ScrollPos", "PageNo", "Zoom", "Rotation", "WindowState", "WindowPos", "ShowToc", "SidebarDx", "DisplayR2L", "ReparseIdx", "TocState"}

	TabState = []*Field{
		MkField("FilePath", String, nil,
			"path of the document"),
		MkField("DisplayMode", String, "automatic",
			"same as FileStates -> DisplayMode"),
		MkField("PageNo", Int, 1,
			"number of the last read page"),
		MkField("Zoom", Utf8String, "fit page",
			"same as FileStates -> Zoom"),
		MkField("Rotation", Int, 0,
			"same as FileStates -> Rotation"),
		MkCompactStruct("ScrollPos", ScrollPos,
			"how far this document has been scrolled (in x and y direction)").SetStructName("PointI"),
		MkField("ShowToc", Bool, True,
			"if true, the table of contents was shown when the document was closed"),
		MkCompactArray("TocState", Int, nil,
			"same as FileStates -> TocState"),
	}

	SessionData = []*Field{
		MkArray("TabStates", TabState,
			"a subset of FileState required for restoring the state of a single tab "+
				"(required for handling documents being opened twice)").SetDoc("data required for restoring the view state of a single tab"),
		MkField("TabIndex", Int, 1, "index of the currently selected tab (1-based)"),
		MkField("WindowState", Int, 0, "same as FileState -> WindowState"),
		MkCompactStruct("WindowPos", WindowPos, "default position (can be on any monitor)").SetStructName("RectI"),
		MkField("SidebarDx", Int, 0, "width of favorites/bookmarks sidebar (if shown)"),
	}

	GlobalPrefs = []*Field{
		MkComment(""),
		EmptyLine(),

		MkField("MainWindowBackground", Color, RGBA(0xFF, 0xF2, 0x00, 0x80),
			"background color of the non-document windows, traditionally yellow").SetExpert(),

		//MkField("ThemeName", Utf8String, "light", "the name of the theme to use"),

		MkField("EscToExit", Bool, false,
			"if true, Esc key closes SumatraPDF").SetExpert(),
		MkField("ReuseInstance", Bool, false,
			"if true, we'll always open files using existing SumatraPDF process").SetExpert(),
		MkField("UseSysColors", Bool, false,
			"if true, we use Windows system colors for background/text color. Over-rides other settings").SetExpert(),
		MkField("RestoreSession", Bool, true,
			"if true and SessionData isn't empty, that session will be restored at startup").SetExpert(),
		MkField("TabWidth", Int, 300,
			"maximum width of a single tab"),
		EmptyLine(),

		MkStruct("FixedPageUI", FixedPageUI,
			"customization options for PDF, XPS, DjVu and PostScript UI").SetExpert(),
		MkStruct("EbookUI", EbookUI,
			"customization options for eBooks (EPUB, Mobi, FictionBook) UI. If UseFixedPageUI is true, FixedPageUI settings apply instead").SetExpert(),
		MkStruct("ComicBookUI", ComicBookUI,
			"customization options for Comic Book and images UI").SetExpert(),
		MkStruct("ChmUI", ChmUI,
			"customization options for CHM UI. If UseFixedPageUI is true, FixedPageUI settings apply instead").SetExpert(),
		MkArray("ExternalViewers", ExternalViewer,
			"list of additional external viewers for various file types "+
				"(can have multiple entries for the same format)").SetExpert(),
		MkField("ShowMenubar", Bool, true,
			"if false, the menu bar will be hidden for all newly opened windows "+
				"(use F9 to show it until the window closes or Alt to show it just briefly), only applies if UseTabs is false").SetExpert().SetVersion("2.5"),
		MkField("ReloadModifiedDocuments", Bool, true,
			"if true, a document will be reloaded automatically whenever it's changed "+
				"(currently doesn't work for documents shown in the ebook UI)").SetExpert().SetVersion("2.5"),
		MkField("FullPathInTitle", Bool, false,
			"if true, we show the full path to a file in the title bar").SetExpert().SetVersion("3.0"),
		//the below prefs don't apply to EbookUI (so far)
		MkCompactArray("ZoomLevels", Float, "8.33 12.5 18 25 33.33 50 66.67 75 100 125 150 200 300 400 600 800 1000 1200 1600 2000 2400 3200 4800 6400",
			"zoom levels which zooming steps through in addition to Fit Page, Fit Width and "+
				"the minimum and maximum allowed values (8.33 and 6400)").SetExpert().SetDoc("sequence of zoom levels when zooming in/out; all values must lie between 8.33 and 6400"),
		MkField("ZoomIncrement", Float, 0,
			"zoom step size in percents relative to the current zoom level. "+
				"if zero or negative, the values from ZoomLevels are used instead").SetExpert(),
		EmptyLine(),

		// the below prefs apply only to FixedPageUI and ComicBookUI (so far)
		MkStruct("PrinterDefaults", PrinterDefaults,
			"these override the default settings in the Print dialog").SetExpert(),
		MkStruct("ForwardSearch", ForwardSearch,
			"customization options for how we show forward search results (used from "+
				"LaTeX editors)").SetExpert(),
		MkStruct("AnnotationDefaults", AnnotationDefaults,
			"default values for user added annotations in FixedPageUI documents "+
				"(preliminary and still subject to change)").SetExpert().SetPreRelease(),
		MkCompactArray("DefaultPasswords", String, nil,
			"passwords to try when opening a password protected document").SetDoc("a whitespace separated list of passwords to try when opening a password protected document " +
			"(passwords containing spaces must be quoted)").SetExpert().SetVersion("2.4"),
		MkField("CustomScreenDPI", Int, 0,
			"actual resolution of the main screen in DPI (if this value "+
				" isn't positive, the system's UI setting is used)").SetExpert().SetVersion("2.5"),
		EmptyLine(),

		MkField("RememberStatePerDocument", Bool, True,
			"if true, we store display settings for each document separately (i.e. everything "+
				"after UseDefaultState in FileStates)"),
		MkField("UiLanguage", Utf8String, nil,
			"ISO code of the current UI language").SetDoc("[ISO code](langs.html) of the current UI language"),
		MkField("ShowToolbar", Bool, True,
			"if true, we show the toolbar at the top of the window"),
		MkField("ShowFavorites", Bool, False,
			"if true, we show the Favorites sidebar"),
		MkField("AssociatedExtensions", String, nil,
			"a list of extensions that SumatraPDF has associated itself with and will "+
				"reassociate if a different application takes over (e.g. \".pdf .xps .epub\")"),
		MkField("AssociateSilently", Bool, False,
			"whether file associations should be fixed silently or only after user feedback"),
		MkField("CheckForUpdates", Bool, True,
			"if true, we check once a day if an update is available"),
		MkField("VersionToSkip", String, nil,
			"we won't ask again to update to this version"),
		MkField("RememberOpenedFiles", Bool, True,
			"if true, we remember which files we opened and their display settings"),
		MkField("InverseSearchCmdLine", String, nil,
			"pattern used to launch the LaTeX editor when doing inverse search"),
		MkField("EnableTeXEnhancements", Bool, False,
			"if true, we expose the SyncTeX inverse search command line in Settings -> Options"),
		MkField("DefaultDisplayMode", String, "automatic",
			"how pages should be laid out by default, needs to be synchronized with "+
				"DefaultDisplayMode after deserialization and before serialization").SetDoc("default layout of pages. valid values: automatic, single page, facing, " +
			"book view, continuous, continuous facing, continuous book view"),
		MkField("DefaultZoom", Utf8String, "fit page",
			"default zoom (in %) or one of those values: fit page, fit width, fit content"),
		MkField("WindowState", Int, 1,
			"default state of new windows (same as the last closed)").SetDoc("default state of the window. 1 is normal, 2 is maximized, " +
			"3 is fullscreen, 4 is minimized"),
		MkCompactStruct("WindowPos", WindowPos,
			"default position (can be on any monitor)").SetStructName("RectI").SetDoc("default position (x, y) and size (width, height) of the window"),
		MkField("ShowToc", Bool, True,
			"if true, we show table of contents (Bookmarks) sidebar if it's present "+
				"in the document"),
		MkField("SidebarDx", Int, 0,
			"width of favorites/bookmarks sidebar (if shown)"),
		MkField("TocDy", Int, 0,
			"if both favorites and bookmarks parts of sidebar are visible, this is "+
				"the height of bookmarks (table of contents) part"),
		MkField("ShowStartPage", Bool, True,
			"if true, we show a list of frequently read documents when no document is loaded"),
		MkField("UseTabs", Bool, True,
			"if true, documents are opened in tabs instead of new windows").SetVersion("3.0"),
		EmptyLine(),

		// file history and favorites
		MkArray("FileStates", FileSettings,
			"information about opened files (in most recently used order)"),
		MkArray("SessionData", SessionData,
			"state of the last session, usage depends on RestoreSession").SetVersion("3.1"),
		MkCompactArray("ReopenOnce", String, nil,
			"a list of paths for files to be reopened at the next start "+
				"or the string \"SessionData\" if this data is saved in SessionData "+
				"(needed for auto-updating)").SetDoc("data required for reloading documents after an auto-update").SetVersion("3.0"),
		MkCompactStruct("TimeOfLastUpdateCheck", FileTime,
			"timestamp of the last update check").SetStructName("FILETIME").SetDoc("data required to determine when SumatraPDF last checked for updates"),
		MkField("OpenCountWeek", Int, 0,
			"week count since 2011-01-01 needed to \"age\" openCount values in file history").SetDoc("value required to determine recency for the OpenCount value in FileStates"),
		// non-serialized fields
		MkCompactStruct("LastPrefUpdate", FileTime,
			"modification time of the preferences file when it was last read").SetStructName("FILETIME").SetInternal(),
		MkField("DefaultDisplayModeEnum", &Type{"", "DisplayMode"}, "DM_AUTOMATIC",
			"value of DefaultDisplayMode for internal usage").SetInternal(),
		MkField("DefaultZoomFloat", Float, -1,
			"value of DefaultZoom for internal usage").SetInternal(),
		EmptyLine(),
		MkComment("Settings after this line have not been recognized by the current version"),
	}

	GlobalPrefsStruct = MkStruct("GlobalPrefs", GlobalPrefs,
		"Most values on this structure can be updated through the UI and are persisted "+
			"in SumatraPDF-settings.txt")
)

// limit comment lines to 72 chars
func FormatComment(comment string, start string) []string {
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

func FormatArrayLines(data [][]string) []string {
	var lines []string
	for _, ld := range data {
		s := fmt.Sprintf("\t{ %s, %s, %s },", ld[0], ld[1], ld[2])
		lines = append(lines, s)
	}
	return lines
}

func BuildStruct(struc *Field, built map[string]int) string {
	lines := []string{}
	required := []string{}
	var s string
	if struc.Comment != "" {
		comments := FormatComment(struc.Comment, "//")
		lines = append(lines, comments...)
	}
	s = fmt.Sprintf("struct %s {", struc.StructName)
	lines = append(lines, s)
	fields := struc.Default.([]*Field)
	for _, field := range fields {
		if field.isComment() {
			continue
		}
		comments := FormatComment(field.Comment, "\t//")
		lines = append(lines, comments...)
		s = fmt.Sprintf("\t%s %s;", field.Type.Ctype, field.CName)
		lines = append(lines, s)
		switch field.Type.Name {
		case "Struct", "Compact", "Array", "Prerelease":
			name := field.Name
			if name == field.StructName || name == field.StructName+"s" {
				if _, ok := built[name]; !ok {
					s = BuildStruct(field, built)
					required = append(required, s)
					required = append(required, "")
					built[name]++
				}
			}
		}
	}
	lines = append(lines, "};")
	s1 := strings.Join(required, "\n")
	s2 := strings.Join(lines, "\n")
	return s1 + s2
}

func BuildMetaData(struc *Field, built map[string]int) string {
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
		s = fmt.Sprintf("Type_%s", tpName)
		dataLine = append(dataLine, s)
		s = field.cdefault(built)
		dataLine = append(dataLine, s)

		names = append(names, field.Name)
		switch field.Type.Name {
		case "Struct", "Compact", "Array":
			sublines := BuildMetaData(field, built) // TODO: pass built?
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
	dataLines := FormatArrayLines(data)
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

const SettingsStructsHeader = `/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 (see COPYING) */

// !!!!! This file is auto-generated by do/gen_settings_structs.go

enum DisplayMode {
	// automatic means: the continuous form of single page, facing or
	// book view - depending on the document's desired PageLayout
	DM_AUTOMATIC,
	DM_SINGLE_PAGE,
	DM_FACING,
	DM_BOOK_VIEW,
	DM_CONTINUOUS,
	DM_CONTINUOUS_FACING,
	DM_CONTINUOUS_BOOK_VIEW,
};

class RenderedBitmap;

typedef struct FileState DisplayState;

{{structDef}}

#ifdef INCLUDE_SETTINGSSTRUCTS_METADATA

{{structMetadata}}

#endif
`

func genSettingsStruct() string {
	built := map[string]int{}
	structDef := BuildStruct(GlobalPrefsStruct, built)
	structMetaData := BuildMetaData(GlobalPrefsStruct, map[string]int{})

	content := SettingsStructsHeader
	content = strings.Replace(content, "{{structDef}}", structDef, -1)
	content = strings.Replace(content, "{{structMetadata}}", structMetaData, -1)
	return content
}

func genAndSaveSettingsStructs() {
	helpURI := fmt.Sprintf("For documentation, see https://www.sumatrapdfreader.org/settings%s.html", extractSumatraVersionMust())

	GlobalPrefs[0].Comment = helpURI

	s := genSettingsStruct()

	// TODO: port this
	/*
		beforeUseDefaultState = True
		for field in FileSettings:
			if field.name == "UseDefaultState":
				beforeUseDefaultState = False
			elif beforeUseDefaultState:
				assert field.name not in rememberedDisplayState, "%s shouldn't be serialized when UseDefaultState is true" % field.name
			else:
				assert field.name in rememberedDisplayState or field.internal, "%s won't be serialized when UseDefaultState is true" % field.name
	*/

	//fmt.Printf("%s\n", s)
	s = strings.Replace(s, "\n", "\r\n", -1)
	s = strings.Replace(s, "\t", "    ", -1)
	path := filepath.Join("src", "SettingsStructs2.h")
	u.WriteFileMust(path, []byte(s))
	detectClangFormat()
	clangFormatFile(path)

	fmt.Printf("Wrote '%s'\n", path)
}
