
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <ObjBase.h>
#include <Shlwapi.h>
#include <Thumbcache.h>
#include <Unknwn.h>

#include <stdio.h>

#include "utils/BaseUtil.h"

#define kPdfPreviewClsid L"{3D3B1846-CC43-42AE-BFF9-D914083C2BA3}"
#define kXpsPreviewClsid L"{D427A82C-6545-4FBE-8E87-030EDB3BE46D}"
#define kDjVuPreviewClsid L"{6689D0D4-1E9C-400A-8BCA-FA6C56B2C3B5}"
#define kEpubPreviewClsid L"{80C4E4B1-2B0F-40D5-95AF-BE7B57FEA4F9}"
#define kFb2PreviewClsid L"{D5878036-E863-403E-A62C-7B9C7453336A}"
#define kMobiPreviewClsid L"{42CA907E-BDF5-4A75-994A-E1AEC8A10954}"
#define kCbxPreviewClsid L"{C29D3E2B-8FF6-4033-A4E8-54221D859D74}"
#define kTgaPreviewClsid L"{CB1D63A6-FE5E-4DED-BEA5-3F6AF1A70D08}"

// Our GUID here:
LPCOLESTR myGuid = kPdfPreviewClsid;

typedef HRESULT ourDllGetClassObjectT(REFCLSID rclsid, REFIID riid, void** ppv);

void log(const char* s, int) {
    OutputDebugStringA(s);
    printf("%s", s);
}

void log(const char* s, bool) {
    int cb = (int)str::Len(s);
    log(s, cb);
}

constexpr const char* kPdfPreviewDllName = "PdfPreview.dll";

int main(int c, char** v) {
    GUID clsid{};
    IIDFromString(myGuid, &clsid);

    if (c < 2) {
        printf("not enough arguments: file name\n");
        return 1;
    }
    HRESULT r;
    IStream* pStream = NULL;
    HMODULE dll = NULL;

    dll = LoadLibraryA(kPdfPreviewDllName);
    if (!dll) {
        printf("can't open DLL\n");
        return 1;
    }

    ourDllGetClassObjectT* ourDllGetClassObject = (ourDllGetClassObjectT*)GetProcAddress(dll, "DllGetClassObject");

    IClassFactory* pFactory = NULL;
    r = ourDllGetClassObject(clsid, IID_IClassFactory, (void**)&pFactory);
    if (r != S_OK) {
        printf("failed: get factory: %08x\n", r);
        return 2;
    }

    IInitializeWithStream* pInit;
    r = pFactory->CreateInstance(NULL, IID_IInitializeWithStream, (void**)&pInit);
    if (r != S_OK) {
        printf("failed: get object\n");
        return 3;
    }
    pFactory->Release();

    IThumbnailProvider* pProvider;
    r = pInit->QueryInterface(IID_IThumbnailProvider, (void**)&pProvider);
    if (r != S_OK) {
        printf("failed: get provider\n");
        return 5;
    }

    wchar_t wfile[256]{};
    MultiByteToWideChar(CP_ACP, 0, v[1], -1, wfile, 256);
    r = SHCreateStreamOnFileEx(wfile, STGM_READ, 0, FALSE, NULL, &pStream);
    if (r != S_OK || !pStream) {
        printf("can't open file\n");
        return 10;
    }

    r = pInit->Initialize(pStream, 0);
    pInit->Release();
    pStream->Release();
    if (r != S_OK) {
        printf("failed: init provider\n");
        return 11;
    }

    HBITMAP bmp;
    WTS_ALPHATYPE alpha;
    r = pProvider->GetThumbnail(256, &bmp, &alpha);
    pProvider->Release();
    if (r != S_OK) {
        printf("failed: make thumbnail\n");
        return 12;
    }

    printf("done");
}
