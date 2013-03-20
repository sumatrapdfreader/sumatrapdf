"""
This script generates a struct and enough metadata for reading
a variety of preference values from a user provided settings file.
See further below for the definition of all currently supported options.
"""

import os, util2

class Type(object):
	def __init__(self, name, ctype, vectype=None):
		self.name = name; self.ctype = ctype; self.vectype = vectype or "Vec<%s>" % ctype

Bool = Type("Bool", "bool")
Color = Type("Color", "COLORREF")
FileTime =  Type("FileTime", "FILETIME")
Float = Type("Float", "float")
Int = Type("Int", "int")
String = Type("String", "ScopedMem<WCHAR>", "WStrVec")
Utf8String = Type("Utf8String", "ScopedMem<char>", "StrVec")

Flag_None, Flag_OnlyNonDefault, Flag_NonGlobal = [0, 1, 2]

class Field(object):
	def __init__(self, name, type, default, comment, alias=None, internalName=None, flags=0):
		self.name = name; self.type = type; self.default = default; self.comment = comment
		self.alias = alias or name; self.internalName = internalName; self.flags = flags
		self.cname = name[0].lower() + name[1:] if name else None

	def cdefault(self):
		if self.type == Bool:
			return "%s(%s)" % (self.cname, "true" if self.default else "false")
		if self.type == Color:
			return "%s(%#06x)" % (self.cname, self.default)
		if self.type == Float:
			return "%s(%f)" % (self.cname, self.default)
		if self.type == Int:
			return "%s(%d)" % (self.cname, self.default)
		if self.type == String and self.default is not None:
			return "%s(str::Dup(L\"%s\"))" % (self.cname, self.default)
		if self.type == Utf8String and self.default is not None:
			return "%s(str::Dup(\"%s\"))" % (self.cname, self.default)
		if self.type.name == "Custom" and self.default is not None:
			return "%s(%s)" % (self.cname, self.default)
		return None

class Section(object):
	def __init__(self, name, fields, internal=False):
		self.name = name; self.fields = fields; self.internal = internal

class SectionArray(Section):
	def __init__(self, name, fields, internal=False):
		super(SectionArray, self).__init__(name, fields, internal)

# ##### setting definitions for SumatraPDF #####

AdvancedOptions = [
	Field("TraditionalEbookUI", Bool, False,
		"whether the UI used for PDF documents will be used for ebooks as well " +
		"(enables printing and searching, disables automatic reflow)"),
	Field("ReuseInstance", Bool, False,
		"whether opening a new document should happen in an already running SumatraPDF " +
		"instance so that there's only one process and documents aren't opend twice"),
	Field("MainWindowBackground", Color, 0xFFF200,
		"background color of the non-document windows, traditionally yellow"),
	Field("EscToExit", Bool, False,
		"whether the Esc key will exit SumatraPDF same as 'q'"),
	Field("TextColor", Color, 0x000000,
		"color value with which black (text) will be substituted"),
	Field("PageColor", Color, 0xFFFFFF,
		"color value with which white (background) will be substituted"),
]

PagePadding = [
	Field("OuterX", Int, 4, "size of the left/right margin between window and document"),
	Field("OuterY", Int, 2, "size of the top/bottom margin between window and document"),
	Field("InnerX", Int, 4, "size of the horizontal margin between two pages"),
	Field("InnerY", Int, 4, "size of the vertical margin between two pages"),
]

PrinterDefaults = [
	Field("PrintScale", String, None, "default value for scaling (shrink, fit, none or NULL)"),
	Field("PrintAsImage", Bool, False, "default value for the compatibility option"),
]

ForwardSearch = [
	Field("EnableTeXEnhancements", Bool, False,
		"whether the inverse search command line setting is visible in the Settings dialog"),
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

ExternalViewer = [
	Field("CommandLine", String, None,
		"command line with which to call the external viewer, may contain " +
		"%p for page numer and %1 for the file name"),
	Field("Name", String, None,
		"name of the external viewer to be shown in the menu (implied by CommandLine if missing)"),
	Field("Filter", String, None,
		"filter for which file types the menu item is to be shown (e.g. \"*.pdf;*.xps\"; \"*\" if missing)"),
]

IniSettings = [
	Section("AdvancedOptions", AdvancedOptions),
	Section("PrinterDefaults", PrinterDefaults),
	Section("PagePadding", PagePadding),
	Section("ForwardSearch", ForwardSearch),
	SectionArray("ExternalViewer", ExternalViewer),
]

LegacyPrefs = [
	Field("GlobalPrefsOnly", Bool, False,
		"whether not to store display settings for individual documents"),
	Field("CurrLangCode", Type("Custom", "const char *"), '"en"',
		"pointer to a static string that is part of LangDef, don't free", alias="UILanguage"),
	Field("ToolbarVisible", Bool, True,
		"whether the toolbar should be visible by default in the main window", alias="ShowToolbar"),
	Field("FavVisible", Bool, False,
		"whether the Favorites sidebar should be visible by default in the main window"),
	Field("PdfAssociateDontAskAgain", Bool, False,
		"if false, we won't ask the user if he wants Sumatra to handle PDF files"),
	Field("PdfAssociateShouldAssociate", Bool, False,
		"if pdfAssociateDontAskAgain is true, says whether we should silently associate or not"),
	Field("EnableAutoUpdate", Bool, True,
		"whether SumatraPDF should check once a day whether updates are available"),
	Field("RememberOpenedFiles", Bool, True,
		"if true, we remember which files we opened and their settings"),
	Field("BgColor", Color, (0x00F2FF - 0x80000000),
		"used for the Start page, About page and Properties dialog " +
		"(negative values indicate that the default color will be used)"),
	Field("EscToExit", Bool, False,
		"whether the Esc key will exit SumatraPDF same as 'q'"),
	Field("UseSysColors", Bool, False,
		"whether to display documents black-on-white or in system colors"),
	Field("InverseSearchCmdLine", String, None,
		"pattern used to launch the editor when doing inverse search", alias="InverseSearchCommandLine"),
	Field("EnableTeXEnhancements", Bool, False,
		"whether to expose the SyncTeX enhancements to the user", alias="ExposeInverseSearch"),
	Field("VersionToSkip", String, None,
		"When we show 'new version available', user has an option to check 'skip this version'. " +
		"This remembers which version is to be skipped. If NULL - don't skip"),
	Field("LastUpdateTime", FileTime, 0,
		"the time SumatraPDF has last checked for updates (cf. EnableAutoUpdate)", alias="LastUpdate"),
	Field("DefaultDisplayMode", Type("Custom", "DisplayMode"), "DM_AUTOMATIC",
		"how pages should be layed out by default", alias="Display Mode"),
	Field("DefaultZoom", Float, -1,
		"the default zoom factor in % (negative values indicate virtual settings)", alias="ZoomVirtual"),
	Field("WindowState", Int, 1,
		"default state of new SumatraPDF windows (same as the last closed)", alias="Window State"),
	Field(None, Int, 0, None, alias="Window X", internalName="windowPos.x"),
	Field(None, Int, 0, None, alias="Window Y", internalName="windowPos.y"),
	Field(None, Int, 0, None, alias="Window DX", internalName="windowPos.dx"),
	Field(None, Int, 0, None, alias="Window DY", internalName="windowPos.dy"),
	Field("TocVisible", Bool, True,
		"whether the table of contents (Bookmarks) sidebar should be shown by " +
		"default when its available for a document", alias="ShowToc"),
	Field("SidebarDx", Int, 0,
		"if sidebar (favorites and/or bookmarks) is visible, this is "+
		"the width of the left sidebar panel containing them", alias="Toc DX"),
	Field("TocDy", Int, 0,
		"if both favorites and bookmarks parts of sidebar are visible, this is " +
		"the height of bookmarks (table of contents) part", alias="Toc Dy"),
	Field("FwdSearchOffset", Int, 0,
		"if <=0 then use the standard (inline) highlighting style, otherwise use the " +
		"margin highlight (i.e. coloured block on the left side of the page)", alias="ForwardSearch_HighlightOffset"),
	Field("FwdSearchColor", Color, 0x6581FF,
		"highlight color of the forward-search for both the standard and margin style", alias="ForwardSearch_HighlightColor"),
	Field("FwdSearchWidth", Int, 15,
		"width of the coloured blocks for the margin style", alias="ForwardSearch_HighlightWidth"),
	Field("FwdSearchPermanent", Bool, False,
		"if false then highlights are hidden automatically after a short period of time, " +
		"if true then highlights remain until the next forward search", alias="ForwardSearch_HighlightPermanent"),
	Field("ShowStartPage", Bool, True,
		"whether to display Frequently Read documents or the About page in an empty window"),
	Field("OpenCountWeek", Int, 0,
		"week count since 2011-01-01 needed to \"age\" openCount values in file history"),
	Field("CbxR2L", Bool, False,
		"display CBX double pages from right to left", alias="CBX_Right2Left"),
]

InternalSettings = [
	Field("LastPrefUpdate", FileTime, 0,
		"modification time of the preferences file when it was last read"),
	Field("PrevSerialization", Utf8String, None,
		"serialization of what was loaded (needed to prevent discarding unknown options)"),
	Field("WindowPos", Type(None, "RectI"), None,
		"default position (can be on any monitor)"),
]

GlobalPrefs = [
	Section(None, LegacyPrefs), # section without header
	Section("InternalPrefs", InternalSettings, True),
]

FileSettings = [
	Field("FilePath", String, None,
		"absolute path to a document that's been loaded successfully", alias="File"),
	Field("OpenCount", Int, 0,
		"in order to prevent documents that haven't been opened for a while " +
		"but used to be opened very frequently constantly remain in top positions, " +
		"the openCount will be cut in half after every week, so that the " +
		"Frequently Read list hopefully better reflects the currently relevant documents"),
	Field("IsPinned", Bool, False,
		"a user can \"pin\" a preferred document to the Frequently Read list " +
		"so that the document isn't replaced by more frequently used ones", alias="Pinned",
		flags=Flag_OnlyNonDefault),
	Field("IsMissing", Bool, False,
		"if a document can no longer be found but we still remember valuable state, " +
		"it's classified as missing so that it can be hidden instead of removed", alias="Missing",
		flags=Flag_OnlyNonDefault),
	Field("UseGlobalValues", Bool, False,
		"whether global defaults should be used when reloading this file instead of " +
		"the values listed below",
		flags=Flag_OnlyNonDefault),
	Field("DisplayMode", Type("Custom", "DisplayMode"), "DM_AUTOMATIC",
		"how pages should be layed out for this document", alias="Display Mode",
		flags=Flag_NonGlobal),
	Field(None, Int, 0, None, alias="Scroll X2", internalName="scrollPos.x", flags=Flag_NonGlobal),
	Field(None, Int, 0, None, alias="Scroll Y2", internalName="scrollPos.y", flags=Flag_NonGlobal),
	Field("PageNo", Int, 1,
		"the scrollPos values are relative to the top-left corner of this page", alias="Page",
		flags=Flag_NonGlobal),
	Field("ReparseIdx", Int, 0,
		"for bookmarking ebook files: offset of the current page reparse point within html",
		flags=Flag_NonGlobal),
	Field("ZoomVirtual", Float, 100.0,
		"the current zoom factor in % (negative values indicate virtual settings)",
		flags=Flag_NonGlobal),
	Field("Rotation", Int, 0,
		"how far pages have been rotated as a multiple of 90 degrees",
		flags=Flag_NonGlobal),
	Field("WindowState", Int, 0,
		"default state of new SumatraPDF windows (same as the last closed)", alias="Window State",
		flags=Flag_NonGlobal),
	Field(None, Int, 0, None, alias="Window X", internalName="windowPos.x", flags=Flag_NonGlobal),
	Field(None, Int, 0, None, alias="Window Y", internalName="windowPos.y", flags=Flag_NonGlobal),
	Field(None, Int, 0, None, alias="Window DX", internalName="windowPos.dx", flags=Flag_NonGlobal),
	Field(None, Int, 0, None, alias="Window DY", internalName="windowPos.dy", flags=Flag_NonGlobal),
	Field("DecryptionKey", Utf8String, None,
		"hex encoded MD5 fingerprint of file content (32 chars) followed by " +
		"crypt key (64 chars) - only applies for PDF documents", alias="Decryption Key"),
	Field("TocVisible", Bool, True,
		"whether the table of contents (Bookmarks) sidebar is shown for this document", alias="ShowToc",
		flags=Flag_NonGlobal),
	Field("SidebarDx", Int, 0,
		"the width of the left sidebar panel containing the table of contents", alias="Toc DX",
		flags=Flag_NonGlobal),
	Field("TocState", Type("Custom", "ScopedPtr<Vec<int>>", "?"), None,
		"tocState is an array of ids for ToC items that have been toggled by " +
		"the user (i.e. aren't in their default expansion state). - " +
		"Note: We intentionally track toggle state as opposed to expansion state " +
		"so that we only have to save a diff instead of all states for the whole " +
		"tree (which can be quite large) - and also due to backwards compatibility", alias="TocToggles",
		flags=Flag_NonGlobal),
]

FileInternals = [
	Field("Index", Type("Custom", "size_t"), "0",
		"temporary value needed for FileHistory::cmpOpenCount"),
	Field("Thumbnail", Type("Custom", "ScopedPtr<RenderedBitmap>", "?"), None,
		"the thumbnail is persisted separately as a PNG in sumatrapdfcache"),
	Field("ScrollPos", Type(None, "PointI"), None,
		"how far this document has been scrolled"),
	Field("WindowPos", Type(None, "RectI"), None,
		"position of the window containing this document (can be on any monitor)"),
]

FileState = [
	Section(None, FileSettings), # section without header
	Section("FileInternals", FileInternals, True),
]

# ##### end of setting definitions for SumatraPDF #####

def FormatComment(comment):
	result, parts, line = [], comment.split(), "\t//"
	while parts:
		while parts and len(line + parts[0]) < 72:
			line += " " + parts.pop(0)
		result.append(line)
		line = "\t//"
	return result

def BuildStruct(sections, name):
	lines = ["class %s {" % name, "public:"]
	defaults, structs = [], []
	for section in sections:
		if type(section) == SectionArray:
			lines += ["", "\t/* ***** fields for array section %s ***** */" % section.name, ""]
			for field in section.fields:
				lines += FormatComment(field.comment)
				lines.append("\t%s vec%s;" % (field.type.vectype, field.name))
		else:
			lines += ["", "\t/* ***** fields for section %s ***** */" % (section.name or name), ""]
			for field in section.fields:
				if field.internalName:
					assert not field.name
					continue
				lines += FormatComment(field.comment)
				lines.append("\t%s %s;" % (field.type.ctype, field.cname))
				if field.cdefault():
					defaults.append(field.cdefault())
				elif field.type == FileTime:
					structs.append(field.cname)
	
	if defaults or structs:
		lines.append("")
		lines.append("\t%s()" % name)
		if defaults:
			lines[-1] += " : " + ",\n\t\t".join(", ".join(grp) for grp in util2.group(defaults, 3))
		lines[-1] += " {"
		for strct in structs:
			lines.append("\t\tZeroMemory(&%s, sizeof(%s));" % (strct, strct))
		lines.append("\t}")
	lines.append("};")
	return "\n".join(lines)

def BuildMetaData(sections, name, sort=False):
	lines = ["static SettingInfo g%sInfo[] = {" % name]
	for section in sections:
		if section.internal:
			lines.append("\t/* ***** skipping internal section %s ***** */" % section.name)
			continue
		fields = section.fields
		if sort:
			fields = sorted(fields, key=lambda field: field.alias)
		if type(section) == SectionArray:
			lines.append("\t/* ***** fields for array section %s ***** */" % section.name)
			lines.append("\t{ \"%s\", Type_SectionVec }," % section.name)
			for field in fields:
				lines.append("\t{ \"%s\", Type_%s, offsetof(%s, vec%s), %d }," % (field.alias, field.type.name, name, field.name, field.flags))
		else:
			lines.append("\t/* ***** fields for section %s ***** */" % (section.name or name))
			if section.name:
				lines.append("\t{ \"%s\", Type_Section }," % section.name)
			else:
				assert sections[0] == section
			for field in fields:
				lines.append("\t{ \"%s\", Type_%s, offsetof(%s, %s), %d }," % (field.alias, field.type.name, name, field.internalName or field.cname, field.flags))
	lines.append("};")
	return "\n".join(lines)

AppPrefs2_Header = """\
/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 (see COPYING) */

// This file is auto-generated by gen_appprefs2.py

#ifndef AppPrefs2_h
#define AppPrefs2_h

%(mainStructDef)s

%(fileStructDef)s

%(advStructDef)s

#ifdef INCLUDE_APPPREFS2_METADATA
enum SettingType {
	Type_Section, Type_SectionVec, Type_Custom,
	Type_Bool, Type_Color, Type_FileTime, Type_Float, Type_Int, Type_String, Type_Utf8String,
};

enum SettingFlag { Flag_None, Flag_NonGlobal, Flag_OnlyNonDefault };

struct SettingInfo {
	const char *name;
	SettingType type;
	size_t offset;
	int flags;
};

%(mainStructMetadata)s

%(fileStructMetadata)s

%(advStructMetadata)s
#endif

#endif
"""

def main():
	util2.chdir_top()
	
	mainStructDef = BuildStruct(GlobalPrefs, "SerializableGlobalPrefs")
	mainStructMetadata = BuildMetaData(GlobalPrefs, "SerializableGlobalPrefs", True)
	fileStructDef = BuildStruct(FileState, "DisplayState")
	fileStructMetadata = BuildMetaData(FileState, "DisplayState", True)
	advStructDef = BuildStruct(IniSettings, "AdvancedSettings")
	advStructMetadata = BuildMetaData(IniSettings, "AdvancedSettings")
	content = AppPrefs2_Header % locals()
	open("src/AppPrefs2.h", "wb").write(content.replace("\n", "\r\n").replace("\t", "    "))

if __name__ == "__main__":
	main()
