/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#define INCLUDE_SETTINGSSTRUCTS_METADATA
#include "AppPrefs.h"
#include "DisplayState.h"

#include "AppTools.h"
#include "BencUtil.h"
#include "DebugLog.h"
#include "Favorites.h"
#include "FileHistory.h"
#include "FileTransactions.h"
#include "FileUtil.h"
#include "FileWatcher.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "WindowInfo.h"

#define PREFS_INFO_URL          "http://blog.kowalczyk.info/software/sumatrapdf/settings.html"
#define OLD_PREFS_FILE_NAME     L"sumatrapdfprefs.dat"

GlobalPrefs *        gGlobalPrefs = NULL;

static WatchedFile * gWatchedSettingsFile = NULL;

Favorite *NewFavorite(int pageNo, const WCHAR *name, const WCHAR *pageLabel)
{
    Favorite *fav = (Favorite *)DeserializeStruct(&gFavoriteInfo, NULL);
    fav->pageNo = pageNo;
    fav->name = str::Dup(name);
    fav->pageLabel = str::Dup(pageLabel);
    return fav;
}

void DeleteFavorite(Favorite *fav)
{
    FreeStruct(&gFavoriteInfo, fav);
}

DisplayState *NewDisplayState(const WCHAR *filePath)
{
    DisplayState *ds = (DisplayState *)DeserializeStruct(&gFileStateInfo, NULL);
    str::ReplacePtr(&ds->filePath, filePath);
    return ds;
}

void DeleteDisplayState(DisplayState *ds)
{
    delete ds->thumbnail;
    FreeStruct(&gFileStateInfo, ds);
}

void DeleteGlobalPrefs(GlobalPrefs *globalPrefs)
{
    if (!globalPrefs)
        return;
    for (size_t i = 0; i < globalPrefs->fileStates->Count(); i++) {
        delete globalPrefs->fileStates->At(i)->thumbnail;
    }
    FreeStruct(&gGlobalPrefsInfo, globalPrefs);
}

// metadata mapping from legacy Benc names to current structures
static FieldInfo gGlobalPrefsFieldsBenc[] = {
    { offsetof(GlobalPrefs, cbxR2L), Type_Bool, false },
    { offsetof(GlobalPrefs, defaultDisplayMode), Type_String, (intptr_t)L"automatic" },
    { offsetof(GlobalPrefs, checkForUpdates), Type_Bool, true },
    { offsetof(GlobalPrefs, enableTeXEnhancements), Type_Bool, false },
    { offsetof(GlobalPrefs, showFavorites), Type_Bool, false },
    { offsetof(GlobalPrefs, rememberStatePerDocument), Type_Bool, false }, // note: used to be globalPrefsOnly
    { offsetof(GlobalPrefs, inverseSearchCmdLine), Type_String, NULL },
    { offsetof(GlobalPrefs, timeOfLastUpdateCheck), Type_Compact, 0 },
    { offsetof(GlobalPrefs, openCountWeek), Type_Int, 0 },
    { offsetof(GlobalPrefs, associateSilently), Type_Bool, false },
    { offsetof(GlobalPrefs, associatedExtensions), Type_Compact, false },
    { offsetof(GlobalPrefs, rememberOpenedFiles), Type_Bool, true },
    { offsetof(GlobalPrefs, showStartPage), Type_Bool, true },
    { offsetof(GlobalPrefs, showToc), Type_Bool, true },
    { offsetof(GlobalPrefs, showToolbar), Type_Bool, true },
    { offsetof(GlobalPrefs, sidebarDx), Type_Int, 0 },
    { offsetof(GlobalPrefs, tocDy), Type_Int, 0 },
    { offsetof(GlobalPrefs, uiLanguage), Type_Utf8String, NULL },
    { offsetof(GlobalPrefs, useSysColors), Type_Bool, false },
    { offsetof(GlobalPrefs, versionToSkip), Type_String, NULL },
    { offsetof(GlobalPrefs, windowPos.dx), Type_Int, 0 },
    { offsetof(GlobalPrefs, windowPos.dy), Type_Int, 0 },
    { offsetof(GlobalPrefs, windowState), Type_Int, 1 },
    { offsetof(GlobalPrefs, windowPos.x), Type_Int, 0 },
    { offsetof(GlobalPrefs, windowPos.y), Type_Int, 0 },
    { offsetof(GlobalPrefs, defaultZoom), Type_Utf8String, (intptr_t)"fit page" },
    { offsetof(GlobalPrefs, mainWindowBackground), Type_Color, 0xfff200 },
    { offsetof(GlobalPrefs, escToExit), Type_Bool, false },
    { offsetof(GlobalPrefs, forwardSearch.highlightColor), Type_Color, 0x6581ff },
    { offsetof(GlobalPrefs, forwardSearch.highlightOffset), Type_Int, 0 },
    { offsetof(GlobalPrefs, forwardSearch.highlightPermanent), Type_Bool, false },
    { offsetof(GlobalPrefs, forwardSearch.highlightWidth), Type_Int, 15 },
};
static StructInfo gGlobalPrefsInfoBenc = { sizeof(GlobalPrefs), 26, gGlobalPrefsFieldsBenc, "CBX_Right2Left\0Display Mode\0EnableAutoUpdate\0ExposeInverseSearch\0FavVisible\0GlobalPrefsOnly\0InverseSearchCommandLine\0LastUpdate\0OpenCountWeek\0PdfAssociateDontAskAgain\0PdfAssociateShouldAssociate\0RememberOpenedFiles\0ShowStartPage\0ShowToc\0ShowToolbar\0Toc DX\0Toc Dy\0UILanguage\0UseSysColors\0VersionToSkip\0Window DX\0Window DY\0Window State\0Window X\0Window Y\0ZoomVirtual\0BgColor\0EscToExit\0ForwardSearch_HighlightColor\0ForwardSearch_HighlightOffset\0ForwardSearch_HighlightPermanent\0ForwardSearch_HighlightWidth" };

static FieldInfo gFileFieldsBenc[] = {
    { offsetof(FileState, decryptionKey), Type_Utf8String, NULL },
    { offsetof(FileState, displayMode), Type_String, (intptr_t)L"automatic" },
    { offsetof(FileState, filePath), Type_String, NULL },
    { offsetof(FileState, isMissing), Type_Bool, false },
    { offsetof(FileState, openCount), Type_Int, 0 },
    { offsetof(FileState, pageNo), Type_Int, 1 },
    { offsetof(FileState, isPinned), Type_Bool, false },
    { offsetof(FileState, reparseIdx), Type_Int, 0 },
    { offsetof(FileState, rotation), Type_Int, 0 },
    { offsetof(FileState, scrollPos.x), Type_Int, 0 },
    { offsetof(FileState, scrollPos.y), Type_Int, 0 },
    { offsetof(FileState, showToc), Type_Bool, true },
    { offsetof(FileState, sidebarDx), Type_Int, 0 },
    { offsetof(FileState, tocState), Type_IntArray, NULL },
    { offsetof(FileState, useDefaultState), Type_Bool, false },
    { offsetof(FileState, windowPos.dx), Type_Int, 0 },
    { offsetof(FileState, windowPos.dy), Type_Int, 0 },
    { offsetof(FileState, windowState), Type_Int, 1 },
    { offsetof(FileState, windowPos.x), Type_Int, 0 },
    { offsetof(FileState, windowPos.y), Type_Int, 0 },
    { offsetof(FileState, zoom), Type_Utf8String, (intptr_t)"fit page" },
};
static StructInfo gFileInfoBenc = { sizeof(FileState), 21, gFileFieldsBenc, "Decryption Key\0Display Mode\0File\0Missing\0OpenCount\0Page\0Pinned\0ReparseIdx\0Rotation\0Scroll X2\0Scroll Y2\0ShowToc\0Toc DX\0TocToggles\0UseGlobalValues\0Window DX\0Window DY\0Window State\0Window X\0Window Y\0ZoomVirtual" };

static FieldInfo gBencGlobalPrefsFields[] = {
    { offsetof(GlobalPrefs, fileStates), Type_Array, (intptr_t)&gFileInfoBenc },
    { 0 /* self */, Type_Struct, (intptr_t)&gGlobalPrefsInfoBenc },
    // Favorites must be read after File History
    { offsetof(GlobalPrefs, fileStates), Type_Compact, NULL },
};
static StructInfo gBencGlobalPrefs = { sizeof(GlobalPrefs), 3, gBencGlobalPrefsFields, "File History\0gp\0Favorites" };

static bool BencGlobalPrefsCallback(BencDict *dict, const FieldInfo *field, const char *fieldName, uint8_t *fieldPtr)
{
    if (str::Eq(fieldName, "LastUpdate")) {
        BencString *val = dict ? dict->GetString(fieldName) : NULL;
        if (!val || !_HexToMem(val->RawValue(), (FILETIME *)fieldPtr))
            ZeroMemory(fieldPtr, sizeof(FILETIME));
        return true;
    }
    if (str::Eq(fieldName, "PdfAssociateShouldAssociate")) {
        BencInt *val = dict ? dict->GetInt(fieldName) : NULL;
        free(*(WCHAR **)fieldPtr);
        *(WCHAR **)fieldPtr = str::Dup(val && val->Value() ? L".pdf" : NULL);
        return true;
    }
    if (str::Eq(fieldName, "Favorites")) {
        BencArray *favDict = dict ? dict->GetArray(fieldName) : NULL;
        Vec<FileState *> *files = *(Vec<FileState *> **)fieldPtr;
        for (size_t j = 0; j < files->Count(); j++) {
            FileState *file = files->At(j);
            CrashIf(file->favorites);
            file->favorites = new Vec<Favorite *>();
            if (!favDict)
                continue;
            BencArray *favList = NULL;
            for (size_t k = 0; k < favDict->Length() && !favList; k += 2) {
                BencString *path = favDict->GetString(k);
                ScopedMem<WCHAR> filePath(path ? path->Value() : NULL);
                if (str::Eq(filePath, file->filePath))
                    favList = favDict->GetArray(k + 1);
            }
            if (!favList)
                continue;
            for (size_t k = 0; k < favList->Length(); k += 2) {
                BencInt *page = favList->GetInt(k);
                BencString *name = favList->GetString(k + 1);
                int pageNo = page ? (int)page->Value() : -1;
                ScopedMem<WCHAR> favName(name ? name->Value() : NULL);
                if (favName && pageNo > 0)
                    file->favorites->Append(NewFavorite(pageNo, favName, NULL));
            }
        }
        return true;
    }
    return false;
}

// number of weeks past since 2011-01-01
static int GetWeekCount()
{
    SYSTEMTIME date20110101 = { 0 };
    date20110101.wYear = 2011; date20110101.wMonth = 1; date20110101.wDay = 1;
    FILETIME origTime, currTime;
    SystemTimeToFileTime(&date20110101, &origTime);
    GetSystemTimeAsFileTime(&currTime);
    return (currTime.dwHighDateTime - origTime.dwHighDateTime) / 1408;
    // 1408 == (10 * 1000 * 1000 * 60 * 60 * 24 * 7) / (1 << 32)
}

static int cmpFloat(const void *a, const void *b)
{
    return *(float *)a < *(float *)b ? -1 : *(float *)a > *(float *)b ? 1 : 0;
}

namespace prefs {

/* Caller needs to DeleteGlobalPrefs(gGlobalPrefs) */
bool Load()
{
    CrashIf(gGlobalPrefs);

    ScopedMem<WCHAR> path(AppGenDataFilename(PREFS_FILE_NAME));
    ScopedMem<char> prefsData(file::ReadAll(path, NULL));
    gGlobalPrefs = (GlobalPrefs *)DeserializeStruct(&gGlobalPrefsInfo, prefsData);
    CrashAlwaysIf(!gGlobalPrefs);

#ifdef DISABLE_EBOOK_UI
    if (!file::Exists(path)) {
        gGlobalPrefs->ebookUI.useFixedPageUI = true;
        gGlobalPrefs->chmUI.useFixedPageUI = true;
    }
#endif

    if (!file::Exists(path)) {
        ScopedMem<WCHAR> bencPath(AppGenDataFilename(OLD_PREFS_FILE_NAME));
        ScopedMem<char> bencPrefsData(file::ReadAll(bencPath, NULL));
        // the old format used the inverted meaning for this pref
        gGlobalPrefs->rememberStatePerDocument = !gGlobalPrefs->rememberStatePerDocument;
        DeserializeStructBenc(&gBencGlobalPrefs, bencPrefsData, gGlobalPrefs, BencGlobalPrefsCallback);
        gGlobalPrefs->rememberStatePerDocument = !gGlobalPrefs->rememberStatePerDocument;
        // update the zoom values to the more readable format
        float zoom = conv::ToZoom(gGlobalPrefs->defaultZoom);
        str::ReplacePtr(&gGlobalPrefs->defaultZoom, NULL);
        conv::FromZoom(&gGlobalPrefs->defaultZoom, zoom);
        for (DisplayState **ds = gGlobalPrefs->fileStates->IterStart(); ds; ds = gGlobalPrefs->fileStates->IterNext()) {
            zoom = conv::ToZoom((*ds)->zoom);
            str::ReplacePtr(&(*ds)->zoom, NULL);
            conv::FromZoom(&(*ds)->zoom, zoom);
        }
    }

#ifdef ENABLE_SUMATRAPDF_USER_INI
    ScopedMem<WCHAR> userPath(AppGenDataFilename(L"SumatraPDF-user.ini"));
    if (userPath && file::Exists(userPath)) {
        ScopedMem<char> userPrefsData(file::ReadAll(userPath, NULL));
        DeserializeStruct(&gGlobalPrefsInfo, userPrefsData, gGlobalPrefs);
    }
#endif

    if (!gGlobalPrefs->uiLanguage || !trans::ValidateLangCode(gGlobalPrefs->uiLanguage)) {
        // guess the ui language on first start
        gGlobalPrefs->uiLanguage = str::Dup(trans::DetectUserLang());
    }
    gGlobalPrefs->lastPrefUpdate = file::GetModificationTime(path);
    gGlobalPrefs->defaultDisplayModeEnum = conv::ToDisplayMode(gGlobalPrefs->defaultDisplayMode, DM_AUTOMATIC);
    gGlobalPrefs->defaultZoomFloat = conv::ToZoom(gGlobalPrefs->defaultZoom, ZOOM_ACTUAL_SIZE);
    CrashIf(!IsValidZoom(gGlobalPrefs->defaultZoomFloat));

    int weekDiff = GetWeekCount() - gGlobalPrefs->openCountWeek;
    gGlobalPrefs->openCountWeek = GetWeekCount();
    if (weekDiff > 0) {
        // "age" openCount statistics (cut in in half after every week)
        for (DisplayState **ds = gGlobalPrefs->fileStates->IterStart(); ds; ds = gGlobalPrefs->fileStates->IterNext()) {
            (*ds)->openCount >>= weekDiff;
        }
    }

    // make sure that zoom levels are in the order expected by DisplayModel
    gGlobalPrefs->zoomLevels->Sort(cmpFloat);
    while (gGlobalPrefs->zoomLevels->Count() > 0 &&
           gGlobalPrefs->zoomLevels->At(0) < ZOOM_MIN) {
        gGlobalPrefs->zoomLevels->RemoveAt(0);
    }
    while (gGlobalPrefs->zoomLevels->Count() > 0 &&
           gGlobalPrefs->zoomLevels->Last() > ZOOM_MAX) {
        gGlobalPrefs->zoomLevels->Pop();
    }

    gFileHistory.UpdateStatesSource(gGlobalPrefs->fileStates);

    // TODO: update gGlobalPrefs->unknownSettings

    return true;
}

// called whenever global preferences change or a file is
// added or removed from gFileHistory (in order to keep
// the list of recently opened documents in sync)
bool Save()
{
    // don't save preferences for plugin windows
    if (gPluginMode)
        return false;

    // don't save preferences without the proper permission
    if (!HasPermission(Perm_SavePreferences))
        return false;

    /* mark currently shown files as visible */
    for (size_t i = 0; i < gWindows.Count(); i++) {
        UpdateCurrentFileDisplayStateForWin(SumatraWindow::Make(gWindows.At(i)));
    }
    for (size_t i = 0; i < gEbookWindows.Count(); i++) {
        UpdateCurrentFileDisplayStateForWin(SumatraWindow::Make(gEbookWindows.At(i)));
    }

    // remove entries which should (no longer) be remembered
    gFileHistory.Purge(!gGlobalPrefs->rememberStatePerDocument);
    // update display mode and zoom fields from internal values
    str::ReplacePtr(&gGlobalPrefs->defaultDisplayMode, conv::FromDisplayMode(gGlobalPrefs->defaultDisplayModeEnum));
    conv::FromZoom(&gGlobalPrefs->defaultZoom, gGlobalPrefs->defaultZoomFloat);

    ScopedMem<WCHAR> path(AppGenDataFilename(PREFS_FILE_NAME));
    CrashIf(!path);
    if (!path)
        return false;
    size_t prevPrefsDataSize;
    ScopedMem<char> prevPrefsData(file::ReadAll(path, &prevPrefsDataSize));

    if (!gGlobalPrefs->rememberStatePerDocument) {
        for (DisplayState **ds = gGlobalPrefs->fileStates->IterStart(); ds; ds = gGlobalPrefs->fileStates->IterNext()) {
            (*ds)->useDefaultState = true;
        }
        // prevent unnecessary settings from being written out
        uint16_t fieldCount = 0;
        while (++fieldCount <= dimof(gFileStateFields)) {
            // count the number of fields up to and including useDefaultState
            if (gFileStateFields[fieldCount - 1].offset == offsetof(FileState, useDefaultState))
                break;
        }
        // restore the correct fieldCount ASAP after serialization
        gFileStateInfo.fieldCount = fieldCount;
    }

    size_t prefsDataSize;
    ScopedMem<char> prefsData(SerializeStruct(&gGlobalPrefsInfo, gGlobalPrefs, prevPrefsData,
                                              PREFS_INFO_URL, &prefsDataSize));

    if (!gGlobalPrefs->rememberStatePerDocument)
        gFileStateInfo.fieldCount = dimof(gFileStateFields);

    CrashIf(!prefsData || 0 == prefsDataSize);
    if (!prefsData || 0 == prefsDataSize)
        return false;

    // only save if anything's changed at all
    if (prevPrefsDataSize == prefsDataSize && str::Eq(prefsData, prevPrefsData))
        return true;

    FileTransaction trans;
    bool ok = trans.WriteAll(path, prefsData, prefsDataSize) && trans.Commit();
    if (!ok)
        return false;
    gGlobalPrefs->lastPrefUpdate = file::GetModificationTime(path);

    // notify all SumatraPDF instances about the updated prefs file
    HWND hwnd = NULL;
    while ((hwnd = FindWindowEx(HWND_DESKTOP, hwnd, FRAME_CLASS_NAME, NULL)) != NULL) {
        PostMessage(hwnd, UWM_PREFS_FILE_UPDATED, 0, 0);
    }
    return true;
}

// refresh the preferences when a different SumatraPDF process saves them
// TODO: should immediately update as much look&feel as possible. Settings
// that are not immediately reflected:
//  - MainWindowBackground when showing the start page
bool Reload()
{
    ScopedMem<WCHAR> path(AppGenDataFilename(PREFS_FILE_NAME));
    if (!path || !file::Exists(path))
        return false;

    FILETIME time = file::GetModificationTime(path);
    if (time.dwLowDateTime == gGlobalPrefs->lastPrefUpdate.dwLowDateTime &&
        time.dwHighDateTime == gGlobalPrefs->lastPrefUpdate.dwHighDateTime) {
        return true;
    }

    ScopedMem<char> uiLanguage(str::Dup(gGlobalPrefs->uiLanguage));
    bool showToolbar = gGlobalPrefs->showToolbar;
    bool useSysColors = gGlobalPrefs->useSysColors;

    gFileHistory.UpdateStatesSource(NULL);
    DeleteGlobalPrefs(gGlobalPrefs);
    gGlobalPrefs = NULL;

    bool ok = Load();
    if (!ok)
        return false;

    if (gWindows.Count() > 0 && gWindows.At(0)->IsAboutWindow()) {
        gWindows.At(0)->DeleteInfotip();
        gWindows.At(0)->RedrawAll(true);
    }

    if (!str::Eq(uiLanguage, gGlobalPrefs->uiLanguage)) {
        SetCurrentLanguageAndRefreshUi(gGlobalPrefs->uiLanguage);
    }

    if (gGlobalPrefs->showToolbar != showToolbar)
        ShowOrHideToolbarGlobally();
    if (gGlobalPrefs->useSysColors != useSysColors)
        UpdateDocumentColors();
    UpdateFavoritesTreeForAllWindows();

    return true;
}

class SettingsFileObserver : public FileChangeObserver {
public:
    virtual ~SettingsFileObserver() { }
    virtual void OnFileChanged();
};

void SettingsFileObserver::OnFileChanged()
{
    Reload();
}

void RegisterForFileChanges()
{
    CrashIf(gWatchedSettingsFile); // only call me once
    ScopedMem<WCHAR> path(AppGenDataFilename(PREFS_FILE_NAME));
    gWatchedSettingsFile = FileWatcherSubscribe(path, new SettingsFileObserver());
}

void UnregisterForFileChanges()
{
    FileWatcherUnsubscribe(gWatchedSettingsFile);
}

namespace conv {

#define DM_AUTOMATIC_STR            "automatic"
#define DM_SINGLE_PAGE_STR          "single page"
#define DM_FACING_STR               "facing"
#define DM_BOOK_VIEW_STR            "book view"
#define DM_CONTINUOUS_STR           "continuous"
#define DM_CONTINUOUS_FACING_STR    "continuous facing"
#define DM_CONTINUOUS_BOOK_VIEW_STR "continuous book view"

#define STR_FROM_ENUM(val) \
    if (val == mode) \
        return TEXT(val##_STR); \
    else NoOp()

const WCHAR *FromDisplayMode(DisplayMode mode)
{
    STR_FROM_ENUM(DM_AUTOMATIC);
    STR_FROM_ENUM(DM_SINGLE_PAGE);
    STR_FROM_ENUM(DM_FACING);
    STR_FROM_ENUM(DM_BOOK_VIEW);
    STR_FROM_ENUM(DM_CONTINUOUS);
    STR_FROM_ENUM(DM_CONTINUOUS_FACING);
    STR_FROM_ENUM(DM_CONTINUOUS_BOOK_VIEW);
    CrashIf(true);
    return L"unknown display mode!?";
}

#undef STR_FROM_ENUM

#define IS_STR_ENUM(enumName) \
    if (str::EqIS(s, TEXT(enumName##_STR))) \
        return enumName; \
    else NoOp()

DisplayMode ToDisplayMode(const WCHAR *s, DisplayMode default)
{
    IS_STR_ENUM(DM_AUTOMATIC);
    IS_STR_ENUM(DM_SINGLE_PAGE);
    IS_STR_ENUM(DM_FACING);
    IS_STR_ENUM(DM_BOOK_VIEW);
    IS_STR_ENUM(DM_CONTINUOUS);
    IS_STR_ENUM(DM_CONTINUOUS_FACING);
    IS_STR_ENUM(DM_CONTINUOUS_BOOK_VIEW);
    // for consistency ("continuous" is used instead in the settings instead for brevity)
    if (str::EqIS(s, L"continuous single page"))
        return DM_CONTINUOUS;
    return default;
}

#undef IS_STR_ENUM

void FromZoom(char **dst, float zoom, DisplayState *stateForIssue2140)
{
    float prevZoom = *dst ? ToZoom(*dst, INVALID_ZOOM) : INVALID_ZOOM;
    if (prevZoom == zoom)
        return;
    if (!IsValidZoom(zoom) && stateForIssue2140) {
        // TODO: does issue 2140 still occur?
        dbglog::CrashLogF("Invalid ds->zoom: %g", zoom);
        const WCHAR *ext = path::GetExt(stateForIssue2140->filePath);
        if (!str::IsEmpty(ext)) {
            ScopedMem<char> extA(str::conv::ToUtf8(ext));
            dbglog::CrashLogF("File type: %s", extA.Get());
        }
        dbglog::CrashLogF("DisplayMode: %S", stateForIssue2140->displayMode);
        dbglog::CrashLogF("PageNo: %d", stateForIssue2140->pageNo);
    }
    CrashIf(!IsValidZoom(zoom));
    free(*dst);
    if (ZOOM_FIT_PAGE == zoom)
        *dst = str::Dup("fit page");
    else if (ZOOM_FIT_WIDTH == zoom)
        *dst = str::Dup("fit width");
    else if (ZOOM_FIT_CONTENT == zoom)
        *dst = str::Dup("fit content");
    else
        *dst = str::Format("%g", zoom);
}

float ToZoom(const char *s, float default)
{
    if (str::EqIS(s, "fit page"))
        return ZOOM_FIT_PAGE;
    if (str::EqIS(s, "fit width"))
        return ZOOM_FIT_WIDTH;
    if (str::EqIS(s, "fit content"))
        return ZOOM_FIT_CONTENT;
    float zoom;
    if (str::Parse(s, "%f", &zoom) && IsValidZoom(zoom))
        return zoom;
    return default;
}

}; // namespace conv

}; // namespace prefs
