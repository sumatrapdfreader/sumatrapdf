from gen_settings_types_v2 import DefineStruct, MakeStruct, U16, I32, U32, Color, Bool, StructPtr

# Settings for 2.3 version of Sumatra

paddingStruct = DefineStruct("PaddingSettings", [
	U16("top",     2),
	U16("bottom",  2),
	U16("left",    4),
	U16("right",   4),
	U16("spaceX",  4),
	U16("spaceY",  4),
])

forwardSearchStruct = DefineStruct("ForwardSearchSettings", [
	I32("highlightOffset",    0),
	I32("highlightWidth",     15),
	I32("highlightPermanent", 0),
	Color("highlightColor",   0x6581FF),
])

advancedSettingsStruct = DefineStruct("AdvancedSettings", [
	U32("version", 0x02030000), # 2.3
	Bool("traditionalEbookUI", False),
	Bool("escToExit", False),
	Color("logoColor", 0xFFF200),
	StructPtr("pagePadding", paddingStruct, MakeStruct(paddingStruct)),
	# TODO: fooPading is just for testing, remove
	StructPtr("fooPadding", paddingStruct, MakeStruct(paddingStruct)),
	StructPtr("foo2Padding", paddingStruct, None),
	StructPtr("forwardSearch", forwardSearchStruct, MakeStruct(forwardSearchStruct)),
])

advancedSettings = MakeStruct(advancedSettingsStruct)

