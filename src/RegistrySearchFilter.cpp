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
        {L"Software\\Classes\\CLSID\\" SZ_PDF_FILTER_CLSID, nullptr, L"SumatraPDF IFilter"},
        {L"Software\\Classes\\CLSID\\" SZ_PDF_FILTER_CLSID L"\\InProcServer32", nullptr, dllPath},
        {L"Software\\Classes\\CLSID\\" SZ_PDF_FILTER_CLSID L"\\InProcServer32", L"ThreadingModel", L"Both"},
        {L"Software\\Classes\\CLSID\\" SZ_PDF_FILTER_HANDLER, nullptr, L"SumatraPDF IFilter Persistent Handler"},
        {L"Software\\Classes\\CLSID\\" SZ_PDF_FILTER_HANDLER L"\\PersistentAddinsRegistered", nullptr, L""},
        {L"Software\\Classes\\CLSID"
         L"\\" SZ_PDF_FILTER_HANDLER L"\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}",
         nullptr, SZ_PDF_FILTER_CLSID},
        {L"Software\\Classes\\.pdf\\PersistentHandler", nullptr, SZ_PDF_FILTER_HANDLER},
#ifdef BUILD_TEX_IFILTER
        {L"Software\\Classes\\CLSID\\" SZ_TEX_FILTER_CLSID, nullptr, L"SumatraPDF IFilter"},
        {L"Software\\Classes\\CLSID\\" SZ_TEX_FILTER_CLSID L"\\InProcServer32", nullptr, dllPath},
        {L"Software\\Classes\\CLSID\\" SZ_TEX_FILTER_CLSID L"\\InProcServer32", L"ThreadingModel", L"Both"},
        {L"Software\\Classes\\CLSID\\" SZ_TEX_FILTER_HANDLER, nullptr, L"SumatraPDF LaTeX IFilter Persistent Handler"},
        {L"Software\\Classes\\CLSID\\" SZ_TEX_FILTER_HANDLER L"\\PersistentAddinsRegistered", nullptr, L""},
        {L"Software\\Classes\\CLSID"
         L"\\" SZ_TEX_FILTER_HANDLER L"\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}",
         nullptr, SZ_TEX_FILTER_CLSID},
        {L"Software\\Classes\\.tex\\PersistentHandler", nullptr, SZ_TEX_FILTER_HANDLER},
#endif
#ifdef BUILD_EPUB_IFILTER
        {L"Software\\Classes\\CLSID\\" SZ_EPUB_FILTER_CLSID, nullptr, L"SumatraPDF IFilter"},
        {L"Software\\Classes\\CLSID\\" SZ_EPUB_FILTER_CLSID L"\\InProcServer32", nullptr, dllPath},
        {L"Software\\Classes\\CLSID\\" SZ_EPUB_FILTER_CLSID L"\\InProcServer32", L"ThreadingModel", L"Both"},
        {L"Software\\Classes\\CLSID\\" SZ_EPUB_FILTER_HANDLER, nullptr, L"SumatraPDF EPUB IFilter Persistent Handler"},
        {L"Software\\Classes\\CLSID\\" SZ_EPUB_FILTER_HANDLER L"\\PersistentAddinsRegistered", nullptr, L""},
        {L"Software\\Classes\\CLSID"
         L"\\" SZ_EPUB_FILTER_HANDLER L"\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}",
         nullptr, SZ_EPUB_FILTER_CLSID},
        {L"Software\\Classes\\.epub\\PersistentHandler", nullptr, SZ_EPUB_FILTER_HANDLER},
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

bool UninstallSearchFilter() {
    const WCHAR* regKeys[] = {
        L"Software\\Classes\\CLSID\\" SZ_PDF_FILTER_CLSID,  L"Software\\Classes\\CLSID\\" SZ_PDF_FILTER_HANDLER,
        L"Software\\Classes\\.pdf\\PersistentHandler",
#ifdef BUILD_TEX_IFILTER
        L"Software\\Classes\\CLSID\\" SZ_TEX_FILTER_CLSID,  L"Software\\Classes\\CLSID\\" SZ_TEX_FILTER_HANDLER,
        L"Software\\Classes\\.tex\\PersistentHandler",
#endif
#ifdef BUILD_EPUB_IFILTER
        L"Software\\Classes\\CLSID\\" SZ_EPUB_FILTER_CLSID, L"Software\\Classes\\CLSID\\" SZ_EPUB_FILTER_HANDLER,
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
    AutoFreeWstr iid = LoggedReadRegStr(HKEY_CLASSES_ROOT, key, nullptr);
    bool isInstalled = str::EqI(iid, SZ_PDF_FILTER_HANDLER);
    logf("IsSearchFilterInstalled() isInstalled=%d\n", (int)isInstalled);
    return isInstalled;
}
