/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Preview test: loads PdfPreview.dll, creates a thumbnail for a file,
// and saves it as preview.png.
// Activated with -test-preview <filename.ext>
// Only available in debug builds.

#include "utils/BaseUtil.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/WinUtil.h"
#include "utils/GdiPlusUtil.h"

#include "RegistryPreview.h"

#include "utils/Log.h"

#include <ObjBase.h>
#include <Shlwapi.h>
#include <Thumbcache.h>
#include <Unknwn.h>

typedef HRESULT DllGetClassObjectFn(REFCLSID rclsid, REFIID riid, void** ppv);

constexpr const char* kPdfPreviewDllName = "PdfPreview.dll";

void TestPreview(const WCHAR* cmdLine) {
    StrVec argList;
    ParseCmdLine(cmdLine, argList);

    // find args after -test-preview
    int idx = -1;
    for (int i = 0; i < argList.Size(); i++) {
        if (str::EqI(argList.At(i), "-test-preview")) {
            idx = i;
            break;
        }
    }

    if (idx < 0 || idx + 1 >= argList.Size()) {
        printf("Usage: SumatraPDF.exe -test-preview <filename>\n");
        return;
    }

    const char* filePathA = argList.At(idx + 1);
    TempWStr filePath = ToWStrTemp(filePathA);

    // use kPdfPreviewClsid by default
    GUID clsid{};
    TempWStr clsidW = ToWStrTemp(kPdfPreviewClsid);
    IIDFromString(clsidW, &clsid);

    HMODULE dll = LoadLibraryA(kPdfPreviewDllName);
    if (!dll) {
        printf("Can't load %s\n", kPdfPreviewDllName);
        return;
    }

    auto fn = (DllGetClassObjectFn*)GetProcAddress(dll, "DllGetClassObject");
    if (!fn) {
        printf("Can't find DllGetClassObject in %s\n", kPdfPreviewDllName);
        FreeLibrary(dll);
        return;
    }

    IClassFactory* pFactory = nullptr;
    HRESULT hr = fn(clsid, IID_IClassFactory, (void**)&pFactory);
    if (hr != S_OK || !pFactory) {
        printf("DllGetClassObject failed: 0x%08x\n", (unsigned)hr);
        FreeLibrary(dll);
        return;
    }

    IInitializeWithStream* pInit = nullptr;
    hr = pFactory->CreateInstance(nullptr, IID_IInitializeWithStream, (void**)&pInit);
    pFactory->Release();
    if (hr != S_OK || !pInit) {
        printf("CreateInstance(IInitializeWithStream) failed: 0x%08x\n", (unsigned)hr);
        FreeLibrary(dll);
        return;
    }

    IThumbnailProvider* pProvider = nullptr;
    hr = pInit->QueryInterface(IID_IThumbnailProvider, (void**)&pProvider);
    if (hr != S_OK || !pProvider) {
        printf("QueryInterface(IThumbnailProvider) failed: 0x%08x\n", (unsigned)hr);
        pInit->Release();
        FreeLibrary(dll);
        return;
    }

    IStream* pStream = nullptr;
    hr = SHCreateStreamOnFileEx(filePath, STGM_READ, 0, FALSE, nullptr, &pStream);
    if (hr != S_OK || !pStream) {
        printf("Can't open file: %s\n", filePathA);
        pProvider->Release();
        pInit->Release();
        FreeLibrary(dll);
        return;
    }

    hr = pInit->Initialize(pStream, 0);
    pInit->Release();
    pStream->Release();
    if (hr != S_OK) {
        printf("Initialize() failed: 0x%08x\n", (unsigned)hr);
        pProvider->Release();
        FreeLibrary(dll);
        return;
    }

    HBITMAP bmp = nullptr;
    WTS_ALPHATYPE alpha;
    hr = pProvider->GetThumbnail(256, &bmp, &alpha);
    pProvider->Release();
    if (hr != S_OK || !bmp) {
        printf("GetThumbnail() failed: 0x%08x\n", (unsigned)hr);
        FreeLibrary(dll);
        return;
    }

    Gdiplus::Bitmap gdipBmp(bmp, nullptr);
    CLSID pngClsid = GetEncoderClsid(L"image/png");
    Gdiplus::Status st = gdipBmp.Save(L"preview.png", &pngClsid);
    if (st != Gdiplus::Ok) {
        printf("Failed to save preview.png: %d\n", (int)st);
    } else {
        printf("Saved preview.png\n");
    }

    DeleteObject(bmp);
    FreeLibrary(dll);
}
