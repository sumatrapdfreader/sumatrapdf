/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: what the engines and the application model expect from the layers above
// them. Everything here is replaced by the real file as the port gets to it:
// the document model in step 5b, the gpui shell in step 6, the theme and the
// toolbar in step 12. Delete this file when it is empty.

#include "base/Base.h"
#include "base/SettingsUtil.h"
#include "base/GuessFileType.h"
#include "base/Archive.h"

#include "DocProperties.h"
#include "Settings.h"
#include "AppSettings.h"
#include "Commands.h"
#include "DisplayMode.h"
#include "Translations.h"
#include "EmbeddedResources.h"
#include "SumatraPDF.h"

// EngineMupdf asks the app how the user configured ebook rendering
struct EBookUI;
struct FileEBookUI;

EBookUI* GetEBookUI() {
    return nullptr;
}

FileEBookUI* GetFileEBookUI(Str) {
    return nullptr;
}

TempStr GetSumatraDataDirTemp() {
    return {};
}

bool IsStressTesting() {
    return false;
}

// SumatraPDF.cpp (step 6)
void InitializePolicies(bool) {}

bool HasPermission(Perm) {
    return true;
}

bool CanAccessDisk() {
    return true;
}

bool AnnotationsAreDisabled() {
    return false;
}

bool IsUIRtl() {
    return false;
}

bool SettingsUseTabs() {
    return true;
}

bool SettingsRememberOpenedFiles() {
    return true;
}

void SetCurrentLang(Str langCode) {
    trans::SetCurrentLangByCode(langCode);
}

void SetCurrentLanguageAndRefreshUI(Str langCode) {
    SetCurrentLang(langCode);
}

void UpdateDocumentColors() {}

void UpdateFixedPageScrollbarsVisibility() {}

void ApplySettingsToWindowsUi() {}

void ReloadSettingsUpdateWindows(bool) {}

void MaybeRedrawHomePage() {}

SeqStrings gScrollbarModeNames = "windows\0smart\0overlay\0hidden\0";
SeqStrings gToolbarModeNames = "show\0hide\0overlay\0";
SeqStrings gToolbarPositionNames = "top\0bottom\0";

// ng: bodies copied from orig SumatraPDF.cpp; the model asks for them, the
// window that owns the render cache and the scrollbars comes in step 6
RenderCache* gRenderCache;

int ScrollbarModeFromPrefs() {
    int idx = SeqStrIndexIS(gScrollbarModeNames, gSettings->scrollbars);
    if (idx < 0) {
        idx = kScrollbarWindows;
    }
    return idx;
}

bool ScrollbarsAreHidden() {
    return ScrollbarModeFromPrefs() == kScrollbarHidden;
}

bool ScrollbarsUseOverlay() {
    int mode = ScrollbarModeFromPrefs();
    return mode == kScrollbarSmart || mode == kScrollbarOverlay;
}

// Canvas.cpp (step 7)
bool ShowTransparencyGrid() {
    return false;
}

// Print.cpp / TextViewWnd.cpp (step 14 / step 11)
void GetPrintersInfo(str::Builder&) {}

void ShowTextInWindowDialog(Str, Str) {}
void ShowTextInWindow(MainWindow*, Str, Str) {}
void SetTextViewWindowText(Str) {}
bool IsTextViewWindowTitle(Str) {
    return false;
}
void CloseTextViewWindow() {}
bool IsTextViewWindowVisible() {
    return false;
}

// Menu.cpp (step 6)
int CmdIdFromVirtualZoom(float) {
    return CmdZoomCustom;
}

// Favorites.cpp is UI now (step 9a) and lives in the SumatraPDF target;
// CommandAvailability only asks whether there are any
bool HasFavorites() {
    return false;
}

// HomePage.cpp (step 9b); FileThumbnails.cpp is in the app lib now
void HomePageInvalidateLayoutCache() {}

struct FileState;
void HomePageThumbnailChanged(FileState*) {}

// Theme.cpp asks the UI to repaint; AppSettings owns the UI fonts (step 6/12)
void UpdateAfterThemeChange() {}

// gui/GpuiTheme.cpp: the console tools have no gpui app (step 12b)
void ThemeInstallInGpui() {}

bool IsMenuFontSizeDefault() {
    return true;
}

// Notifications.cpp (step 6)
void MaybeDelayedWarningNotification(Str msg) {
    logf("warning: %s\n", msg);
}

// gui/DocCanvas.cpp: RenderCache draws through the gpui canvas, which the
// console tools have no window for (step 7)
struct BitmapCacheEntry;
namespace gpui {
struct PaintCtx;
}

bool CanvasDrawTile(gpui::PaintCtx*, BitmapCacheEntry*, Rect, Rect) {
    return false;
}

void CanvasDrawTileOutline(gpui::PaintCtx*, Rect) {}

void CanvasFreeTileImage(BitmapCacheEntry*) {}

// ExternalViewers.cpp is in the SumatraPDF target (it needs WindowTab); the
// command policy asks it whether a viewer is there (step 10b)
struct WindowTab;
struct CustomCommand;

bool IsOpenWithKnownExternalViewerCmd(int) {
    return false;
}

bool IsOpenWithKnownExternalViewerCmd(CustomCommand*) {
    return false;
}

bool HasKnownExternalViewerForCmd(int) {
    return false;
}

bool CanViewWithKnownExternalViewer(WindowTab*, int) {
    return false;
}

bool PathMatchFilter(Str, Str) {
    return false;
}

// UpdateCheck.cpp (step 14a) is in the app target only
bool HasPendingPreReleaseUpdate() {
    return false;
}

// GlobalHotkeys.cpp / ExplorerQuickLook.cpp (step 14a) are app-target only
void ReRegisterGlobalHotkeys() {}

void ExplorerQuickLookApplyFromSettings() {}

// ReadAloud.cpp (step 14c) is app-target only; the settings apply the voice
bool ApplyReadAloudVoiceFromSettings() {
    return false;
}
