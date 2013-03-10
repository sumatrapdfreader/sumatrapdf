from gen_settings_types import DefineStruct, MakeStruct, U16, I32, Color, Bool, StructPtr, String

# Settings for 2.3 version of Sumatra

paddingStruct1 = DefineStruct("PaddingSettings1", None, [
	U16("top",     2),
	U16("bottom",  2),
	U16("left",    4),
	U16("right",   4),
])

# this is artificial, just to test inheritance
paddingStruct = DefineStruct("PaddingSettings", paddingStruct1, [
	U16("spaceX",  4),
	U16("spaceY",  4),
])

forwardSearchStruct = DefineStruct("ForwardSearchSettings", None, [
	I32("highlightOffset",    0),
	I32("highlightWidth",     15),
	I32("highlightPermanent", 0),
	Color("highlightColor",   0x6581FF),
])

advancedSettingsStruct = DefineStruct("AdvancedSettings", None, [
	Bool("traditionalEbookUI", False),
	Bool("escToExit", False),
	String("emptyString", None),
	Color("logoColor", 0xFFF200),
	StructPtr("pagePadding", paddingStruct, MakeStruct(paddingStruct)),
	# TODO: fooPading and foo2Padding is just for testing, remove after testing
	#StructPtr("fooPadding", paddingStruct, MakeStruct(paddingStruct)),
	StructPtr("foo2Padding", paddingStruct, None),
	String("notEmptyString", "Hello"),
	StructPtr("forwardSearch", forwardSearchStruct, MakeStruct(forwardSearchStruct)),
])

advancedSettings = MakeStruct(advancedSettingsStruct)
version = "2.3"