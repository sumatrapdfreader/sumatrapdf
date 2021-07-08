/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* styling for About/Properties windows */

constexpr const WCHAR* kLeftTextFont = L"Arial";
constexpr int kLeftTextFontSize = 12;
constexpr const WCHAR* kRightTextFont = L"Arial Black";
constexpr int kRightTextFontSize = 12;

void OnMenuAbout();

void DrawAboutPage(WindowInfo* win, HDC hdc);

const WCHAR* GetStaticLink(Vec<StaticLinkInfo>& linkInfo, int x, int y, StaticLinkInfo* info);

constexpr const WCHAR* kLinkOpenFile = L"<File,Open>";
constexpr const WCHAR* kLinkShowList = L"<View,ShowList>";
constexpr const WCHAR* kLinkHideList = L"<View,HideList>";

void DrawStartPage(WindowInfo* win, HDC hdc, FileHistory& fileHistory, COLORREF textColor, COLORREF backgroundColor);
