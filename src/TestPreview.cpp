/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Preview test: loads PdfPreview.dll, creates a thumbnail for a file,
// and saves it as preview.png.
// Activated with -test-preview <filename.ext>
// Only available in debug builds.

#include "base/Base.h"
#include "base/CmdLineArgsIter.h"
#include "base/GdiPlus.h"

#include "RegistryPreview.h"

#include <thumbcache.h>

typedef HRESULT DllGetClassObjectFn(REFCLSID rclsid, REFIID riid, void** ppv);

#define kPdfPreviewDllName StrL("PdfPreview.dll")

void TestPreview(WStr cmdLine) {
    StrVec argList;
    ParseCmdLine(cmdLine, argList);

    // find args after -test-preview
    int idx = -1;
    for (int i = 0; i < len(argList); i++) {
        if (str::EqI(argList[i], "-test-preview")) {
            idx = i;
            break;
        }
    }

    if (idx < 0 || idx + 1 >= len(argList)) {
        printf("Usage: SumatraPDF.exe -test-preview <filename>\n");
        return;
    }

    Str filePathA = argList[idx + 1];
    WCHAR* filePath = CWStrTemp(filePathA);

    // use kPdfPreviewClsid by default
    GUID clsid{};
    WCHAR* clsidW = CWStrTemp(kPdfPreviewClsid);
    IIDFromString(clsidW, &clsid);

    HMODULE dll = LoadLibraryA(kPdfPreviewDllName.s);
    if (!dll) {
        printf("Can't load %s\n", kPdfPreviewDllName.s);
        return;
    }

    auto fn = (DllGetClassObjectFn*)GetProcAddress(dll, "DllGetClassObject");
    if (!fn) {
        printf("Can't find DllGetClassObject in %s\n", kPdfPreviewDllName.s);
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
        printf("Can't open file: %s\n", filePathA.s);
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
    CLSID pngClsid = GetGdiPlusEncoderClsid(L"image/png");
    Gdiplus::Status st = gdipBmp.Save(L"preview.png", &pngClsid);
    if (st != Gdiplus::Ok) {
        printf("Failed to save preview.png: %d\n", (int)st);
    } else {
        printf("Saved preview.png\n");
    }

    DeleteObject(bmp);
    FreeLibrary(dll);
}
