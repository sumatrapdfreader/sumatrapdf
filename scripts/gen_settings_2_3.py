from gen_settings_types import Struct, Field

# Settings for 2.3 version of Sumatra

padding = Struct("PaddingSettings", [
	Field("top", 	"u16", 2),
	Field("bottom", "u16", 2),
	Field("left", 	"u16", 4),
	Field("right", 	"u16", 4),
	Field("spaceX", "u16", 4),
	Field("spaceY", "u16", 4),
])

forwardSearch = Struct("ForwardSearchSettings", [
	Field("highlightOffset", "i32", 0),
	Field("highlightWidth", "i32", 15),
	Field("highlightPermanent", "i32", 0),
	Field("highlightColor", "color", 0x6581FF),
])

advancedSettings = Struct("AdvancedSettings", [
	Field("version", "u32", 0x02030000), # 2.3
	Field("traditionalEbookUI", "bool", False),
	Field("escToExit", "bool", False),
	Field("logoColor", "color", 0xFFF200),
	Field("pagePadding", padding, None),
	Field("forwardSearch", forwardSearch, None),
])
