from gen_settings_types import DefineStruct, MakeStruct, Field

# Settings for 2.3 version of Sumatra

paddingStruct = DefineStruct("PaddingSettings", [
	Field("top", 	"u16", 2),
	Field("bottom", "u16", 2),
	Field("left", 	"u16", 4),
	Field("right", 	"u16", 4),
	Field("spaceX", "u16", 4),
	Field("spaceY", "u16", 4),
])

forwardSearchStruct = DefineStruct("ForwardSearchSettings", [
	Field("highlightOffset", "i32", 0),
	Field("highlightWidth", "i32", 15),
	Field("highlightPermanent", "i32", 0),
	Field("highlightColor", "color", 0x6581FF),
])

advancedSettingsStruct = DefineStruct("AdvancedSettings", [
	Field("version", "u32", 0x02030000), # 2.3
	Field("traditionalEbookUI", "bool", False),
	Field("escToExit", "bool", False),
	Field("logoColor", "color", 0xFFF200),
	Field("pagePadding", paddingStruct, MakeStruct(paddingStruct)),
	# TODO: fooPading is just for testing, remove
	Field("fooPadding", paddingStruct, MakeStruct(paddingStruct)),
	Field("foo2Padding", paddingStruct, None),
	Field("forwardSearch", forwardSearchStruct, MakeStruct(forwardSearchStruct)),
])

advancedSettings = MakeStruct(advancedSettingsStruct)
