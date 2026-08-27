/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"

#include "RegistrySearchFilter.h"

bool InstallSearchFilter(Str dllPath, bool allUsers) {
    struct {
        const char* key;
        const char* value;
        Str data;
    } regVals[] = {
        {"Software\\Classes\\CLSID\\" kPdfFilterClsid, nullptr, StrL("SumatraPDF IFilter")},
        {"Software\\Classes\\CLSID\\" kPdfFilterClsid "\\InProcServer32", nullptr, dllPath},
        {"Software\\Classes\\CLSID\\" kPdfFilterClsid "\\InProcServer32", "ThreadingModel", StrL("Both")},
        {"Software\\Classes\\CLSID\\" kPdfFilterHandler, nullptr, StrL("SumatraPDF IFilter Persistent Handler")},
        {"Software\\Classes\\CLSID\\" kPdfFilterHandler "\\PersistentAddinsRegistered", nullptr, StrL("")},
        {"Software\\Classes\\CLSID"
         "\\" kPdfFilterHandler "\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}",
         nullptr, StrL(kPdfFilterClsid)},
        {R"(Software\Classes\.pdf\PersistentHandler)", nullptr, StrL(kPdfFilterHandler)},
#ifdef BUILD_TEX_IFILTER
        {"Software\\Classes\\CLSID\\" kTexFilterClsid, nullptr, StrL("SumatraPDF IFilter")},
        {"Software\\Classes\\CLSID\\" kTexFilterClsid "\\InProcServer32", nullptr, dllPath},
        {"Software\\Classes\\CLSID\\" kTexFilterClsid "\\InProcServer32", "ThreadingModel", StrL("Both")},
        {"Software\\Classes\\CLSID\\" kTexFilterHandler, nullptr, StrL("SumatraPDF LaTeX IFilter Persistent Handler")},
        {"Software\\Classes\\CLSID\\" kTexFilterHandler "\\PersistentAddinsRegistered", nullptr, StrL("")},
        {"Software\\Classes\\CLSID"
         "\\" kTexFilterHandler "\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}",
         nullptr, StrL(kTexFilterClsid)},
        {"Software\\Classes\\.tex\\PersistentHandler", nullptr, StrL(kTexFilterHandler)},
#endif
#ifdef BUILD_EPUB_IFILTER
        {"Software\\Classes\\CLSID\\" kEpubFilterClsid, nullptr, StrL("SumatraPDF IFilter")},
        {"Software\\Classes\\CLSID\\" kEpubFilterClsid "\\InProcServer32", nullptr, dllPath},
        {"Software\\Classes\\CLSID\\" kEpubFilterClsid "\\InProcServer32", "ThreadingModel", StrL("Both")},
        {"Software\\Classes\\CLSID\\" kEpubFilterHandler, nullptr, StrL("SumatraPDF EPUB IFilter Persistent Handler")},
        {"Software\\Classes\\CLSID\\" kEpubFilterHandler "\\PersistentAddinsRegistered", nullptr, StrL("")},
        {"Software\\Classes\\CLSID"
         "\\" kEpubFilterHandler "\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}",
         nullptr, StrL(kEpubFilterClsid)},
        {"Software\\Classes\\.epub\\PersistentHandler", nullptr, StrL(kEpubFilterHandler)},
#endif
    };
    HKEY hkey = allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    for (auto& regVal : regVals) {
        auto keyName = regVal.key;
        auto valName = regVal.value;
        auto value = regVal.data;
        bool ok = LoggedWriteRegStr(hkey, Str(keyName), valName ? Str(valName) : Str(), value);
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
        R"(Software\Classes\.pdf\PersistentHandler)",
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

    for (auto regKey : regKeys) {
        LoggedDeleteRegKey(HKEY_LOCAL_MACHINE, Str(regKey));
        ok &= LoggedDeleteRegKey(HKEY_CURRENT_USER, Str(regKey));
    }
    return ok;
}

bool IsSearchFilterInstalled() {
    Str key = StrL(".pdf\\PersistentHandler");
    TempStr iid = LoggedReadRegStrTemp(HKEY_CLASSES_ROOT, key, Str());
    bool isInstalled = str::EqI(iid, StrL(kPdfFilterHandler));
    logf("IsSearchFilterInstalled() isInstalled=%d\n", (int)isInstalled);
    return isInstalled;
}