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
	def __init__(self, name, type, default, comment, internal=False):
		self.name = name; self.type = type; self.default = default; self.comment = comment
		self.internal = internal; self.cname = name[0].lower() + name[1:] if name else None

	def cdefault(self):
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
			return "(intptr_t)&g%sInfo" % self.structName
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
	def __init__(self, name, fields, comment, structName=None, compact=False, internal=False):
		self.structName = structName or name
		super(Struct, self).__init__(name, Type("Struct", "%s" % self.structName), fields, comment, internal)
		if compact: self.type.name = "Compact"

class Array(Field):
	def __init__(self, name, fields, comment, structName=None, internal=False):
		self.structName = structName or name
		if not structName and name.endswith("s"):
			# trim plural 's' from struct name
			self.structName = name[:-1]
		super(Array, self).__init__(name, Type("Array", "Vec<%s *> *" % self.structName), fields, comment, internal)

class CompactArray(Field):
	def __init__(self, name, type, default, comment, internal=False):
		super(CompactArray, self).__init__(name, Type("%sArray" % type.name, "Vec<%s> *" % type.ctype), default, comment, internal)

# ##### setting definitions for SumatraPDF #####

RectI = [
	Field("X", Int, 0, "x coordinate"),
	Field("Y", Int, 0, "y coordinate"),
	Field("Dx", Int, 0, "width"),
	Field("Dy", Int, 0, "height"),
]

PointI = [
	Field("X", Int, 0, "x coordinate"),
	Field("Y", Int, 0, "y coordinate"),
]

FileTime = [
	Field("DwHighDateTime", Int, 0, ""),
	Field("DwLowDateTime", Int, 0, ""),
]

# kjk: this is non-standard terminology. Let's adopt css padding terminology:
# top, right, bottom, left
# zeniko: there's outer and inner padding, though, so it'd rather have to be
# OuterPadding [ TopBottom, LeftRight ] and InnerPadding [ Vert, Horz ]
PagePadding = [
	Field("OuterX", Int, 4, "size of the left/right margin between window and document"),
	Field("OuterY", Int, 2, "size of the top/bottom margin between window and document"),
	Field("InnerX", Int, 4, "size of the horizontal margin between two pages"),
	Field("InnerY", Int, 4, "size of the vertical margin between two pages"),
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
		"the width of the highlight rectangle for when HighlightOffset is set"),
	Field("HighlightColor", Color, 0x6581FF,
		"the color used for the forward search highlight"),
	Field("HighlightPermanent", Bool, False,
		"whether the forward search highlight will remain visible until the next " +
		"mouse click instead of fading away instantly"),
]

EbookUI = [
	# kjk: don't have an alternative, but I'm not happy with this name
	# zeniko: DisableEbookUI? Disable? UseFixedPageSize? DisableReflow?
	# (or all the Disable options as Enable options with default true instead?)
	Field("TraditionalEbookUI", Bool, False,
		"whether the UI used for PDF documents will be used for ebooks as well " +
		"(enables printing and searching, disables automatic reflow)"),
	Field("TextColor", Color, 0x324b5f, "color for text"),
	Field("BackgroundColor", Color, 0xfbf0d9, "color of the background (page)"),
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

UserPrefs = [
	# kjk: we should order things in usefulness/likelihood of being changed order
	Field("MainWindowBackground", Color, 0x8000F2FF,
		"background color of the non-document windows, traditionally yellow"),
	Field("EscToExit", Bool, False,
		"whether the Esc key will exit SumatraPDF same as 'q'"),
	# zeniko: move these two into it's own struct (FixedPageUI?) to match EbookUI?
	Field("TextColor", Color, 0x000000,
		"color value with which black (text) will be substituted"),
	Field("BackgroundColor", Color, 0xFFFFFF,
		"color value with which white (background) will be substituted"),
	Field("ReuseInstance", Bool, False,
		"whether opening a new document should happen in an already running SumatraPDF " +
		"instance so that there's only one process and documents aren't opend twice"),
	Field("ZoomIncrement", Float, 0,
		"zoom step size in percents relative to the current zoom level " +
		"(if zero or negative, the values from ZoomLevels are used instead)"),
	CompactArray("ZoomLevels", Float, "8.33 12.5 18 25 33.33 50 66.67 75 100 125 150 200 300 400 600 800 1000 1200 1600 2000 2400 3200 4800 6400",
		"zoom levels which zooming steps through, excluding the virtual zoom levels " +
		"fit page, fit content and fit width (minimal allowed value is 8.33 and maximum "
		"allowed value is 6400)"),
	CompactArray("GradientColors", Color, None, # "#2828aa #28aa28 #aa2828",
		"colors to use for the gradient from top to bottom (stops will be inserted " +
		"at regular intervals throughout the document); currently only up to three " +
		"colors are supported; the idea behind this experimental feature is that the " +
		"background might allow to subconsciously determine reading progress; " +
		"suggested values: #2828aa #28aa28 #aa2828"),
	Struct("PrinterDefaults", PrinterDefaults,
		"these values allow to override the default settings in the Print dialog"),
	# kjk: should be compact
	# zeniko: only if it's split into two structs for outer and inner padding
	Struct("PagePadding", PagePadding,
		"these values allow to change how far apart pages are layed out"),
	Struct("ForwardSearch", ForwardSearch,
		"these values allow to customize how the forward search highlight appears"),
	Struct("EbookUI", EbookUI,
		"these values allow to customize the UI used for ebooks (with TraditionalEbookUI disabled)"),
	Array("ExternalViewers", ExternalViewer,
		"this list contains a list of additional external viewers for various file types " +
		"(multiple entries of the same format are recognised)"),
]

FileSettings = [
	Field("FilePath", String, None,
		"file path of the document"),
	Field("OpenCount", Int, 0,
		"in order to prevent documents that haven't been opened for a while " +
		"but used to be opened very frequently constantly remain in top positions, " +
		"the openCount will be cut in half after every week, so that the " +
		"Frequently Read list hopefully better reflects the currently relevant documents"),
	Field("IsPinned", Bool, False,
		"a user can \"pin\" a preferred document to the Frequently Read list " +
		"so that the document isn't replaced by more frequently used ones"),
	# kjk: should be marked as internal
	# zeniko: this would cause missing files to be checked at every startup again,
	# is that intended?
	Field("IsMissing", Bool, False,
		"if a document can no longer be found but we still remember valuable state, " +
		"it's classified as missing so that it can be hidden instead of removed"),
	Field("UseGlobalValues", Bool, False,
		"whether global defaults should be used when reloading this file instead of " +
		"the values listed below"),
	Field("DisplayMode", String, "automatic",
		"how pages should be layed out for this document, needs to be synchronized with " +
		"DefaultDisplayMode after deserialization and before serialization"),
	Struct("ScrollPos", PointI,
		"how far this document has been scrolled", structName="PointI", compact=True),
	Field("PageNo", Int, 1,
		"the scrollPos values are relative to the top-left corner of this page"),
	Field("ReparseIdx", Int, 0,
		"for bookmarking ebook files: offset of the current page reparse point within html"),
	Field("ZoomVirtual", Float, 100.0,
		"the current zoom factor in % (negative values indicate virtual settings)"),
	Field("Rotation", Int, 0,
		"how far pages have been rotated as a multiple of 90 degrees"),
	Field("WindowState", Int, 0,
		"default state of new SumatraPDF windows (same as the last closed)"),
	Struct("WindowPos", RectI,
		"default position (can be on any monitor)", structName="RectI", compact=True),
	Field("DecryptionKey", Utf8String, None,
		"hex encoded MD5 fingerprint of file content (32 chars) followed by " +
		"crypt key (64 chars) - only applies for PDF documents"),
	# kjk: ShowToc
	# zeniko: that's what it used to be before you changed it to TocVisible in r3696
	Field("TocVisible", Bool, True,
		"whether the table of contents (Bookmarks) sidebar is shown for this document"),
	Field("SidebarDx", Int, 0,
		"the width of the left sidebar panel containing the table of contents"),
	CompactArray("TocState", Int, None,
		"tocState is an array of ids for ToC items that have been toggled by " +
		"the user (i.e. aren't in their default expansion state). - " +
		"Note: We intentionally track toggle state as opposed to expansion state " +
		"so that we only have to save a diff instead of all states for the whole " +
		"tree (which can be quite large) - and also due to backwards compatibility"),
	Array("Favorites", [
		Field("Name", String, None, "name of this favorite as shown in the menu"),
		Field("PageNo", Int, 0, "which page this favorite is about"),
		Field("PageLabel", String, None, "optional label for this page (if logical and physical page numbers are not the same)"),
		Field("MenuId", Int, 0, "for internal use", internal=True),
	], "Values which are persisted for bookmarks/favorites"),
	Field("Index", Type(None, "size_t"), "0",
		"temporary value needed for FileHistory::cmpOpenCount",
		internal=True),
	Field("Thumbnail", Type(None, "RenderedBitmap *"), "NULL",
		"the thumbnail is persisted separately as a PNG in sumatrapdfcache directory",
		internal=True),
	Field("DisplayModeEnum", Type(None, "DisplayMode"), "DM_AUTOMATIC",
		"for internal use",
		internal=True),
]

UserPrefs = Struct("UserPrefs", UserPrefs,
	"All values in this structure are can't be modified from within the UI and " +
	"are for experimental or expert settings. They're overwritten by the content " +
	"of SumatraPDF-user.ini if that file exists.")

GlobalPrefs = [
	# kjk: those settings should be foleded into GlobalPrefs. From the point of
	# view of people who are not us, it's not understandable why a given setting
	# is here and not there
	# zeniko: having them separate has the advantages of indicating which
	# settings are not exposed in the UI (both in the settings file and in the code)
	# and makes sure that all such options are right at the top of the file where
	# users can see them without having to go looking for them
	UserPrefs,

	# kjk: not a good name
	# zeniko: proposal?
	Field("GlobalPrefsOnly", Bool, False,
		"whether not to store display settings for individual documents"),
	# kjk: we need an "auto" value, which means "auto-detect". We shouldn't serializee
	# auto-detected language
	Field("CurrLangCode", Utf8String, None, # TODO: "auto"
		"the code of the current UI language"),
	Field("ShowToolbar", Bool, True,
		"whether the toolbar should be visible by default in the main window"),
	Field("ShowFavorites", Bool, False,
		"whether the Favorites sidebar should be visible by default in the main window"),
	Field("PdfAssociateDontAskAgain", Bool, False,
		"if false, we won't ask the user if he wants Sumatra to handle PDF files"),
	Field("PdfAssociateShouldAssociate", Bool, False,
		"if pdfAssociateDontAskAgain is true, says whether we should silently associate or not"),
	Field("CheckForUpdates", Bool, True,
		"whether SumatraPDF should check once a day whether updates are available"),
	Field("RememberOpenedFiles", Bool, True,
		"if true, we remember which files we opened and their settings"),
	# kjk: probably should be removed as we'll provide a way to set colors explicitly
	# zeniko: no, this is a UI exposed option for people who prefer changing their
	# color scheme system wide
	Field("UseSysColors", Bool, False,
		"whether to display documents black-on-white or in system colors"),
	Field("InverseSearchCmdLine", String, None,
		"pattern used to launch the editor when doing inverse search"),
	Field("EnableTeXEnhancements", Bool, False,
		"whether to expose the SyncTeX enhancements to the user"),
	Field("VersionToSkip", String, None,
		"When we show 'new version available', user has an option to check 'skip this version'. " +
		"This remembers which version is to be skipped."),
	Struct("LastUpdateTime", FileTime,
		"the time SumatraPDF has last checked for updates (see: EnableAutoUpdate)",
		structName="FILETIME", compact=True),
	Field("DefaultDisplayMode", String, "automatic",
		"how pages should be laid out by default, needs to be synchronized with " +
		"DefaultDisplayMode after deserialization and before serialization"),
	Field("DefaultZoom", Float, -1,
		"the default zoom factor in % (negative values indicate virtual settings)"),
	Field("WindowState", Int, 1,
		"default state of new windows (same as the last closed)"),
	Struct("WindowPos", RectI,
		"default position (can be on any monitor)", structName="RectI", compact=True),
	# kjk: ShowToc?
	Field("TocVisible", Bool, True,
		"whether the table of contents (Bookmarks) sidebar should be shown by " +
		"default when its available for a document"),
	Field("SidebarDx", Int, 0,
		"if sidebar (favorites and/or bookmarks) is visible, this is "+
		"the width of the left sidebar panel containing them"),
	Field("TocDy", Int, 0,
		"if both favorites and bookmarks parts of sidebar are visible, this is " +
		"the height of bookmarks (table of contents) part"),
	Field("ShowStartPage", Bool, True,
		"whether to display Frequently Read documents or the About page in an empty window"),
	Field("OpenCountWeek", Int, 0,
		"week count since 2011-01-01 needed to \"age\" openCount values in file history"),
	# kjk: unless I'm missing something, this should be per-file setting and
	# we really need ui for easy toggling of this state
	# zeniko: sure, let's add a "Manga Mode" menu item in View which is only visible for comics
	Field("CbxR2L", Bool, False,
		"display CBX double pages from right to left"),
	# file history and favorites
	# kjk: FileHistory
	# zeniko: it's more than just history and naming it so might get people to remove
	# the entire thing, only realizing later on that their favorites are gone as well
	Array("FileStates", FileSettings,
		"Most values in this structure are remembered individually for every file and " +
		"are by default also persisted so that reading can be resumed"),
	# non-serialized fields
	Struct("LastPrefUpdate", FileTime,
		"modification time of the preferences file when it was last read",
		structName="FILETIME", compact=True, internal=True),
	Field("DefaultDisplayModeEnum", Type(None, "DisplayMode"), "DM_AUTOMATIC",
		"the value of DefaultDisplayMode for internal usage",
		internal=True),
	Field("UnknownSettings", Utf8String, None,
		"a list of settings which this version of SumatraPDF didn't know how to handle ",
		internal=True),
]

GlobalPrefs = Struct("GlobalPrefs", GlobalPrefs,
	"Most values on this structure can be updated through the UI and are persisted " +
	"in SumatraPDF.dat (previously in sumatrapdfprefs.dat)")

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
	lines = ["struct %s {" % struct.structName]
	if struct.comment:
		lines = FormatComment(struct.comment, "//") + lines
	for field in struct.default:
		lines += FormatComment(field.comment, "\t//")
		lines.append("\t%s %s;" % (field.type.ctype, field.cname))
		if type(field) in [Struct, Array] and field.name in [field.structName, field.structName + "s"] and field.name not in built:
			lines = [BuildStruct(field), ""] + lines
			built.append(field.name)
	lines.append("};")
	return "\n".join(lines)

def BuildMetaData(struct, built=[]):
	lines, data, names = [], [], []
	for field in struct.default:
		if field.internal:
			continue
		data.append(("offsetof(%s, %s)" % (struct.structName, field.cname), "Type_%s" % field.type.name, field.cdefault()))
		names.append(field.name)
		if type(field) in [Struct, Array] and field.structName not in built:
			lines += [BuildMetaData(field), ""]
			built.append(field.structName)
	lines.append("static const FieldInfo g%sFields[] = {" % struct.structName)
	lines += ["\t{ %s }," % line for line in FormatArrayLine(data, "%s, %s, %s")]
	lines.append("};")
	lines.append("static const StructInfo g%sInfo = { sizeof(%s), %d, g%sFields, \"%s\" };" % (struct.structName, struct.structName, len(names), struct.structName, "\\0".join(names)))
	return "\n".join(lines)

def AssembleDefaults(struct, comment=None):
	lines, more = [], []
	if comment:
		lines += FormatComment(comment, ";") + [""]
	for field in struct.default:
		if field.internal:
			continue
		if type(field) in [Struct, Array] and not field.type.name == "Compact":
			more.append("\n".join(FormatComment(field.comment, ";") + ["[%s]" % field.name, AssembleDefaults(field)]))
		else:
			lines += FormatComment(field.comment, ";") + [field.inidefault()]
	if more:
		lines += [""] + more
	return "\n".join(lines) + "\n"

def AssembleDefaultsSqt(struct, comment=None, indent=""):
	lines = []
	if comment:
		lines += FormatComment(comment, ";") + [""]
	for field in struct.default:
		if field.internal:
			continue
		if type(field) in [Struct, Array] and not field.type.name == "Compact":
			lines += FormatComment(field.comment, indent + "#") + ["%s%s [" % (indent, field.name), AssembleDefaultsSqt(field, None, indent + "\t"), "%s]" % indent, ""]
		else:
			lines += FormatComment(field.comment, indent + "#") + [indent + field.inidefault(commentChar="#")]
	return "\n".join(lines)

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

	# content = AssembleDefaultsSqt(UserPrefs,
	content = AssembleDefaults(UserPrefs,
		"You can use this file to modify settings not changeable through the UI " +
		"instead of modifying SumatraPDF.dat directly. Just copy this file alongside " +
		"SumatraPDF.dat and change the values below. They will overwrite the corresponding " +
		"settings in SumatraPDF.dat at every startup.")
	content = "# Caution: All settings below are still subject to change!\n\n" + content
	open("docs/SumatraPDF-user.ini", "wb").write(content.replace("\n", "\r\n").encode("utf-8-sig"))

if __name__ == "__main__":
	main()
