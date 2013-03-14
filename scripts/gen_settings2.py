#!/usr/bin/env python

from gen_settings_types2 import Bool, U16, I32, U64, Float, Color
from gen_settings_types2 import String, WString, Struct

# TODO: fold default value into U16 etc. ?
class PaddingSettings(Struct):
    fields =[
        ("top", U16(2)),
        ("bottom", U16(2)),
        ("left", U16(4)),
        ("right", U16(4)),
        ("spaceX", U16(4)),
        ("spaceY", U16(4))
    ]

class ForwardSearch(Struct):
    fields = [
        ("highlightOffset", I32(0)),
        ("highlightWidth", I32(15)),
        ("highlightPermanent", I32(0)),
        ("highlightColor", Color(0x6581FF)),
        ("enableTeXEnhancements", Bool(False)),
]

class RectInt(Struct):
    fields = [
        ("x", I32(0)),
        ("y", I32(0)),
        ("dx", I32(0)),
        ("dy", I32(0)),
    ]

class BasicSettings(Struct):
    fields = [
        ("globalPrefsOnly", Bool(False)),
        ("currLanguage", String(None)), # auto-detect
        ("toolbarVisible", Bool(True)),
        ("pdfAssociateDontAsk", Bool(False)),
        ("pdfAssociateDoIt", Bool(False)),
        ("checkForUpdates", Bool(True)),
        ("rememberMRUFiles", Bool(True)),
        # TODO: useSystemColorScheme obsolete by textColor/pageColor ?
        ("useSystemColorScheme", Bool(False)),
        ("inverseSearchCmdLine", String(None)),
        ("versionToSkip", String(None)),
        ("lastUpdateTime", String(None)),
        ("defaultDisplayMode", U16(0)),  # DM_AUTOMATIC
        # -1 == Fit Page
        ("defaultZoom", Float(-1)),
        ("windowState", I32(1)), # WIN_STATE_NORMAL
        ("windowPos", RectInt()),
        ("tocVisible", Bool(True)),
        ("favVisible", Bool(False)),
        ("sidebarDx", I32(0)),
        ("tocDy", I32(0)),
        ("showStartPage", Bool(True)),
        ("openCountWeek", I32(0)),
        ("lastPrefUpdate", U64(0)),
    ]

class AdvancedSettings(Struct):
    fields = [
        ("traditionalEbookUI", Bool(True)),
        ("escToExit", Bool(False)),
        # TODO: different for different document types? For example, ebook
        # really needs one just for itself
        ("textColor", Color(0x0)),      # black
        ("pageColor", Color(0xffffff)), # white
        ("mainWindowBackground", Color(0xFFF200)),
        ("pagePadding", PaddingSettings()),
        ("forwardSearch", ForwardSearch()),

        # TODO: just for testing
        ("s", String("Hello")),
        ("defaultZoom", Float(-1)),
        ("ws", WString("A wide string")),
    ]

# TODO: merge basic/advanced into one?
class Settings(Struct):
    fields = [
        ("basic", BasicSettings()),
        ("advanced", AdvancedSettings()),
        # TODO: just for testing
        #Array("intArray", I32, [I32("", 1), I32("", 3)]),
    ]

settings = Settings()
version = "2.3"

def main():
    print("Hello")

if __name__ == "__main__":
    main()
