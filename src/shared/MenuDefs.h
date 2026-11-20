/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Menu definitions: tables that orig and ng share live in MenuDefs.cpp, the
// ones that differ in each app's Menu.cpp. Each ends with an empty entry.

// id of a separator that has to be addressable
constexpr uint kMenuSeparatorID = (uint)-13;

extern MenuDef menuDefFileOpen[];
extern MenuDef menuDefFile[];
extern MenuDef menuDefView[];
extern MenuDef menuDefGoTo[];
extern MenuDef menuDefZoom[];
extern MenuDef menuDefThemes[];
extern MenuDef menuDefSettings[];
extern MenuDef menuDefTabGroups[];
extern MenuDef menuDefFavorites[];
extern MenuDef menuDefHelp[];
extern MenuDef menuDefDebug[];
extern MenuDef menuDefGoogleLens[];
extern MenuDef menuDefTranslateWith[];
extern MenuDef menuDefSearchWith[];
extern MenuDef menuDefMainSelection[];
extern MenuDef menuDefReadAloud[];
extern MenuDef menuDefContextReadAloud[];
extern MenuDef menuDefMenubar[];
extern MenuDef menuDefCreateAnnotFromSelection[];
extern MenuDef menuDefContextAnnotations[];
extern MenuDef menuDefContextImage[];
extern MenuDef menuDefDocumentAIChat[];
extern MenuDef menuDefDocumentOperations[];
extern MenuDef menuDefContextStart[];
extern MenuDef menuDefContextTab[];
extern MenuDef menuDefStripGroup[];
extern MenuDef menuDefContextToc[];
extern MenuDef menuDefContextFav[];
extern MenuDef menuDefCreateAnnotUnderCursor[];
