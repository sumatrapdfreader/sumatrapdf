#!/usr/bin/env python

"""
This script generates structs and enough metadata for reading
a variety of preference values from user provided settings files.
See further below for the definition of all currently supported options.
"""

import util2

class Type(object):
	def __init__(self, name, ctype):
		self.name = name; self.ctype = ctype

Bool = Type("Bool", "bool")
Color = Type("Color", "COLORREF")
Float = Type("Float", "float")
Int = Type("Int", "int")
String = Type("String", "WCHAR *")
Utf8String = Type("Utf8String", "char *")

class Field(object):
	def __init__(self, name, type, default, comment, internal=False, expert=False, doc=None):
		self.name = name; self.type = type; self.default = default; self.comment = comment
		self.internal = internal; self.cname = name[0].lower() + name[1:] if name else None
		self.expert = expert # "expert" prefs are the ones not exposed by the UI
		self.docComment = doc or comment

	def cdefault(self, built):
		if self.type == Bool:
			return "true" if self.default else "false"
		if self.type == Color:
			return "0x%06x" % self.default
		if self.type == Float:
			return '(intptr_t)"%g"' % self.default # converting float to intptr_t rounds the value
		if self.type == Int:
			return "%d" % self.default
		if self.type == String:
			return '(intptr_t)L"%s"' % self.default if self.default is not None else "NULL"
		if self.type == Utf8String:
			return '(intptr_t)"%s"' % self.default if self.default is not None else "NULL"
		if self.type.name in ["Struct", "Array", "Compact"]:
			id = built.count(self.structName)
			return "(intptr_t)&g%sInfo" % (self.structName + ("" if not id else "_%d_" % id))
		if self.type.name in ["ColorArray", "FloatArray", "IntArray"]:
			return '(intptr_t)"%s"' % self.default if self.default is not None else "NULL"
		return None

	def inidefault(self, commentChar=";"):
		if self.type == Bool:
			return "%s = %s" % (self.name, "true" if self.default else "false")
		if self.type == Color:
			return "%s = #%02x%02x%02x" % (self.name, self.default & 0xFF, (self.default >> 8) & 0xFF, (self.default >> 16) & 0xFF)
		if self.type == Float:
			return "%s = %g" % (self.name, self.default)
		if self.type == Int:
			return "%s = %d" % (self.name, self.default)
		if self.type == String:
			if self.default is not None:
				return "%s = %s" % (self.name, self.default.encode("UTF-8"))
			return "%s %s =" % (commentChar, self.name)
		if self.type == Utf8String:
			if self.default is not None:
				return "%s = %s" % (self.name, self.default)
			return "%s %s =" % (commentChar, self.name)
		if self.type.name == "Compact":
			return "%s = %s" % (self.name, " ".join(field.inidefault().split(" = ", 1)[1] for field in self.default))
		if self.type.name in ["ColorArray", "FloatArray", "IntArray"]:
			if self.default is not None:
				return "%s = %s" % (self.name, self.default)
			return "%s %s =" % (commentChar, self.name)
		assert False

class Struct(Field):
	def __init__(self, name, fields, comment, structName=None, compact=False, internal=False, expert=False, doc=None):
		self.structName = structName or name
		super(Struct, self).__init__(name, Type("Struct", "%s" % self.structName), fields, comment, internal, expert, doc)
		if compact: self.type.name = "Compact"

class Array(Field):
	def __init__(self, name, fields, comment, structName=None, internal=False, expert=False):
		self.structName = structName or name
		if not structName and name.endswith("s"):
			# trim plural 's' from struct name
			self.structName = name[:-1]
		super(Array, self).__init__(name, Type("Array", "Vec<%s *> *" % self.structName), fields, comment, internal, expert)

class CompactArray(Field):
	def __init__(self, name, type, default, comment, internal=False, expert=False, doc=None):
		super(CompactArray, self).__init__(name, Type("%sArray" % type.name, "Vec<%s> *" % type.ctype), default, comment, internal, expert, doc)

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
	Field("PrintAsImage", Bool, False, "default value for the compatibility option"),
]

ForwardSearch = [
	Field("HighlightOffset", Int, 0,
		"when set to a positive value, the forward search highlight style will " +
		"be changed to a rectangle at the left of the page (with the indicated " +
		"amount of margin from the page margin)"),
	Field("HighlightWidth", Int, 15,
		"width of the highlight rectangle (if HighlightOffset is > 0)"),
	Field("HighlightColor", Color, 0x6581FF,
		"color used for the forward search highlight"),
	Field("HighlightPermanent", Bool, False,
		"if true, highlight remains visible until the next " +
		"mouse click (instead of fading away immediately)"),
]

WindowMargin_FixedPageUI = [
	Field("Top", Int, 2, "size of the top margin between window and document"),
	Field("Right", Int, 4, "size of the right margin between window and document"),
	Field("Bottom", Int, 2, "size of the bottom margin between window and document"),
	Field("Left", Int, 4, "size of the left margin between window and document"),
]

WindowMargin_ImageOnlyUI = [
	Field("Top", Int, 0, "size of the top margin between window and document"),
	Field("Right", Int, 0, "size of the right margin between window and document"),
	Field("Bottom", Int, 0, "size of the bottom margin between window and document"),
	Field("Left", Int, 0, "size of the left margin between window and document"),
]

PageSpacing = [
	Field("Dx", Int, 4, "horizontal difference"),
	Field("Dy", Int, 4, "vertical difference"),
]

# zeniko: move these two into it's own struct (FixedPageUI?) to match EbookUI?
# kjk: I think we need a scheme where we can set those settings per document
# type. For example, the best background color for comic books is black,
# which is not the best for other documents, so let's just generalize it
# We can either key those by a list of extensions (most flexible but more
# generates more settings) or we can create fixed categories (pdf, comic books,
# images, ebooks etc.). Or both.
# This could include more settings other than color
# (paddings?). In order to avoid repetition, we could have a "default"
# entry which then could be over-written for a given document type/extension
# (or a list of extensions/types)
# zeniko: extensions doesn't work as reliable as file type due to sniffing
# (also, when adding support for more document types, grouping by category should
# scale better); currently, there are three major groups:
# FixedPage (PDF, XPS, DjVu, etc.), ImageOnly (images and comic books) and Reflow/Ebook
FixedPageUI = [
	Field("TextColor", Color, 0x000000,
		"color value with which black (text) will be substituted"),
	Field("BackgroundColor", Color, 0xFFFFFF,
		"color value with which white (background) will be substituted"),
	Struct("WindowMargin", WindowMargin_FixedPageUI,
		"top, right, bottom and left margin (in that order) between window and document",
		compact=True, expert=True),
	Struct("PageSpacing", PageSpacing,
		"horizontal and vertical distance between two pages in facing and book view modes",
		structName="SizeI", compact=True),
	CompactArray("GradientColors", Color, None, # "#2828aa #28aa28 #aa2828",
		"colors to use for the gradient from top to bottom (stops will be inserted " +
		"at regular intervals throughout the document); currently only up to three " +
		"colors are supported; the idea behind this experimental feature is that the " +
		"background might allow to subconsciously determine reading progress; " +
		"suggested values: #2828aa #28aa28 #aa2828"),
]

EbookUI = [
	# kjk: don't have an alternative, but I'm not happy with this name
	# zeniko: DisableEbookUI? Disable? UseFixedPageSize? DisableReflow?
	# (or all the Disable options as Enable options with default true instead?)
	# kjk: AltEbookUI ? (Alt - short for alternative, as in "different from the default")
	# zeniko: "alternative" isn't that descriptive, if there's a FixedPageUI struct
	# (and a ImageOnlyUI one), then UseFixedPageUI might be be clearer which alternative is meant
	Field("UseFixedPageUI", Bool, False,
		"if true, the UI used for PDF documents will be used for ebooks as well " +
		"(enables printing and searching, disables automatic reflow)"),
	Field("TextColor", Color, 0x324b5f, "color for text"),
	Field("BackgroundColor", Color, 0xd9f0fb, "color of the background (page)"),
]

ImageOnlyUI = [
	Struct("WindowMargin", WindowMargin_ImageOnlyUI,
		"top, right, bottom and left margin (in that order) between window and document",
		compact=True, expert=True),
	Struct("PageSpacing", PageSpacing,
		"horizontal and vertical distance between two pages in facing and book view modes",
		structName="SizeI", compact=True),
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
		"%p for page numer and %1 for the file name"),
	Field("Name", String, None,
		"name of the external viewer to be shown in the menu (implied by CommandLine if missing)"),
	Field("Filter", String, None,
		"filter for which file types the menu item is to be shown (e.g. \"*.pdf;*.xps\"; \"*\" if missing)"),
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
	# kjk: I think this only applies to certain settings. Should those settings
	# be grouped in a separate struct and the name reflect that? How does it
	# interact with GlobalPrefsOnly?
	# zeniko: in the previous implementation, when UseGlobalValues was set, all
	# document specific settings weren't saved at all; that's no longer easily possible
	# This pref applies to: DisplayMode, ScrollPos, PageNo, ReparseIdx, Zoom, Rotation,
	# WindowState, WindowPos, ShowToc, SidebarDx, DisplayR2L and TocState
	Field("UseDefaultState", Bool, False,
		"if true, we use global defaults when opening this file (instead of " +
		"the values below)"),
	# NOTE: fields below UseDefaultState aren't serialized if UseDefaultState is true!
	Field("DisplayMode", String, "automatic",
		"how pages should be laid out for this document, needs to be synchronized with " +
		"DefaultDisplayMode after deserialization and before serialization",
		doc="layout of pages. valid values: automatic, single page, facing, book view, " +
		"continuous, continuous facing, continuous book view"),
	Struct("ScrollPos", ScrollPos,
		"how far this document has been scrolled (in x and y direction)",
		structName="PointI", compact=True),
	Field("PageNo", Int, 1,
		"number of the last read page"),
	Field("Zoom", Utf8String, "fit page",
		"zoom (in %) or one of those values: fit page, fit width, fit content"),
	Field("Rotation", Int, 0,
		"how far pages have been rotated as a multiple of 90 degrees"),
	Field("WindowState", Int, 0,
		"state of the window. 1 is normal, 2 is maximized, "+
		"3 is fullscreen, 4 is minimized"),
	Struct("WindowPos", WindowPos,
		"default position (can be on any monitor)", structName="RectI", compact=True),
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
rememberedFileSettings = ["DisplayMode", "ScrollPos", "PageNo", "Zoom", "Rotation", "WindowState", "WindowPos", "ShowToc", "SidebarDx", "DisplayR2L", "ReparseIdx", "TocState"]

GlobalPrefs = [
	Field("MainWindowBackground", Color, 0x8000F2FF,
		"background color of the non-document windows, traditionally yellow",
		expert=True),
	Field("EscToExit", Bool, False,
		"if true, Esc key closes SumatraPDF",
		expert=True),
	# kjk: SingleInstance is a common term for such functionality
	# zeniko: this pref is to make the -reuse-instance command line switch permanent
	# kjk: I understand that. It should be renamed as -single-instance
	Field("ReuseInstance", Bool, False,
		"if true, we'll always open files using existing SumatraPDF process",
		expert=True),
	Struct("FixedPageUI", FixedPageUI,
		"customization options for PDF, XPS, DjVu and PostScript UI",
		expert=True),
	Struct("EbookUI", EbookUI,
		"customization options for eBooks (EPUB, Mobi, FictionBook) UI. If UseFixedPageUI is true, FixedPageUI settings apply instead",
		expert=True),
	Struct("ComicBookUI", ImageOnlyUI,
		"customization options for Comic Book and images UI",
		expert=True),
	Struct("ChmUI", ChmUI,
		"customization options for CHM UI. If UseFixedPageUI is true, FixedPageUI settings apply instead",
		expert=True),
	Array("ExternalViewers", ExternalViewer,
		"list of additional external viewers for various file types " +
		"(can have multiple entries for the same format)",
		expert=True),
	# zeniko: the below prefs apply only to FixedPageUI and ImageOnlyUI (so far)
	CompactArray("ZoomLevels", Float, "8.33 12.5 18 25 33.33 50 66.67 75 100 125 150 200 300 400 600 800 1000 1200 1600 2000 2400 3200 4800 6400",
		"zoom levels which zooming steps through in addition to Fit Page, Fit Width and " +
		"the minimum and maximum allowed values (8.33 and 6400)",
		expert=True,
		doc="sequence of zoom levels when zooming in/out; all values must lie between 8.33 and 6400"),
	Field("ZoomIncrement", Float, 0,
		"zoom step size in percents relative to the current zoom level. " +
		"if zero or negative, the values from ZoomLevels are used instead",
		expert=True),
	Struct("PrinterDefaults", PrinterDefaults,
		"these override the default settings in the Print dialog",
		expert=True),
	Struct("ForwardSearch", ForwardSearch,
		"customization options for how we show forward search results (used from " +
		"LaTeX editors)",
		expert=True),

	Field("RememberStatePerDocument", Bool, True,
		"if true, we store display settings for each document separately (i.e. everything " +
		"after UseDefaultState in FileStates)"),
	# kjk: we need an "auto" value, which means "auto-detect". We shouldn't serializee
	# auto-detected language
	# zeniko: one issue with "auto": since that setting isn't exposed in the UI, this
	# can result in two unexpected behaviors for portable versions: either the UI language
	# unexpectedly changes when using it abroad; or if a user ever closed the Choose Language
	# dialog with OK, the language never again adapts to system changes
	Field("UiLanguage", Utf8String, None, # TODO: "auto"
		"ISO code of the current UI language",
		doc="[ISO code](langs.html) of the current UI language"),
	Field("ShowToolbar", Bool, True,
		"if true, we show the toolbar at the top of the window"),
	Field("ShowFavorites", Bool, False,
		"if true, we show the Favorites sidebar"),
	# zeniko: replace these two with AssociatedExtensions (String, default: empty,
	# might be e.g. ".pdf .xps") and CheckAssociationsAtStartup (Bool, default: true) ?
	# the semantics are good but "association" is rather technical term. A better
	# name would be more descriptive (UseSumatraToOpenThoseFiles, MakeSumatraDefaultAppFor ?)
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
	# kjk: probably should be removed as we'll provide a way to set colors explicitly
	# zeniko: no, this is a UI exposed option for people who prefer changing their
	# color scheme system wide
	# kjk: exposed how? My problem with this is that it has complex semantics.
	# First: what are "system colors"? Second: how does it interact with other ways
	# to set colors (i.e. who gets precedence)? Is there a better way to provide
	# such functionality?
	# zeniko: system colors are the ones you set in Windows' Appearance Settings dialog,
	# if they're not the usual black text on white background, then there's an option to
	# use "Windows' color scheme" in the Settings dialog, and if that option is checked,
	# system colors are used instead of TextColor/BackgroundColor
	Field("UseSysColors", Bool, False,
		"if true, we use Windows system colors for background/text color. Over-rides other settings"),
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
	Struct("WindowPos", WindowPos,
		"default position (can be on any monitor)", structName="RectI", compact=True,
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
	# file history and favorites
	Array("FileStates", FileSettings,
		"information about opened files (in most recently used order)"),
	Struct("TimeOfLastUpdateCheck", FileTime,
		"timestamp of the last update check",
		structName="FILETIME", compact=True,
		doc="data required to determine when SumatraPDF last checked for updates"),
	Field("OpenCountWeek", Int, 0,
		"week count since 2011-01-01 needed to \"age\" openCount values in file history",
		doc="value required to determine recency for the OpenCount value in FileStates"),
	# non-serialized fields
	Struct("LastPrefUpdate", FileTime,
		"modification time of the preferences file when it was last read",
		structName="FILETIME", compact=True, internal=True),
	Field("DefaultDisplayModeEnum", Type(None, "DisplayMode"), "DM_AUTOMATIC",
		"value of DefaultDisplayMode for internal usage",
		internal=True),
	Field("DefaultZoomFloat", Float, -1,
		"value of DefaultZoom for internal usage",
		internal=True),
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
		assert len(item) == len(maxs) and len(fmts) ==len(maxs)
		item2 = []
		for i in range(len(item)):
			item2.append(fmts[i] % item[i])
			maxs[i] = max(maxs[i], len(item2[-1]))
		data2.append(item2)
	for item in data2:
		for i in range(len(item)):
			item[i] += " " * (maxs[i] - len(item[i]))
		yield " ".join(item)

def BuildStruct(struct, built=[]):
	lines, required = ["struct %s {" % struct.structName], []
	if struct.comment:
		lines = FormatComment(struct.comment, "//") + lines
	for field in struct.default:
		lines += FormatComment(field.comment, "\t//")
		lines.append("\t%s %s;" % (field.type.ctype, field.cname))
		if type(field) in [Struct, Array] and field.name in [field.structName, field.structName + "s"] and field.name not in built:
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
		if type(field) in [Struct, Array]:
			lines += [BuildMetaData(field), ""]
			built.append(field.structName)
	lines.append("static const FieldInfo g%sFields[] = {" % fullName)
	lines += ["\t{ %s }," % line for line in FormatArrayLine(data, "%s, %s, %s")]
	lines.append("};")
	# gFileStateInfo isn't const so that the number of fields can be changed at runtime (cf. UseDefaultState)
	lines.append("static %sStructInfo g%sInfo = { sizeof(%s), %d, g%sFields, \"%s\" };" % ("const " if fullName != "FileState" else "", fullName, struct.structName, len(names), fullName, "\\0".join(names)))
	return "\n".join(lines)

def AssembleDefaults(struct, topLevelComment=None):
	lines, more = [], []
	if topLevelComment:
		lines += FormatComment(topLevelComment, ";") + [""]
	for field in struct.default:
		if field.internal:
			continue
		if topLevelComment and not field.expert:
			continue
		if type(field) in [Struct, Array] and not field.type.name == "Compact":
			assert topLevelComment
			more.append("\n".join(FormatComment(field.docComment, ";") + ["[%s]" % field.name, AssembleDefaults(field)]))
		else:
			lines += FormatComment(field.docComment, ";") + [field.inidefault()]
	if more:
		lines += [""] + more
	return "\n".join(lines) + "\n"

SettingsStructs_Header = """\
/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 (see COPYING) */

// This file is auto-generated by gen_settingsstructs.py

#ifndef SettingsStructs_h
#define SettingsStructs_h

%(structDef)s

#ifdef INCLUDE_SETTINGSSTRUCTS_METADATA

#include "SettingsUtil.h"

%(structMetadata)s

#endif

#endif
"""

def main():
	util2.chdir_top()

	structDef = BuildStruct(GlobalPrefs)
	structMetadata = BuildMetaData(GlobalPrefs)

	content = SettingsStructs_Header % locals()
	open("src/SettingsStructs.h", "wb").write(content.replace("\n", "\r\n").replace("\t", "    "))

	content = AssembleDefaults(GlobalPrefs,
		"You can use this file to modify experimental and expert settings not changeable " +
		"through the UI instead of modifying SumatraPDF-settings.txt directly. Just copy " +
		"this file alongside SumatraPDF-settings.txt and change the values below. " +
		"They will overwrite the corresponding settings in SumatraPDF-settings.txt at every startup.")
	content = "# Warning: This file only works for builds compiled with ENABLE_SUMATRAPDF_USER_INI !\n\n" + content
	open("docs/SumatraPDF-user.ini", "wb").write(content.replace("\n", "\r\n").encode("utf-8-sig"))
	
	beforeUseDefaultState = True
	for field in FileSettings:
		if field.name == "UseDefaultState":
			beforeUseDefaultState = False
		elif beforeUseDefaultState:
			assert field.name not in rememberedFileSettings, "%s shouldn't be serialized when UseDefaultState is true" % field.name
		else:
			assert field.name in rememberedFileSettings or field.internal, "%s won't be serialized when UseDefaultState is true" % field.name

if __name__ == "__main__":
	main()
