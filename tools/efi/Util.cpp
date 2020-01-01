/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Dict.h"

#include "Dia2Subset.h"
#include "Util.h"

void log(const char *s)
{
    fprintf(stderr, s);
}

static const struct DLLDesc
{
    const char *Filename;
    IID UseCLSID;
} msdiaDlls[] = {
    // this list is complete as of April 2013. In the future new msdia version
    // could be added, in which case it should be added to the top of this list
    "msdia110.dll", __uuidof(DiaSource110),     // Visual Studio 2012
    "msdia100.dll", __uuidof(DiaSource100),     // Visual Studio 2010
    // Note: there are also older version (msdia90.dll, msdia80.dll, msdia71.dll)
    // but they are not compatible because vtable layout for IDiaSymbol
    // changed in msdia100.dll
    0
};

// note: we leak g_dia_source but who cares
IDiaDataSource *g_dia_source = 0;

IDiaDataSource *LoadDia()
{
    if (g_dia_source)
        return g_dia_source;

    HRESULT hr = E_FAIL;

    // Try creating things "the official way"
    for (int i=0; msdiaDlls[i].Filename; i++)
    {
        hr = CoCreateInstance(msdiaDlls[i].UseCLSID,0,CLSCTX_INPROC_SERVER,
            __uuidof(IDiaDataSource),(void**) &g_dia_source);

        if (SUCCEEDED(hr)) {
            //logf("using registered dia %s\n", msdiaDlls[i].Filename);
            return g_dia_source;
        }
    }

    // None of the classes are registered, but most programmers will have the
    // DLLs on their system anyway and can copy it over; try loading it directly.

    for (int i=0; msdiaDlls[i].Filename; i++)
    {
        const char *dllName = msdiaDlls[i].Filename;
        // TODO: also try to find Visual Studio directories where it might exist. On
        // my system:
        // c:/Program Files/Common Files/microsoft shared/VC/msdia100.dll
        // c:/Program Files/Common Files/microsoft shared/VC/msdia90.dll
        // c:/Program Files/Microsoft Visual Studio 10.0/Common7/Packages/Debugger/msdia100.dll
        // c:/Program Files/Microsoft Visual Studio 10.0/DIA SDK/bin/msdia100.dll
        // c:/Program Files/Microsoft Visual Studio 11.0/Common7/IDE/Remote Debugger/x86/msdia110.dll
        // c:/Program Files/Microsoft Visual Studio 11.0/Common7/Packages/Debugger/msdia110.dll
        // c:/Program Files/Microsoft Visual Studio 11.0/DIA SDK/bin/msdia110.dll
        // c:/Program Files/Windows Kits/8.0/App Certification Kit/msdia100.dll
        // I'm sure Visual Studio 8 also puts them somewhere

        HMODULE hDll = LoadLibraryA(dllName);
        if (!hDll)
            continue;

        typedef HRESULT (__stdcall *PDllGetClassObject)(REFCLSID rclsid,REFIID riid,void** ppvObj);
        PDllGetClassObject DllGetClassObject = (PDllGetClassObject) GetProcAddress(hDll,"DllGetClassObject");
        if (DllGetClassObject)
        {
            // first create a class factory
            IClassFactory *classFactory;
            hr = DllGetClassObject(msdiaDlls[i].UseCLSID,IID_IClassFactory,(void**) &classFactory);
            if (SUCCEEDED(hr))
            {
                hr = classFactory->CreateInstance(0,__uuidof(IDiaDataSource),(void**) &g_dia_source);
                classFactory->Release();
                //logf("using loaded dia %s\n", dllName);
                return g_dia_source;
            } else {
                logf("DllGetClassObject() in %s failed", dllName);
            }
        } else {
            logf("dia dll found as %s but is missing DllGetClassObject function", dllName);
        }
        FreeLibrary(hDll);
    }
    log("  couldn't find (or properly initialize) any DIA dll, copying msdia*.dll to app dir might help.\n");
    return NULL;
}

void BStrToString(str::Str& strInOut, BSTR str, const char *defString, bool stripWhitespace)
{
    strInOut.Reset();
    if (!str) {
        strInOut.Append(defString);
        return;
    }

    OLECHAR c;
    int len = SysStringLen(str);
    for (int i=0; i<len; i++)
    {
        c = str[i];
        if (stripWhitespace && isspace(c))
            continue;
        if (c < 32 || c >= 128)
            c = '?';
        strInOut.Append((char)c);
    }
}
