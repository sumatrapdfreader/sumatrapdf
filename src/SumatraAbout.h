/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void OnMenuAbout();

void DrawAboutPage(WindowInfo* win, HDC hdc);

const WCHAR* GetStaticLink(Vec<StaticLinkInfo>& linkInfo, int x, int y, StaticLinkInfo* info = nullptr);

#define SLINK_OPEN_FILE L"<File,Open>"
#define SLINK_LIST_SHOW L"<View,ShowList>"
#define SLINK_LIST_HIDE L"<View,HideList>"

void DrawStartPage(WindowInfo* win, HDC hdc, FileHistory& fileHistory, COLORREF textColor, COLORREF backgroundColor);
