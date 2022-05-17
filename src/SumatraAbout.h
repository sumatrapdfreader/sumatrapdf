/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* styling for About/Properties windows */

struct WindowInfo;

constexpr const WCHAR* kLeftTextFont = L"Arial";
constexpr int kLeftTextFontSize = 12;
constexpr const WCHAR* kRightTextFont = L"Arial Black";
constexpr int kRightTextFontSize = 12;

void OnMenuAbout(WindowInfo*);

void DrawAboutPage(WindowInfo* win, HDC hdc);

char* GetStaticLinkTemp(Vec<StaticLinkInfo*>& linkInfo, int x, int y, StaticLinkInfo** info);

constexpr const char* kLinkOpenFile = "<File,Open>";
constexpr const char* kLinkShowList = "<View,ShowList>";
constexpr const char* kLinkHideList = "<View,HideList>";

void DrawStartPage(WindowInfo* win, HDC hdc, FileHistory& fileHistory, COLORREF textColor, COLORREF backgroundColor);
