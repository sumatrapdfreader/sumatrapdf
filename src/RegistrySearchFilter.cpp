/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "RegistrySearchFilter.h"

#include "utils/Log.h"

bool InstallSearchFilter(const char* dllPath, bool allUsers) {
    struct {
        const char *key, *value, *data;
    } regVals[] = {
        {"Software\\Classes\\CLSID\\" kPdfFilterClsid, nullptr, "SumatraPDF IFilter"},
        {"Software\\Classes\\CLSID\\" kPdfFilterClsid "\\InProcServer32", nullptr, dllPath},
        {"Software\\Classes\\CLSID\\" kPdfFilterClsid "\\InProcServer32", "ThreadingModel", "Both"},
        {"Software\\Classes\\CLSID\\" kPdfFilterHandler, nullptr, "SumatraPDF IFilter Persistent Handler"},
        {"Software\\Classes\\CLSID\\" kPdfFilterHandler "\\PersistentAddinsRegistered", nullptr, ""},
        {"Software\\Classes\\CLSID"
         "\\" kPdfFilterHandler "\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}",
         nullptr, kPdfFilterClsid},
        {"Software\\Classes\\.pdf\\PersistentHandler", nullptr, kPdfFilterHandler},
#ifdef BUILD_TEX_IFILTER
        {"Software\\Classes\\CLSID\\" kTexFilterClsid, nullptr, "SumatraPDF IFilter"},
        {"Software\\Classes\\CLSID\\" kTexFilterClsid "\\InProcServer32", nullptr, dllPath},
        {"Software\\Classes\\CLSID\\" kTexFilterClsid "\\InProcServer32", "ThreadingModel", "Both"},
        {"Software\\Classes\\CLSID\\" kTexFilterHandler, nullptr, "SumatraPDF LaTeX IFilter Persistent Handler"},
        {"Software\\Classes\\CLSID\\" kTexFilterHandler "\\PersistentAddinsRegistered", nullptr, ""},
        {"Software\\Classes\\CLSID"
         "\\" kTexFilterHandler "\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}",
         nullptr, kTexFilterClsid},
        {"Software\\Classes\\.tex\\PersistentHandler", nullptr, kTexFilterHandler},
#endif
#ifdef BUILD_EPUB_IFILTER
        {"Software\\Classes\\CLSID\\" kEpubFilterClsid, nullptr, "SumatraPDF IFilter"},
        {"Software\\Classes\\CLSID\\" kEpubFilterClsid "\\InProcServer32", nullptr, dllPath},
        {"Software\\Classes\\CLSID\\" kEpubFilterClsid "\\InProcServer32", "ThreadingModel", "Both"},
        {"Software\\Classes\\CLSID\\" kEpubFilterHandler, nullptr, "SumatraPDF EPUB IFilter Persistent Handler"},
        {"Software\\Classes\\CLSID\\" kEpubFilterHandler "\\PersistentAddinsRegistered", nullptr, ""},
        {"Software\\Classes\\CLSID"
         "\\" kEpubFilterHandler "\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}",
         nullptr, kEpubFilterClsid},
        {"Software\\Classes\\.epub\\PersistentHandler", nullptr, kEpubFilterHandler},
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
    const char* regKeys[] = {
        "Software\\Classes\\CLSID\\" kPdfFilterClsid,  "Software\\Classes\\CLSID\\" kPdfFilterHandler,
        "Software\\Classes\\.pdf\\PersistentHandler",
#ifdef BUILD_TEX_IFILTER
        "Software\\Classes\\CLSID\\" kTexFilterClsid,  "Software\\Classes\\CLSID\\" kTexFilterHandler,
        "Software\\Classes\\.tex\\PersistentHandler",
#endif
#ifdef BUILD_EPUB_IFILTER
        "Software\\Classes\\CLSID\\" kEpubFilterClsid, "Software\\Classes\\CLSID\\" kEpubFilterHandler,
        "Software\\Classes\\.epub\\PersistentHandler",
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
    const char* key = ".pdf\\PersistentHandler";
    char* iid = LoggedReadRegStrTemp(HKEY_CLASSES_ROOT, key, nullptr);
    bool isInstalled = str::EqI(iid, kPdfFilterHandler);
    logf("IsSearchFilterInstalled() isInstalled=%d\n", (int)isInstalled);
    return isInstalled;
}
