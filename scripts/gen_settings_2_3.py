from gen_settings_types import DefineStruct, MakeStruct
from gen_settings_types import Bool, U16, I32, U64, Float, Color, String, StructPtr

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
	Bool("enableTeXEnhancements", False),
])

rectStruct = DefineStruct("RectInt", None, [
	I32("x", 0),
	I32("y", 0),
	I32("dx", 0),
	I32("dy", 0)
])

basicSettingsStruct = DefineStruct("BasicSettings", None, [
	Bool("globalPrefsOnly", False),
	String("currLanguage", None), # auto-detect
	Bool("toolbarVisible", True),
	Bool("pdfAssociateDontAsk", False),
	Bool("pdfAssociateDoIt", False),
	Bool("checkForUpdates", True),
	Bool("rememberMRUFiles", True),
	# TODO: useSystemColorScheme obsolete by textColor/pageColor ?
	Bool("useSystemColorScheme", False),
	String("inverseSearchCmdLine", None),
	String("versionToSkip", None),
	String("lastUpdateTime", None),
	U16("defaultDisplayMode", 0),  # DM_AUTOMATIC
	# -1 == Fit Page
	Float("defaultZoom", -1),
	I32("windowState", 1), # WIN_STATE_NORMAL
	StructPtr("windowPos", rectStruct, MakeStruct(rectStruct)),
	Bool("tocVisible", True),
	Bool("favVisible", False),
	I32("sidebarDx", 0),
	I32("tocDy", 0),
	Bool("showStartPage", True),
	I32("openCountWeek", 0),
	U64("lastPrefUpdate", 0),
])

advancedSettingsStruct = DefineStruct("AdvancedSettings", None, [
	Bool("traditionalEbookUI", True),
	Bool("escToExit", False),
	# TODO: different for different document types? For example, ebook
	# really needs one just for itself
	Color("textColor", 0x0),      # black
	Color("pageColor", 0xffffff), # white
	Color("mainWindowBackground", 0xFFF200),
	StructPtr("pagePadding", paddingStruct, MakeStruct(paddingStruct)),
	StructPtr("forwardSearch", forwardSearchStruct, MakeStruct(forwardSearchStruct)),

	# TODO: just for testing
	String("s", "Hello"),
	Float("defaultZoom", -1),
])

# TODO: merge basic/advanced into one?
settingsStruct = DefineStruct("Settings", None, [
	StructPtr("basic", basicSettingsStruct, MakeStruct(basicSettingsStruct)),
	StructPtr("advanced", advancedSettingsStruct, MakeStruct(advancedSettingsStruct))
])

# TODO: should be settingsStruct, but MakeVal complains
settings = MakeStruct(advancedSettingsStruct)
version = "2.3"
