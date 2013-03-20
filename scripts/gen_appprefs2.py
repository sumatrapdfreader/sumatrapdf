"""
This script generates a struct and enough metadata for reading
a variety of preference values from a user provided settings file.
See further below for the definition of all currently supported options.
"""

import os, util2

Bool, Color, Int, String = "bool", "COLORREF", "int", "WCHAR *"

class Field(object):
	def __init__(self, name, type, default, comment):
		self.name = name; self.type = type; self.default = default; self.comment = comment

	def cname(self):
		return self.name[0].lower() + self.name[1:]

	def cdecl(self):
		if self.type == String:
			return "ScopedMem<WCHAR> %s;" % self.cname()
		return "%s %s;" % (self.type, self.cname())

	def cdecl2(self):
		if self.type == String:
			return "WStrVec vec%s;" % self.name
		return "Vec<%s> vec%s;" % (self.type, self.name)

	def cdefault(self):
		if self.type == Bool:
			return "%s(%s)" % (self.cname(), "true" if self.default else "false")
		if self.type == Int:
			return "%s(%d)" % (self.cname(), self.default)
		if self.type == Color:
			return "%s(0x%06X)" % (self.cname(), self.default)
		return None

	def stype(self):
		return "SType_%s" % ("Bool" if self.type == Bool else "Color" if self.type == Color else "Int" if self.type == Int else "String")

class Section(object):
	def __init__(self, name, fields):
		self.name = name; self.fields = fields

class SectionArray(object):
	def __init__(self, name, fields):
		self.name = name; self.fields = fields

# TODO: move these settings into a different file for convenience?

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
	defaults = []
	for section in sections:
		if type(section) == SectionArray:
			lines += ["", "\t/* ***** fields for array section %s ***** */" % section.name, ""]
			for field in section.fields:
				lines += FormatComment(field.comment)
				lines.append("\t" + field.cdecl2())
		else:
			lines += ["", "\t/* ***** fields for section %s ***** */" % section.name, ""]
			for field in section.fields:
				lines += FormatComment(field.comment)
				lines.append("\t" + field.cdecl())
				if field.cdefault():
					defaults.append(field.cdefault())
	if defaults:
		lines.append("")
		lines.append("\t%s() : %s { }" % (name, ", ".join(defaults)))
	lines.append("};")
	return "\n".join(lines)

def BuildMetaData(sections, name):
	lines = ["static SettingInfo g%sInfo[] = {" % name]
	for section in sections:
		if type(section) == SectionArray:
			lines.append("\t/* ***** fields for array section %s ***** */" % section.name)
			lines.append("\t{ \"%s\", SType_SectionVec }," % section.name)
			for field in section.fields:
				lines.append("\t{ \"%s\", %s, offsetof(%s, vec%s) }," % (field.name, field.stype(), name, field.name))
		else:
			lines.append("\t/* ***** fields for section %s ***** */" % section.name)
			lines.append("\t{ \"%s\", SType_Section }," % section.name)
			for field in section.fields:
				lines.append("\t{ \"%s\", %s, offsetof(%s, %s) }," % (field.name, field.stype(), name, field.cname()))
	lines.append("};")
	return "\n".join(lines)

AppPrefs2_Header = """\
/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 (see COPYING) */

// This file is auto-generated by gen_appprefs2.py

#ifndef AppPrefs2_h
#define AppPrefs2_h

%(structDef)s

#ifdef INCLUDE_APPPREFS2_METADATA
enum SettingType { SType_Section, SType_SectionVec, SType_Bool, SType_Color, SType_Int, SType_String };

struct SettingInfo {
	const char *name;
	SettingType type;
	size_t offset;
};

%(structMetadata)s
#endif

#endif
"""

def main():
	util2.chdir_top()
	
	structDef = BuildStruct(IniSettings, "AdvancedSettings")
	structMetadata = BuildMetaData(IniSettings, "AdvancedSettings")
	content = AppPrefs2_Header % locals()
	open("src/AppPrefs2.h", "wb").write(content.replace("\n", "\r\n").replace("\t", "    "))

if __name__ == "__main__":
	main()
