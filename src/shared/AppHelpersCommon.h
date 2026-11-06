/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// one-off helpers shared by orig and ng; see AppHelpersCommon.cpp

struct TabGroup;
struct MainWindow;
struct Annotation;
struct Shortcut;

void FreeTabGroup(TabGroup* group);
void SetTabGroupFiles(TabGroup* group, MainWindow* win);
void LinkTabGroup(MainWindow* win, TabGroup* group);
void SyncTabGroups();
void SyncTabGroup(MainWindow* win);
TempStr TabGroupInfoTemp(TabGroup* group);
Str ScrollbarModeDisplayName(int idx);
int FindMatchIndex(MainWindow* win, int page, int glyph);
int FieldFontPx(Annotation* widget, Rect rc);
int DecimalDigits(int n);
Shortcut* FindScreenshotShortcutEntry();
TempStr FavoritePromptTemp(Str pageLabel);
bool AnnotationHasText(Annotation* annot);
Color TabTextColorForBackground(Color text, Color tabBg);
Str SelectIfChildOf(Str cameFrom, Str dir);

enum class TabGroupDialogMode {
    Save,
    Open,
};

// --- shared by AppHelpersCommon.cpp and each app's AnnotFilterToolbar.cpp ---

bool IsListNavKey(int vkey);
void PostedRefreshAnnots(MainWindow* win);

// --- shared by AppHelpersCommon.cpp and each app's AnnotTextPopup.cpp ---

Color PopupBg();
Color PopupText();
Color PopupMutedText();
Color PopupRuleColor();

// --- shared by AppHelpersCommon.cpp and each app's FindBar.cpp ---

// implemented by each app
void ShowCompactBar(MainWindow* win);

// --- shared by AppHelpersCommon.cpp and each app's ImageEditHostSumatra.cpp ---

Str TranslateStr(Str s);

// --- shared by AppHelpersCommon.cpp and each app's ChangeThemeDialog.cpp ---

// implemented by each app
void ShowThemeDialog(MainWindow* win, bool documentColorsFollowThemeOnly);

// --- shared by AppHelpersCommon.cpp and each app's TabGroupsManage.cpp ---

// implemented by each app
void ShowTabGroupsDialog(MainWindow* win, TabGroupDialogMode mode);
