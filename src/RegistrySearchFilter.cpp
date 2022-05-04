/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "RegistrySearchFilter.h"

#include "utils/Log.h"

bool InstallSearchFiler(const WCHAR* dllPath, bool allUsers) {
    struct {
        const WCHAR *key, *value, *data;
    } regVals[] = {
        {L"Software\\Classes\\CLSID\\" kPdfFilterClsid, nullptr, L"SumatraPDF IFilter"},
        {L"Software\\Classes\\CLSID\\" kPdfFilterClsid L"\\InProcServer32", nullptr, dllPath},
        {L"Software\\Classes\\CLSID\\" kPdfFilterClsid L"\\InProcServer32", L"ThreadingModel", L"Both"},
        {L"Software\\Classes\\CLSID\\" kPdfFilterHandler, nullptr, L"SumatraPDF IFilter Persistent Handler"},
        {L"Software\\Classes\\CLSID\\" kPdfFilterHandler L"\\PersistentAddinsRegistered", nullptr, L""},
        {L"Software\\Classes\\CLSID"
         L"\\" kPdfFilterHandler L"\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}",
         nullptr, kPdfFilterClsid},
        {L"Software\\Classes\\.pdf\\PersistentHandler", nullptr, kPdfFilterHandler},
#ifdef BUILD_TEX_IFILTER
        {L"Software\\Classes\\CLSID\\" kTexFilterClsid, nullptr, L"SumatraPDF IFilter"},
        {L"Software\\Classes\\CLSID\\" kTexFilterClsid L"\\InProcServer32", nullptr, dllPath},
        {L"Software\\Classes\\CLSID\\" kTexFilterClsid L"\\InProcServer32", L"ThreadingModel", L"Both"},
        {L"Software\\Classes\\CLSID\\" kTexFilterHandler, nullptr, L"SumatraPDF LaTeX IFilter Persistent Handler"},
        {L"Software\\Classes\\CLSID\\" kTexFilterHandler L"\\PersistentAddinsRegistered", nullptr, L""},
        {L"Software\\Classes\\CLSID"
         L"\\" kTexFilterHandler L"\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}",
         nullptr, kTexFilterClsid},
        {L"Software\\Classes\\.tex\\PersistentHandler", nullptr, kTexFilterHandler},
#endif
#ifdef BUILD_EPUB_IFILTER
        {L"Software\\Classes\\CLSID\\" kEpubFilterClsid, nullptr, L"SumatraPDF IFilter"},
        {L"Software\\Classes\\CLSID\\" kEpubFilterClsid L"\\InProcServer32", nullptr, dllPath},
        {L"Software\\Classes\\CLSID\\" kEpubFilterClsid L"\\InProcServer32", L"ThreadingModel", L"Both"},
        {L"Software\\Classes\\CLSID\\" kEpubFilterHandler, nullptr, L"SumatraPDF EPUB IFilter Persistent Handler"},
        {L"Software\\Classes\\CLSID\\" kEpubFilterHandler L"\\PersistentAddinsRegistered", nullptr, L""},
        {L"Software\\Classes\\CLSID"
         L"\\" kEpubFilterHandler L"\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}",
         nullptr, kEpubFilterClsid},
        {L"Software\\Classes\\.epub\\PersistentHandler", nullptr, kEpubFilterHandler},
#endif
    };
    HKEY hkey = allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    for (int i = 0; i < dimof(regVals); i++) {
        auto key = regVals[i].key;
        auto val = regVals[i].value;
        auto data = regVals[i].data;
        bool ok = LoggedWriteRegStr(hkey, key, val, data);
        if (!ok) {
            return false;
        }
    }
    return true;
}

// Note: for compat with pre-3.4 removes HKLM and HKCU keys
bool UninstallSearchFilter() {
    const WCHAR* regKeys[] = {
        L"Software\\Classes\\CLSID\\" kPdfFilterClsid,  L"Software\\Classes\\CLSID\\" kPdfFilterHandler,
        L"Software\\Classes\\.pdf\\PersistentHandler",
#ifdef BUILD_TEX_IFILTER
        L"Software\\Classes\\CLSID\\" kTexFilterClsid,  L"Software\\Classes\\CLSID\\" kTexFilterHandler,
        L"Software\\Classes\\.tex\\PersistentHandler",
#endif
#ifdef BUILD_EPUB_IFILTER
        L"Software\\Classes\\CLSID\\" kEpubFilterClsid, L"Software\\Classes\\CLSID\\" kEpubFilterHandler,
        L"Software\\Classes\\.epub\\PersistentHandler",
#endif
    };

    bool ok = true;

    for (int i = 0; i < dimof(regKeys); i++) {
        LoggedDeleteRegKey(HKEY_LOCAL_MACHINE, regKeys[i]);
        ok &= LoggedDeleteRegKey(HKEY_CURRENT_USER, regKeys[i]);
    }
    return ok;
}

bool IsSearchFilterInstalled() {
    const WCHAR* key = L".pdf\\PersistentHandler";
    AutoFreeWstr iid = LoggedReadRegStr2(key, nullptr);
    bool isInstalled = str::EqI(iid, kPdfFilterHandler);
    logf("IsSearchFilterInstalled() isInstalled=%d\n", (int)isInstalled);
    return isInstalled;
}
