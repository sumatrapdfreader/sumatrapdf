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
#include "SumatraPDF.h"
#include "Translations.h"
#include "WindowInfo.h"

#define PREFS_INFO_URL          "http://blog.kowalczyk.info/software/sumatrapdf/settings.html"
// TODO: more appropriate file extension?
#define NEW_PREFS_FILE_NAME     L"SumatraPDF.dat"
#define USER_PREFS_FILE_NAME    L"SumatraPDF-user.ini"
#define OLD_PREFS_FILE_NAME     L"sumatrapdfprefs.dat"

GlobalPrefs *gGlobalPrefs = NULL;
UserPrefs *gUserPrefs = NULL;

Favorite *NewFavorite(int pageNo, const WCHAR *name, const WCHAR *pageLabel)
{
    Favorite *fav = AllocStruct<Favorite>();
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
    ds->filePath = str::Dup(filePath);
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
    { offsetof(GlobalPrefs, enableAutoUpdate), Type_Bool, true },
    { offsetof(GlobalPrefs, enableTeXEnhancements), Type_Bool, false },
    { offsetof(GlobalPrefs, favVisible), Type_Bool, false },
    { offsetof(GlobalPrefs, globalPrefsOnly), Type_Bool, false },
    { offsetof(GlobalPrefs, inverseSearchCmdLine), Type_String, NULL },
    { offsetof(GlobalPrefs, lastUpdateTime), Type_Compact, 0 },
    { offsetof(GlobalPrefs, openCountWeek), Type_Int, 0 },
    { offsetof(GlobalPrefs, pdfAssociateDontAskAgain), Type_Bool, false },
    { offsetof(GlobalPrefs, pdfAssociateShouldAssociate), Type_Bool, false },
    { offsetof(GlobalPrefs, rememberOpenedFiles), Type_Bool, true },
    { offsetof(GlobalPrefs, showStartPage), Type_Bool, true },
    { offsetof(GlobalPrefs, tocVisible), Type_Bool, true },
    { offsetof(GlobalPrefs, toolbarVisible), Type_Bool, true },
    { offsetof(GlobalPrefs, sidebarDx), Type_Int, 0 },
    { offsetof(GlobalPrefs, tocDy), Type_Int, 0 },
    { offsetof(GlobalPrefs, currLangCode), Type_Utf8String, NULL },
    { offsetof(GlobalPrefs, useSysColors), Type_Bool, false },
    { offsetof(GlobalPrefs, versionToSkip), Type_String, NULL },
    { offsetof(GlobalPrefs, windowPos.dx), Type_Int, 1 },
    { offsetof(GlobalPrefs, windowPos.dy), Type_Int, 1 },
    { offsetof(GlobalPrefs, windowState), Type_Int, 1 },
    { offsetof(GlobalPrefs, windowPos.x), Type_Int, 1 },
    { offsetof(GlobalPrefs, windowPos.y), Type_Int, 1 },
    { offsetof(GlobalPrefs, defaultZoom), Type_Float, (intptr_t)"-1" },
    { offsetof(GlobalPrefs, userPrefs.mainWindowBackground), Type_Color, 0xfff200 },
    { offsetof(GlobalPrefs, userPrefs.escToExit), Type_Bool, false },
    { offsetof(GlobalPrefs, userPrefs.forwardSearch.highlightColor), Type_Color, 0x6581ff },
    { offsetof(GlobalPrefs, userPrefs.forwardSearch.highlightOffset), Type_Int, 0 },
    { offsetof(GlobalPrefs, userPrefs.forwardSearch.highlightPermanent), Type_Bool, false },
    { offsetof(GlobalPrefs, userPrefs.forwardSearch.highlightWidth), Type_Int, 15 },
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
    { offsetof(FileState, tocVisible), Type_Bool, true },
    { offsetof(FileState, sidebarDx), Type_Int, 0 },
    { offsetof(FileState, tocState), Type_IntArray, NULL },
    { offsetof(FileState, useGlobalValues), Type_Bool, false },
    { offsetof(FileState, windowPos.dx), Type_Int, 1 },
    { offsetof(FileState, windowPos.dy), Type_Int, 1 },
    { offsetof(FileState, windowState), Type_Int, 1 },
    { offsetof(FileState, windowPos.x), Type_Int, 1 },
    { offsetof(FileState, windowPos.y), Type_Int, 1 },
    { offsetof(FileState, zoomVirtual), Type_Float, (intptr_t)"100" },
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
    if (str::Eq(fieldName, "Favorites")) {
        BencArray *favDict = dict ? dict->GetArray(fieldName) : NULL;
        Vec<FileState *> *files = *(Vec<FileState *> **)fieldPtr;
        for (size_t j = 0; j < files->Count(); j++) {
            FileState *file = files->At(j);
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

#define DM_AUTOMATIC_STR            "automatic"
#define DM_SINGLE_PAGE_STR          "single page"
#define DM_FACING_STR               "facing"
#define DM_BOOK_VIEW_STR            "book view"
#define DM_CONTINUOUS_STR           "continuous"
#define DM_CONTINUOUS_FACING_STR    "continuous facing"
#define DM_CONTINUOUS_BOOK_VIEW_STR "continuous book view"

#define IS_STR_ENUM(enumName) \
    if (str::EqIS(txt, TEXT(enumName##_STR))) { \
        *mode = enumName; \
        return true; \
    }

// -view [continuous][singlepage|facing|bookview]
bool ParseViewMode(DisplayMode *mode, const WCHAR *txt)
{
    IS_STR_ENUM(DM_SINGLE_PAGE);
    IS_STR_ENUM(DM_CONTINUOUS);
    IS_STR_ENUM(DM_FACING);
    IS_STR_ENUM(DM_CONTINUOUS_FACING);
    IS_STR_ENUM(DM_BOOK_VIEW);
    IS_STR_ENUM(DM_CONTINUOUS_BOOK_VIEW);
    if (str::EqIS(txt, L"continuous single page")) {
        *mode = DM_CONTINUOUS;
    }
    return true;
}

namespace DisplayModeConv {

#define STR_FROM_ENUM(val) \
    if (val == var) \
        return TEXT(val##_STR);

const WCHAR *NameFromEnum(DisplayMode var)
{
    STR_FROM_ENUM(DM_AUTOMATIC)
    STR_FROM_ENUM(DM_SINGLE_PAGE)
    STR_FROM_ENUM(DM_FACING)
    STR_FROM_ENUM(DM_BOOK_VIEW)
    STR_FROM_ENUM(DM_CONTINUOUS)
    STR_FROM_ENUM(DM_CONTINUOUS_FACING)
    STR_FROM_ENUM(DM_CONTINUOUS_BOOK_VIEW)
    return L"unknown display mode!?";
}

#undef STR_FROM_ENUM

bool EnumFromName(const WCHAR *txt, DisplayMode *mode)
{
    IS_STR_ENUM(DM_AUTOMATIC)
    IS_STR_ENUM(DM_SINGLE_PAGE)
    IS_STR_ENUM(DM_FACING)
    IS_STR_ENUM(DM_BOOK_VIEW)
    IS_STR_ENUM(DM_CONTINUOUS)
    IS_STR_ENUM(DM_CONTINUOUS_FACING)
    IS_STR_ENUM(DM_CONTINUOUS_BOOK_VIEW)
    return false;
}

#undef IS_STR_ENUM

}

/* Caller needs to DeleteGlobalPrefs(gGlobalPrefs) */
bool LoadPrefs()
{
    CrashIf(gGlobalPrefs);

    ScopedMem<WCHAR> path(AppGenDataFilename(NEW_PREFS_FILE_NAME));
    ScopedMem<char> prefsData(file::ReadAll(path, NULL));
    gGlobalPrefs = (GlobalPrefs *)DeserializeStruct(&gGlobalPrefsInfo, prefsData);
    CrashAlwaysIf(!gGlobalPrefs);

#ifdef DISABLE_EBOOK_UI
    if (!file::Exists(path))
        gUserPrefs->ebookUI.traditionalEbookUI = true;
#endif

    if (!file::Exists(path)) {
        ScopedMem<WCHAR> bencPath(AppGenDataFilename(OLD_PREFS_FILE_NAME));
        ScopedMem<char> bencPrefsData(file::ReadAll(bencPath, NULL));
        DeserializeStructBenc(&gBencGlobalPrefs, bencPrefsData, gGlobalPrefs, BencGlobalPrefsCallback);
    }

    gUserPrefs = &gGlobalPrefs->userPrefs;
    ScopedMem<WCHAR> userPath(AppGenDataFilename(USER_PREFS_FILE_NAME));
    if (userPath && file::Exists(userPath)) {
        ScopedMem<char> userPrefsData(file::ReadAll(userPath, NULL));
        DeserializeStruct(&gUserPrefsInfo, userPrefsData, gUserPrefs);
    }

    if (!gGlobalPrefs->currLangCode || !trans::ValidateLangCode(gGlobalPrefs->currLangCode)) {
        // guess the ui language on first start
        gGlobalPrefs->currLangCode = str::Dup(trans::DetectUserLang());
    }
    gGlobalPrefs->lastPrefUpdate = file::GetModificationTime(path);
    bool ok = DisplayModeConv::EnumFromName(gGlobalPrefs->defaultDisplayMode, &gGlobalPrefs->defaultDisplayModeEnum);
    if (!ok)
        gGlobalPrefs->defaultDisplayModeEnum = DM_AUTOMATIC;

    int weekDiff = GetWeekCount() - gGlobalPrefs->openCountWeek;
    gGlobalPrefs->openCountWeek = GetWeekCount();
    for (size_t i = 0; i < gGlobalPrefs->fileStates->Count(); i++) {
        DisplayState *state = gGlobalPrefs->fileStates->At(i);
        ok = DisplayModeConv::EnumFromName(state->displayMode, &state->displayModeEnum);
        if (!ok)
            state->displayModeEnum = DM_AUTOMATIC;
        // "age" openCount statistics (cut in in half after every week)
        state->openCount >>= weekDiff;
        // work-around https://code.google.com/p/sumatrapdf/issues/detail?id=2140
        if (!IsValidZoom(state->zoomVirtual))
            state->zoomVirtual = 100.f;
    }

    if (!IsValidZoom(gGlobalPrefs->defaultZoom))
        gGlobalPrefs->defaultZoom = 100.f;

    gFileHistory.UpdateStatesSource(gGlobalPrefs->fileStates);

    // TODO: update gGlobalPrefs->unknownSettings

    return true;
}

// called whenever global preferences change or a file is
// added or removed from gFileHistory (in order to keep
// the list of recently opened documents in sync)
bool SavePrefs()
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
    gFileHistory.Purge();

    str::ReplacePtr(&gGlobalPrefs->defaultDisplayMode, DisplayModeConv::NameFromEnum(gGlobalPrefs->defaultDisplayModeEnum));

    for (size_t i = 0; i < gGlobalPrefs->fileStates->Count(); i++) {
        DisplayState *state = gGlobalPrefs->fileStates->At(i);
        str::ReplacePtr(&state->displayMode, DisplayModeConv::NameFromEnum(state->displayModeEnum));
        // BUG: 2140
        if (!IsValidZoom(state->zoomVirtual)) {
            dbglog::CrashLogF("Invalid ds->zoomVirtual: %.4f", state->zoomVirtual);
            const WCHAR *ext = path::GetExt(state->filePath);
            if (!str::IsEmpty(ext)) {
                ScopedMem<char> extA(str::conv::ToUtf8(ext));
                dbglog::CrashLogF("File type: %s", extA.Get());
            }
            dbglog::CrashLogF("DisplayMode: %d", state->displayMode);
            dbglog::CrashLogF("PageNo: %d", state->pageNo);
            CrashIf(true);
        }
    }

    size_t prefsDataSize;
    ScopedMem<char> prefsData(SerializeStruct(&gGlobalPrefsInfo, gGlobalPrefs, PREFS_INFO_URL, &prefsDataSize));
    CrashIf(!prefsData || 0 == prefsDataSize);
    if (!prefsData || 0 == prefsDataSize)
        return false;

    ScopedMem<WCHAR> path(AppGenDataFilename(NEW_PREFS_FILE_NAME));
    CrashIf(!path);
    if (!path)
        return false;

    // only save if anything's changed at all
    size_t prevPrefsDataSize;
    ScopedMem<char> prevPrefsData(file::ReadAll(path, &prevPrefsDataSize));
    if (prevPrefsDataSize == prefsDataSize && str::Eq(prefsData, prevPrefsData))
        return true;

    FileTransaction trans;
    bool ok = trans.WriteAll(path, prefsData, prefsDataSize) && trans.Commit();
    if (ok)
        gGlobalPrefs->lastPrefUpdate = file::GetModificationTime(path);
    if (!ok)
        return false;

    // notify all SumatraPDF instances about the updated prefs file
    HWND hwnd = NULL;
    while ((hwnd = FindWindowEx(HWND_DESKTOP, hwnd, FRAME_CLASS_NAME, NULL)) != NULL) {
        PostMessage(hwnd, UWM_PREFS_FILE_UPDATED, 0, 0);
    }
    return true;
}

// refresh the preferences when a different SumatraPDF process saves them
bool ReloadPrefs()
{
    ScopedMem<WCHAR> path(AppGenDataFilename(NEW_PREFS_FILE_NAME));
    if (!path || !file::Exists(path))
        return false;

    FILETIME time = file::GetModificationTime(path);
    if (time.dwLowDateTime == gGlobalPrefs->lastPrefUpdate.dwLowDateTime &&
        time.dwHighDateTime == gGlobalPrefs->lastPrefUpdate.dwHighDateTime) {
        return true;
    }

    ScopedMem<char> currLangCode(str::Dup(gGlobalPrefs->currLangCode));
    bool toolbarVisible = gGlobalPrefs->toolbarVisible;
    bool useSysColors = gGlobalPrefs->useSysColors;

    gFileHistory.UpdateStatesSource(NULL);
    DeleteGlobalPrefs(gGlobalPrefs);
    gGlobalPrefs = NULL;

    bool ok = LoadPrefs();
    if (!ok)
        return false;

    if (gWindows.Count() > 0 && gWindows.At(0)->IsAboutWindow()) {
        gWindows.At(0)->DeleteInfotip();
        gWindows.At(0)->RedrawAll(true);
    }

    if (!str::Eq(currLangCode, gGlobalPrefs->currLangCode)) {
        SetCurrentLanguageAndRefreshUi(gGlobalPrefs->currLangCode);
    }

    if (gGlobalPrefs->toolbarVisible != toolbarVisible)
        ShowOrHideToolbarGlobally();
    if (gGlobalPrefs->useSysColors != useSysColors)
        UpdateDocumentColors();
    UpdateFavoritesTreeForAllWindows();

    return true;
}
