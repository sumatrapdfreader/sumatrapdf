from gen_settings_types import DefineStruct, MakeStruct, U16, I32, Color, Bool, StructPtr, Version

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
	Version("2.3"),
	Bool("traditionalEbookUI", False),
	Bool("escToExit", False),
	Color("logoColor", 0xFFF200),
	StructPtr("pagePadding", paddingStruct, MakeStruct(paddingStruct)),
	# TODO: fooPading is just for testing, remove
	StructPtr("fooPadding", paddingStruct, MakeStruct(paddingStruct)),
	StructPtr("foo2Padding", paddingStruct, None),
	StructPtr("forwardSearch", forwardSearchStruct, MakeStruct(forwardSearchStruct)),
])

# test that Version over-rides the parent's version
advancedSettingsStruct2 = DefineStruct("AdvancedSettings2", advancedSettingsStruct, [
	Version("2.4"),
	StructPtr("foo3Padding", paddingStruct, None),
])

advancedSettings = MakeStruct(advancedSettingsStruct2)