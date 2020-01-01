#!/usr/bin/env python

"""
This script generates structs and enough metadata for reading
a variety of preference values from user provided settings files.
See further below for the definition of all currently supported options.
"""

import os
import util

class Type(object):
	def __init__(self, name, ctype):
		self.name = name; self.ctype = ctype

Bool = Type("Bool", "bool")
Color = Type("Color", "COLORREF")
Float = Type("Float", "float")
Int = Type("Int", "int")
String = Type("String", "WCHAR*")
Utf8String = Type("Utf8String", "char*")

class Field(object):
	def __init__(self, name, type, default, comment, internal=False, expert=False, doc=None, version=None, prerelease=False):
		self.name = name; self.type = type; self.default = default; self.comment = comment
		self.internal = internal; self.cname = name[0].lower() + name[1:] if name else None
		self.expert = expert # "expert" prefs are the ones not exposed by the UI
		self.docComment = doc or comment
		self.version = version or "2.3" # version in which this setting was introduced
		self.prerelease = prerelease # prefs which aren't written out in release builds

	def cdefault(self, built):
		if self.type is Bool:
			return "true" if self.default else "false"
		if self.type is Color:
			return "0x%06x" % self.default
		if self.type is Float:
			return '(intptr_t)"%g"' % self.default # converting float to intptr_t rounds the value
		if self.type is Int:
			return "%d" % self.default
		if self.type is String:
			return '(intptr_t)L"%s"' % self.default if self.default is not None else "0"
		if self.type is Utf8String:
			return '(intptr_t)"%s"' % self.default.encode("utf-8") if self.default is not None else "0"
		if self.type.name in ["Struct", "Array", "Compact", "Prerelease"]:
			id = built.count(self.structName)
			return "(intptr_t)&g%sInfo" % (self.structName + ("" if not id else "_%d_" % id))
		if self.type.name in ["ColorArray", "FloatArray", "IntArray"]:
			return '(intptr_t)"%s"' % self.default if self.default is not None else "0"
		if self.type.name == "StringArray":
			return '(intptr_t)"%s"' % self.default.encode("utf-8") if self.default is not None else "0"
		if self.type.name == "Comment":
			return '(intptr_t)"%s"' % self.comment.encode("utf-8") if self.comment is not None else "0"
		return None

	def inidefault(self, commentChar=";"):
		if self.type is Bool:
			return "%s = %s" % (self.name, "true" if self.default else "false")
		if self.type is Color:
			return "%s = #%02x%02x%02x" % (self.name, self.default & 0xFF, (self.default >> 8) & 0xFF, (self.default >> 16) & 0xFF)
		if self.type is Float:
			return "%s = %g" % (self.name, self.default)
		if self.type is Int:
			return "%s = %d" % (self.name, self.default)
		if self.type is String:
			if self.default is not None:
				return "%s = %s" % (self.name, self.default.encode("UTF-8"))
			return "%s %s =" % (commentChar, self.name)
		if self.type is Utf8String:
			if self.default is not None:
				return "%s = %s" % (self.name, self.default)
			return "%s %s =" % (commentChar, self.name)
		if self.type.name == "Compact":
			return "%s = %s" % (self.name, " ".join(field.inidefault().split(" = ", 1)[1] for field in self.default))
		if self.type.name in ["ColorArray", "FloatArray", "IntArray"]:
			if self.default is not None:
				return "%s = %s" % (self.name, self.default)
			return "%s %s =" % (commentChar, self.name)
		if self.type.name == "StringArray":
			if self.default is not None:
				return "%s = %s" % (self.name, self.default.encode("UTF-8"))
			return "%s %s =" % (commentChar, self.name)
		assert False

class Struct(Field):
	def __init__(self, name, fields, comment, structName=None, internal=False, expert=False, doc=None, version=None, prerelease=False):
		self.structName = structName or name
		super(Struct, self).__init__(name, Type("Struct", "%s" % self.structName), fields, comment, internal, expert, doc, version, prerelease)
		if prerelease: self.type.name = "Prerelease"

class CompactStruct(Struct):
	def __init__(self, name, fields, comment, structName=None, internal=False, expert=False, doc=None, version=None):
		super(CompactStruct, self).__init__(name, fields, comment, structName, internal, expert, doc, version)
		self.type.name = "Compact"

class Array(Field):
	def __init__(self, name, fields, comment, structName=None, internal=False, expert=False, doc=None, version=None):
		self.structName = structName or name
		if not structName and name.endswith("s"):
			# trim plural 's' from struct name
			self.structName = name[:-1]
		super(Array, self).__init__(name, Type("Array", "Vec<%s*>*" % self.structName), fields, comment, internal, expert, doc, version)

class CompactArray(Field):
	def __init__(self, name, type, default, comment, internal=False, expert=False, doc=None, version=None):
		super(CompactArray, self).__init__(name, Type("%sArray" % type.name, "Vec<%s>*" % type.ctype), default, comment, internal, expert, doc, version)

class Comment(Field):
	def __init__(self, comment, expert=False, version=None):
		super(Comment, self).__init__("", Type("Comment", None), None, comment, False, expert)

def EmptyLine(expert=False):
	return Comment(None, expert)

def RGB(r, g, b, a=0):
	return r | (g << 8) | (b << 16) | (a << 24)

# ##### setting definitions for SumatraPDF #####

WindowPos = [
	Field("X", Int, 0, "x coordinate"),
	Field("Y", Int, 0, "y coordinate"),
	Field("Dx", Int, 0, "width"),
	Field("Dy", Int, 0, "height"),
]

ScrollPos = [
	Field("X", Int, 0, "x coordinate"),
	Field("Y", Int, 0, "y coordinate"),
]

FileTime = [
	Field("DwHighDateTime", Int, 0, ""),
	Field("DwLowDateTime", Int, 0, ""),
]

PrinterDefaults = [
	Field("PrintScale", Utf8String, "shrink", "default value for scaling (shrink, fit, none)"),
]

ForwardSearch = [
	Field("HighlightOffset", Int, 0,
		"when set to a positive value, the forward search highlight style will " +
		"be changed to a rectangle at the left of the page (with the indicated " +
		"amount of margin from the page margin)"),
	Field("HighlightWidth", Int, 15,
		"width of the highlight rectangle (if HighlightOffset is > 0)"),
	Field("HighlightColor", Color, RGB(0x65, 0x81, 0xFF),
		"color used for the forward search highlight"),
	Field("HighlightPermanent", Bool, False,
		"if true, highlight remains visible until the next mouse click " +
		"(instead of fading away immediately)"),
]

WindowMargin_FixedPageUI = [
	Field("Top", Int, 2, "size of the top margin between window and document"),
	Field("Right", Int, 4, "size of the right margin between window and document"),
	Field("Bottom", Int, 2, "size of the bottom margin between window and document"),
	Field("Left", Int, 4, "size of the left margin between window and document"),
]

WindowMargin_ComicBookUI = [
	Field("Top", Int, 0, "size of the top margin between window and document"),
	Field("Right", Int, 0, "size of the right margin between window and document"),
	Field("Bottom", Int, 0, "size of the bottom margin between window and document"),
	Field("Left", Int, 0, "size of the left margin between window and document"),
]

PageSpacing = [
	Field("Dx", Int, 4, "horizontal difference"),
	Field("Dy", Int, 4, "vertical difference"),
]

FixedPageUI = [
	Field("TextColor", Color, RGB(0x00, 0x00, 0x00),
		"color value with which black (text) will be substituted"),
	Field("BackgroundColor", Color, RGB(0xFF, 0xFF, 0xFF),
		"color value with which white (background) will be substituted"),
	Field("SelectionColor", Color, RGB(0xF5, 0xFC, 0x0C),
		"color value for the text selection rectangle (also used to highlight found text)", version="2.4"),
	CompactStruct("WindowMargin", WindowMargin_FixedPageUI,
		"top, right, bottom and left margin (in that order) between window and document"),
	CompactStruct("PageSpacing", PageSpacing,
		"horizontal and vertical distance between two pages in facing and book view modes",
		structName="SizeI"),
	CompactArray("GradientColors", Color, None, # "#2828aa #28aa28 #aa2828",
		"colors to use for the gradient from top to bottom (stops will be inserted " +
		"at regular intervals throughout the document); currently only up to three " +
		"colors are supported; the idea behind this experimental feature is that the " +
		"background might allow to subconsciously determine reading progress; " +
		"suggested values: #2828aa #28aa28 #aa2828"),
	Field("InvertColors", Bool, False,
		"if true, TextColor and BackgroundColor will be temporarily swapped",
		internal=True),
]

EbookUI = [
	# default serif font, a different font is used for monospaced text (currently always "Courier New")
	Field("FontName", String, "Georgia", "name of the font. takes effect after re-opening the document"),
	Field("FontSize", Float, 12.5, "size of the font. takes effect after re-opening the document"),
	Field("TextColor", Color, RGB(0x5F, 0x4B, 0x32), "color for text"),
	Field("BackgroundColor", Color, RGB(0xFB, 0xF0, 0xD9), "color of the background (page)"),
	Field("UseFixedPageUI", Bool, False,
		"if true, the UI used for PDF documents will be used for ebooks as well " +
		"(enables printing and searching, disables automatic reflow)"),
]

ComicBookUI = [
	CompactStruct("WindowMargin", WindowMargin_ComicBookUI,
		"top, right, bottom and left margin (in that order) between window and document"),
	CompactStruct("PageSpacing", PageSpacing,
		"horizontal and vertical distance between two pages in facing and book view modes",
		structName="SizeI"),
	Field("CbxMangaMode", Bool, False,
		"if true, default to displaying Comic Book files in manga mode (from right to left if showing 2 pages at a time)"),
]

ChmUI = [
	Field("UseFixedPageUI", Bool, False,
		"if true, the UI used for PDF documents will be used for CHM documents as well"),
]

ExternalViewer = [
	Field("CommandLine", String, None,
		"command line with which to call the external viewer, may contain " +
		"%p for page number and \"%1\" for the file name (add quotation " +
		"marks around paths containing spaces)"),
	Field("Name", String, None,
		"name of the external viewer to be shown in the menu (implied by CommandLine if missing)"),
	Field("Filter", String, None,
		"optional filter for which file types the menu item is to be shown; separate multiple entries using ';' and don't include any spaces (e.g. *.pdf;*.xps for all PDF and XPS documents)"),
]

AnnotationDefaults = [
	Field("HighlightColor", Color, RGB(0xFF, 0xFF, 0x60),
		"color used for the highlight tool (in prerelease builds, the current selection " +
		"can be converted into a highlight annotation by pressing the 'h' key)"),
	Field("SaveIntoDocument", Bool, True,
		"if true, annotations are appended to PDF documents, " +
		"else they're always saved to an external .smx file"),
]

Favorite = [
	Field("Name", String, None,
		"name of this favorite as shown in the menu"),
	Field("PageNo", Int, 0,
		"number of the bookmarked page"),
	Field("PageLabel", String, None,
		"label for this page (only present if logical and physical page numbers are not the same)"),
	Field("MenuId", Int, 0,
		"id of this favorite in the menu (assigned by AppendFavMenuItems)",
		internal=True),
]

FileSettings = [
	Field("FilePath", String, None,
		"path of the document"),
	Array("Favorites", Favorite,
		"Values which are persisted for bookmarks/favorites"),
	Field("IsPinned", Bool, False,
		"a document can be \"pinned\" to the Frequently Read list so that it " +
		"isn't displaced by recently opened documents"),
	Field("IsMissing", Bool, False,
		"if a document can no longer be found but we still remember valuable state, " +
		"it's classified as missing so that it can be hidden instead of removed",
		doc="if true, the file is considered missing and won't be shown in any list"),
	Field("OpenCount", Int, 0,
		"in order to prevent documents that haven't been opened for a while " +
		"but used to be opened very frequently constantly remain in top positions, " +
		"the openCount will be cut in half after every week, so that the " +
		"Frequently Read list hopefully better reflects the currently relevant documents",
		doc="number of times this document has been opened recently"),
	Field("DecryptionKey", Utf8String, None,
		"Hex encoded MD5 fingerprint of file content (32 chars) followed by " +
		"crypt key (64 chars) - only applies for PDF documents",
		doc="data required to open a password protected document without having to " +
		"ask for the password again"),
	Field("UseDefaultState", Bool, False,
		"if true, we use global defaults when opening this file (instead of " +
		"the values below)"),
	# NOTE: fields below UseDefaultState aren't serialized if UseDefaultState is true!
	Field("DisplayMode", String, "automatic",
		"how pages should be laid out for this document, needs to be synchronized with " +
		"DefaultDisplayMode after deserialization and before serialization",
		doc="layout of pages. valid values: automatic, single page, facing, book view, " +
		"continuous, continuous facing, continuous book view"),
	CompactStruct("ScrollPos", ScrollPos,
		"how far this document has been scrolled (in x and y direction)",
		structName="PointI"),
	Field("PageNo", Int, 1,
		"number of the last read page"),
	Field("Zoom", Utf8String, "fit page",
		"zoom (in %) or one of those values: fit page, fit width, fit content"),
	Field("Rotation", Int, 0,
		"how far pages have been rotated as a multiple of 90 degrees"),
	Field("WindowState", Int, 0,
		"state of the window. 1 is normal, 2 is maximized, "+
		"3 is fullscreen, 4 is minimized"),
	CompactStruct("WindowPos", WindowPos,
		"default position (can be on any monitor)", structName="RectI"),
	Field("ShowToc", Bool, True,
		"if true, we show table of contents (Bookmarks) sidebar if it's present " +
		"in the document"),
	Field("SidebarDx", Int, 0,
		"width of the left sidebar panel containing the table of contents"),
	Field("DisplayR2L", Bool, False,
		"if true, the document is displayed right-to-left in facing and book view modes " +
		"(only used for comic book documents)"),
	Field("ReparseIdx", Int, 0,
		"index into an ebook's HTML data from which reparsing has to happen " +
		"in order to restore the last viewed page (i.e. the equivalent of PageNo for the ebook UI)",
		doc="data required to restore the last read page in the ebook UI"),
	CompactArray("TocState", Int, None,
		"tocState is an array of ids for ToC items that have been toggled by " +
		"the user (i.e. aren't in their default expansion state). - " +
		"Note: We intentionally track toggle state as opposed to expansion state " +
		"so that we only have to save a diff instead of all states for the whole " +
		"tree (which can be quite large) (internal)",
		doc="data required to determine which parts of the table of contents have been expanded"),
	# NOTE: fields below UseDefaultState aren't serialized if UseDefaultState is true!
	Field("Thumbnail", Type(None, "RenderedBitmap *"), "NULL",
		"thumbnails are saved as PNG files in sumatrapdfcache directory",
		internal=True),
	Field("Index", Type(None, "size_t"), "0",
		"temporary value needed for FileHistory::cmpOpenCount",
		internal=True),
]

# list of fields which aren't serialized when UseDefaultState is set
rememberedDisplayState = ["DisplayMode", "ScrollPos", "PageNo", "Zoom", "Rotation", "WindowState", "WindowPos", "ShowToc", "SidebarDx", "DisplayR2L", "ReparseIdx", "TocState"]

TabState = [
	Field("FilePath", String, None,
		"path of the document"),
	Field("DisplayMode", String, "automatic",
		"same as FileStates -> DisplayMode"),
	Field("PageNo", Int, 1,
		"number of the last read page"),
	Field("Zoom", Utf8String, "fit page",
		"same as FileStates -> Zoom"),
	Field("Rotation", Int, 0,
		"same as FileStates -> Rotation"),
	CompactStruct("ScrollPos", ScrollPos,
		"how far this document has been scrolled (in x and y direction)",
		structName="PointI"),
	Field("ShowToc", Bool, True,
		"if true, the table of contents was shown when the document was closed"),
	CompactArray("TocState", Int, None,
		"same as FileStates -> TocState"),
]

SessionData = [
	Array("TabStates", TabState,
		"a subset of FileState required for restoring the state of a single tab " +
		"(required for handling documents being opened twice)",
		doc="data required for restoring the view state of a single tab"),
	Field("TabIndex", Int, 1, "index of the currently selected tab (1-based)"),
	Field("WindowState", Int, 0,
		"same as FileState -> WindowState"),
	CompactStruct("WindowPos", WindowPos,
		"default position (can be on any monitor)", structName="RectI"),
	Field("SidebarDx", Int, 0,
		"width of favorites/bookmarks sidebar (if shown)"),
]

GlobalPrefs = [
	Comment("For documentation, see https://www.sumatrapdfreader.org/settings%s.html" % util.get_sumatrapdf_version()),
	EmptyLine(),

	Field("MainWindowBackground", Color, RGB(0xFF, 0xF2, 0x00, a=0x80),
		"background color of the non-document windows, traditionally yellow",
		expert=True),

	# Field("ThemeName", Utf8String, "light",
	#	"the name of the theme to use"),

	Field("EscToExit", Bool, False,
		"if true, Esc key closes SumatraPDF",
		expert=True),
	Field("ReuseInstance", Bool, False,
		"if true, we'll always open files using existing SumatraPDF process",
		expert=True),
	Field("UseSysColors", Bool, False,
		"if true, we use Windows system colors for background/text color. Over-rides other settings",
		expert=True),
	Field("RestoreSession", Bool, True,
		"if true and SessionData isn't empty, that session will be restored at startup",
		expert=True),
	Field("TabWidth", Int, 300,
		"maximum width of a single tab"),
	EmptyLine(),

	Struct("FixedPageUI", FixedPageUI,
		"customization options for PDF, XPS, DjVu and PostScript UI",
		expert=True),
	Struct("EbookUI", EbookUI,
		"customization options for eBooks (EPUB, Mobi, FictionBook) UI. If UseFixedPageUI is true, FixedPageUI settings apply instead",
		expert=True),
	Struct("ComicBookUI", ComicBookUI,
		"customization options for Comic Book and images UI",
		expert=True),
	Struct("ChmUI", ChmUI,
		"customization options for CHM UI. If UseFixedPageUI is true, FixedPageUI settings apply instead",
		expert=True),
	Array("ExternalViewers", ExternalViewer,
		"list of additional external viewers for various file types " +
		"(can have multiple entries for the same format)",
		expert=True),
	Field("ShowMenubar", Bool, True,
		"if false, the menu bar will be hidden for all newly opened windows " +
		"(use F9 to show it until the window closes or Alt to show it just briefly), only applies if UseTabs is false",
		expert=True, version="2.5"),
	Field("ReloadModifiedDocuments", Bool, True,
		"if true, a document will be reloaded automatically whenever it's changed " +
		"(currently doesn't work for documents shown in the ebook UI)",
		expert=True, version="2.5"),
	Field("FullPathInTitle", Bool, False,
		"if true, we show the full path to a file in the title bar",
		expert=True, version="3.0"),
	# the below prefs don't apply to EbookUI (so far)
	CompactArray("ZoomLevels", Float, "8.33 12.5 18 25 33.33 50 66.67 75 100 125 150 200 300 400 600 800 1000 1200 1600 2000 2400 3200 4800 6400",
		"zoom levels which zooming steps through in addition to Fit Page, Fit Width and " +
		"the minimum and maximum allowed values (8.33 and 6400)",
		expert=True,
		doc="sequence of zoom levels when zooming in/out; all values must lie between 8.33 and 6400"),
	Field("ZoomIncrement", Float, 0,
		"zoom step size in percents relative to the current zoom level. " +
		"if zero or negative, the values from ZoomLevels are used instead",
		expert=True),
	EmptyLine(),

	# the below prefs apply only to FixedPageUI and ComicBookUI (so far)
	Struct("PrinterDefaults", PrinterDefaults,
		"these override the default settings in the Print dialog",
		expert=True),
	Struct("ForwardSearch", ForwardSearch,
		"customization options for how we show forward search results (used from " +
		"LaTeX editors)",
		expert=True),
	Struct("AnnotationDefaults", AnnotationDefaults,
		"default values for user added annotations in FixedPageUI documents " +
		"(preliminary and still subject to change)",
		expert=True, prerelease=True),
	CompactArray("DefaultPasswords", String, None,
		"passwords to try when opening a password protected document",
		doc="a whitespace separated list of passwords to try when opening a password protected document " +
		"(passwords containing spaces must be quoted)",
		expert=True, version="2.4"),
	Field("CustomScreenDPI", Int, 0,
		"actual resolution of the main screen in DPI (if this value " +
		" isn't positive, the system's UI setting is used)",
		expert=True, version="2.5"),
	EmptyLine(),

	Field("RememberStatePerDocument", Bool, True,
		"if true, we store display settings for each document separately (i.e. everything " +
		"after UseDefaultState in FileStates)"),
	Field("UiLanguage", Utf8String, None,
		"ISO code of the current UI language",
		doc="[ISO code](langs.html) of the current UI language"),
	Field("ShowToolbar", Bool, True,
		"if true, we show the toolbar at the top of the window"),
	Field("ShowFavorites", Bool, False,
		"if true, we show the Favorites sidebar"),
	Field("AssociatedExtensions", String, None,
		"a list of extensions that SumatraPDF has associated itself with and will " +
		"reassociate if a different application takes over (e.g. \".pdf .xps .epub\")"),
	Field("AssociateSilently", Bool, False,
		"whether file associations should be fixed silently or only after user feedback"),
	Field("CheckForUpdates", Bool, True,
		"if true, we check once a day if an update is available"),
	Field("VersionToSkip", String, None,
		"we won't ask again to update to this version"),
	Field("RememberOpenedFiles", Bool, True,
		"if true, we remember which files we opened and their display settings"),
	Field("InverseSearchCmdLine", String, None,
		"pattern used to launch the LaTeX editor when doing inverse search"),
	Field("EnableTeXEnhancements", Bool, False,
		"if true, we expose the SyncTeX inverse search command line in Settings -> Options"),
	Field("DefaultDisplayMode", String, "automatic",
		"how pages should be laid out by default, needs to be synchronized with " +
		"DefaultDisplayMode after deserialization and before serialization",
		doc="default layout of pages. valid values: automatic, single page, facing, " +
		"book view, continuous, continuous facing, continuous book view"),
	Field("DefaultZoom", Utf8String, "fit page",
		"default zoom (in %) or one of those values: fit page, fit width, fit content"),
	Field("WindowState", Int, 1,
		"default state of new windows (same as the last closed)",
		doc="default state of the window. 1 is normal, 2 is maximized, "+
		"3 is fullscreen, 4 is minimized"),
	CompactStruct("WindowPos", WindowPos,
		"default position (can be on any monitor)", structName="RectI",
		doc="default position (x, y) and size (width, height) of the window"),
	Field("ShowToc", Bool, True,
		"if true, we show table of contents (Bookmarks) sidebar if it's present " +
		"in the document"),
	Field("SidebarDx", Int, 0,
		"width of favorites/bookmarks sidebar (if shown)"),
	Field("TocDy", Int, 0,
		"if both favorites and bookmarks parts of sidebar are visible, this is " +
		"the height of bookmarks (table of contents) part"),
	Field("ShowStartPage", Bool, True,
		"if true, we show a list of frequently read documents when no document is loaded"),
	Field("UseTabs", Bool, True,
		"if true, documents are opened in tabs instead of new windows",
		version="3.0"),
	EmptyLine(),

	# file history and favorites
	Array("FileStates", FileSettings,
		"information about opened files (in most recently used order)"),
	Array("SessionData", SessionData,
		"state of the last session, usage depends on RestoreSession",
		version="3.1"),
	CompactArray("ReopenOnce", String, None,
		"a list of paths for files to be reopened at the next start " +
		"or the string \"SessionData\" if this data is saved in SessionData " +
		"(needed for auto-updating)",
		doc="data required for reloading documents after an auto-update",
		version="3.0"),
	CompactStruct("TimeOfLastUpdateCheck", FileTime,
		"timestamp of the last update check", structName="FILETIME",
		doc="data required to determine when SumatraPDF last checked for updates"),
	Field("OpenCountWeek", Int, 0,
		"week count since 2011-01-01 needed to \"age\" openCount values in file history",
		doc="value required to determine recency for the OpenCount value in FileStates"),
	# non-serialized fields
	CompactStruct("LastPrefUpdate", FileTime,
		"modification time of the preferences file when it was last read",
		structName="FILETIME", internal=True),
	Field("DefaultDisplayModeEnum", Type(None, "DisplayMode"), "DM_AUTOMATIC",
		"value of DefaultDisplayMode for internal usage",
		internal=True),
	Field("DefaultZoomFloat", Float, -1,
		"value of DefaultZoom for internal usage",
		internal=True),
	EmptyLine(),
	Comment("Settings after this line have not been recognized by the current version"),
]

GlobalPrefs = Struct("GlobalPrefs", GlobalPrefs,
	"Most values on this structure can be updated through the UI and are persisted " +
	"in SumatraPDF-settings.txt (previously in sumatrapdfprefs.dat)")

# ##### end of setting definitions for SumatraPDF #####

def FormatComment(comment, start):
	result, parts, line = [], comment.split(), start
	while parts:
		while parts and (line == start or len(line + parts[0]) < 72):
			line += " " + parts.pop(0)
		result.append(line)
		line = start
	return result

def FormatArrayLine(data, fmt):
	maxs = [0] * len(data[0])
	fmts = fmt.split()
	data2 = []
	for item in data:
		assert len(item) == len(maxs) and len(fmts) == len(maxs)
		item2 = []
		for i in range(len(item)):
			item2.append(fmts[i] % item[i])
			maxs[i] = max(maxs[i], len(item2[-1]))
		data2.append(item2)
	for item in data2:
		for i in range(len(item)):
			# item[i] += " " * (maxs[i] - len(item[i]))
			item[i] += " "
		yield " ".join(item)

def BuildStruct(struct, built=[]):
	lines, required = ["struct %s {" % struct.structName], []
	if struct.comment:
		lines = FormatComment(struct.comment, "//") + lines
	for field in struct.default:
		if type(field) is Comment:
			continue
		lines += FormatComment(field.comment, "\t//")
		lines.append("\t%s %s;" % (field.type.ctype, field.cname))
		if type(field) in [Struct, CompactStruct, Array] and field.name in [field.structName, field.structName + "s"] and field.name not in built:
			required += [BuildStruct(field), ""]
			built.append(field.name)
	lines.append("};")
	return "\n".join(required + lines)

def BuildMetaData(struct, built=[]):
	lines, data, names = [], [], []
	fullName = struct.structName + ("" if struct.structName not in built else "_%d_" % built.count(struct.structName))
	for field in struct.default:
		if field.internal:
			continue
		data.append(("offsetof(%s, %s)" % (struct.structName, field.cname), "Type_%s" % field.type.name, field.cdefault(built)))
		names.append(field.name)
		if type(field) in [Struct, CompactStruct, Array]:
			lines += [BuildMetaData(field), ""]
			built.append(field.structName)
		elif type(field) is Comment:
			data[-1] = tuple(["(size_t)-1"] + list(data[-1][1:]))
	lines.append("static const FieldInfo g%sFields[] = {" % fullName)
	lines += ["\t{ %s }," % line for line in FormatArrayLine(data, "%s, %s, %s")]
	lines.append("};")
	# gFileStateInfo isn't const so that the number of fields can be changed at runtime (cf. UseDefaultState)
	lines.append("static %sStructInfo g%sInfo = { sizeof(%s), %d, g%sFields, \"%s\" };" % ("const " if fullName != "FileState" else "", fullName, struct.structName, len(names), fullName, "\\0".join(names)))
	return "\n".join(lines)

SettingsStructs_Header = """\
/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 (see COPYING) */

// This file is auto-generated by gen_settingsstructs.py

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

%(structDef)s

#ifdef INCLUDE_SETTINGSSTRUCTS_METADATA

%(structMetadata)s

#endif
"""

def detectClangFormat():
	path = os.path.join("c:\\", "Program Files (x86)", "Microsoft Visual Studio", "2019", "Community", "VC", "Tools", "Llvm", "bin", "clang-format.exe")
	print("path: '%s'" % path)
	if not os.path.exists(path):
		raise BaseException("clang-format not detected")
	return path

def gen():
	clangPath = detectClangFormat()

	structDef = BuildStruct(GlobalPrefs)
	structMetadata = BuildMetaData(GlobalPrefs)

	content = SettingsStructs_Header % locals()
	filePath = "src/SettingsStructs.h"
	open(filePath, "wb").write(content.replace("\n", "\r\n").replace("\t", "    "))

	beforeUseDefaultState = True
	for field in FileSettings:
		if field.name == "UseDefaultState":
			beforeUseDefaultState = False
		elif beforeUseDefaultState:
			assert field.name not in rememberedDisplayState, "%s shouldn't be serialized when UseDefaultState is true" % field.name
		else:
			assert field.name in rememberedDisplayState or field.internal, "%s won't be serialized when UseDefaultState is true" % field.name

	util.run_cmd_throw(clangPath, "-i", "-style=file", filePath)