/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "AppSettings.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraDialogs.h"
#include "Print.h"

#if defined(_MSC_VER) && __has_include(<PrintManagerInterop.h>) && __has_include(<DocumentSource.h>)

#pragma push_macro("NTDDI_VERSION")
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN8
#include <d2d1_1.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <roapi.h>
#include <wincodec.h>
#include <windows.foundation.h>
#include <windows.graphics.printing.h>
#include <DocumentSource.h>
#include <DocumentTarget.h>
#include <PrintManagerInterop.h>
#include <PrintPreview.h>
#include <wrl.h>
#include <wrl/event.h>
#pragma pop_macro("NTDDI_VERSION")

#include "PrintWin11.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::WinRtClassicComMix;

namespace Printing = ABI::Windows::Graphics::Printing;
namespace Foundation = ABI::Windows::Foundation;

using PrintRequestedHandler =
    Foundation::ITypedEventHandler<Printing::PrintManager*, Printing::PrintTaskRequestedEventArgs*>;

static decltype(&WindowsCreateString) gWindowsCreateString;

struct WinRtApi {
    decltype(&RoInitialize) roInitialize = nullptr;
    decltype(&RoUninitialize) roUninitialize = nullptr;
    decltype(&RoGetActivationFactory) roGetActivationFactory = nullptr;
    decltype(&WindowsCreateString) windowsCreateString = nullptr;
    decltype(&WindowsDeleteString) windowsDeleteString = nullptr;

    bool Load() {
        HMODULE combase = LoadLibraryW(L"combase.dll");
        if (!combase) {
            return false;
        }
        roInitialize = (decltype(roInitialize))GetProcAddress(combase, "RoInitialize");
        roUninitialize = (decltype(roUninitialize))GetProcAddress(combase, "RoUninitialize");
        roGetActivationFactory = (decltype(roGetActivationFactory))GetProcAddress(combase, "RoGetActivationFactory");
        windowsCreateString = (decltype(windowsCreateString))GetProcAddress(combase, "WindowsCreateString");
        windowsDeleteString = (decltype(windowsDeleteString))GetProcAddress(combase, "WindowsDeleteString");
        gWindowsCreateString = windowsCreateString;
        return roInitialize && roUninitialize && roGetActivationFactory && windowsCreateString && windowsDeleteString;
    }
};

class D2DFactoryLock {
    ID2D1Multithread* multithread = nullptr;

  public:
    explicit D2DFactoryLock(ID2D1Multithread* multithread) : multithread(multithread) {
        if (multithread) {
            multithread->Enter();
        }
    }

    ~D2DFactoryLock() {
        if (multithread) {
            multithread->Leave();
        }
    }
};

class PrintGraphics {
    ComPtr<ID2D1Factory1> d2dFactory;
    ComPtr<ID2D1Multithread> d2dMultithread;
    ComPtr<ID3D11Device> d3dDevice;
    ComPtr<ID2D1Device> d2dDevice;
    ComPtr<IWICImagingFactory> wicFactory;

  public:
    HRESULT Initialize() {
        if (d2dDevice) {
            return S_OK;
        }

        HMODULE d2dModule = LoadLibraryW(L"d2d1.dll");
        HMODULE d3dModule = LoadLibraryW(L"d3d11.dll");
        if (!d2dModule || !d3dModule) {
            return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
        }
        using D2D1CreateFactoryFn = HRESULT(WINAPI*)(D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS*, void**);
        auto d2dCreateFactory = (D2D1CreateFactoryFn)GetProcAddress(d2dModule, "D2D1CreateFactory");
        auto d3dCreateDevice = (decltype(&D3D11CreateDevice))GetProcAddress(d3dModule, "D3D11CreateDevice");
        if (!d2dCreateFactory || !d3dCreateDevice) {
            return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
        }

        D2D1_FACTORY_OPTIONS factoryOptions{};
        HRESULT hr = d2dCreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory1), &factoryOptions,
                                      (void**)d2dFactory.GetAddressOf());
        if (FAILED(hr)) {
            return hr;
        }
        hr = d2dFactory.As(&d2dMultithread);
        if (FAILED(hr)) {
            return hr;
        }

        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        hr = d3dCreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION,
                             &d3dDevice, nullptr, nullptr);
        if (FAILED(hr)) {
            hr = d3dCreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION,
                                 &d3dDevice, nullptr, nullptr);
        }
        if (FAILED(hr)) {
            return hr;
        }

        ComPtr<IDXGIDevice> dxgiDevice;
        hr = d3dDevice.As(&dxgiDevice);
        if (SUCCEEDED(hr)) {
            hr = d2dFactory->CreateDevice(dxgiDevice.Get(), &d2dDevice);
        }
        if (SUCCEEDED(hr)) {
            hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
        }
        return hr;
    }

    HRESULT CreatePreviewSurface(float width, float height, float dpi, ID3D11Texture2D** texture,
                                 IDXGISurface** surface, ID2D1DeviceContext** context) {
        if (!texture || !surface || !context || width <= 0 || height <= 0) {
            return E_INVALIDARG;
        }
        *texture = nullptr;
        *surface = nullptr;
        *context = nullptr;

        HRESULT hr = Initialize();
        if (FAILED(hr)) {
            return hr;
        }

        D2DFactoryLock factoryLock(d2dMultithread.Get());
        D3D11_TEXTURE2D_DESC textureDesc{};
        textureDesc.Width = (UINT)ceilf(width * dpi / 96.f);
        textureDesc.Height = (UINT)ceilf(height * dpi / 96.f);
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        ComPtr<ID3D11Texture2D> localTexture;
        hr = d3dDevice->CreateTexture2D(&textureDesc, nullptr, &localTexture);
        ComPtr<IDXGISurface> localSurface;
        if (SUCCEEDED(hr)) {
            hr = localTexture.As(&localSurface);
        }
        ComPtr<ID2D1DeviceContext> localContext;
        if (SUCCEEDED(hr)) {
            hr = d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &localContext);
        }

        D2D1_BITMAP_PROPERTIES1 bitmapProperties{};
        bitmapProperties.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
        bitmapProperties.dpiX = dpi;
        bitmapProperties.dpiY = dpi;
        bitmapProperties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
        ComPtr<ID2D1Bitmap1> targetBitmap;
        if (SUCCEEDED(hr)) {
            hr = localContext->CreateBitmapFromDxgiSurface(localSurface.Get(), &bitmapProperties, &targetBitmap);
        }
        if (SUCCEEDED(hr)) {
            localContext->SetTarget(targetBitmap.Get());
            localContext->SetDpi(dpi, dpi);
            *texture = localTexture.Detach();
            *surface = localSurface.Detach();
            *context = localContext.Detach();
        }
        return hr;
    }

    HRESULT CreatePrintControl(IPrintDocumentPackageTarget* packageTarget, float rasterDpi,
                               ID2D1PrintControl** printControl) {
        if (!packageTarget || !printControl) {
            return E_INVALIDARG;
        }
        *printControl = nullptr;
        HRESULT hr = Initialize();
        if (FAILED(hr)) {
            return hr;
        }
        D2D1_PRINT_CONTROL_PROPERTIES properties{};
        properties.rasterDPI = rasterDpi;
        properties.colorSpace = D2D1_COLOR_SPACE_SRGB;
        properties.fontSubset = D2D1_PRINT_FONT_SUBSET_MODE_DEFAULT;
        return d2dDevice->CreatePrintControl(wicFactory.Get(), packageTarget, &properties, printControl);
    }

    HRESULT CreateDeviceContext(ID2D1DeviceContext** context) {
        if (!context) {
            return E_INVALIDARG;
        }
        *context = nullptr;
        HRESULT hr = Initialize();
        if (FAILED(hr)) {
            return hr;
        }
        return d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, context);
    }
};

static Pixmap* ConvertToBgra(Pixmap* source) {
    if (!source) {
        return nullptr;
    }
    if (source->format == PixmapFormat::BGRA8 && source->data) {
        return source;
    }
    if (source->hbmp) {
        Pixmap* converted = PixmapCopyAs32bppDIB(source);
        FreePixmap(source);
        return converted;
    }
    if (!source->data) {
        FreePixmap(source);
        return nullptr;
    }

    Pixmap* converted = AllocPixmap(source->width, source->height, PixmapFormat::BGRA8);
    if (!converted) {
        FreePixmap(source);
        return nullptr;
    }
    for (int y = 0; y < source->height; y++) {
        const u8* src = source->data + (size_t)y * source->stride;
        u8* dst = converted->data + (size_t)y * converted->stride;
        for (int x = 0; x < source->width; x++) {
            if (source->format == PixmapFormat::BGR8) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = 255;
                src += 3;
            } else {
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
                dst[3] = src[3];
                src += 4;
            }
            dst += 4;
        }
    }
    FreePixmap(source);
    return converted;
}

class PrintDocumentSource final
    : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>, Printing::IPrintDocumentSource,
                          IPrintDocumentPageSource, IPrintPreviewPageCollection> {
  public:
    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING* runtimeName) override {
        if (!runtimeName) {
            return E_POINTER;
        }
        *runtimeName = nullptr;
        if (!gWindowsCreateString) {
            return E_NOTIMPL;
        }
        const WCHAR* name = L"Windows.Graphics.Printing.IPrintDocumentSource";
        return gWindowsCreateString(name, (UINT32)wcslen(name), runtimeName);
    }

    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel* trustLevel) override {
        if (!trustLevel) {
            return E_POINTER;
        }
        *trustLevel = BaseTrust;
        return S_OK;
    }

  private:
    EngineBase* engine = nullptr;
    int currentPage = 1;
    Print_Advanced_Data advanced;
    float previewDpi = 96.f;
    Mutex mutex;
    Vec<int> pages;
    ComPtr<Printing::IPrintTaskOptionsCore> previewOptions;
    ComPtr<IPrintPreviewDxgiPackageTarget> previewTarget;
    PrintGraphics graphics;

    int DocumentPage(UINT32 jobPage) const {
        if (jobPage < 1 || jobPage > (UINT32)len(pages)) {
            return 0;
        }
        return pages[(int)jobPage - 1];
    }

    void UseAllPages() {
        pages.Reset();
        for (int pageNo = 1; pageNo <= engine->PageCount(); pageNo++) {
            pages.Append(pageNo);
        }
    }

    void ReadPageRanges(IInspectable* options) {
        pages.Reset();
        ComPtr<Printing::IPrintTaskOptions2> options2;
        HRESULT hr = options->QueryInterface(IID_PPV_ARGS(&options2));
        ComPtr<__FIVector_1_Windows__CGraphics__CPrinting__CPrintPageRange> ranges;
        if (SUCCEEDED(hr)) {
            hr = options2->get_CustomPageRanges(&ranges);
        }
        UINT32 count = 0;
        if (SUCCEEDED(hr)) {
            hr = ranges->get_Size(&count);
        }
        if (FAILED(hr) || count == 0) {
            UseAllPages();
            return;
        }
        for (UINT32 i = 0; i < count; i++) {
            ComPtr<Printing::IPrintPageRange> range;
            if (FAILED(ranges->GetAt(i, range.GetAddressOf()))) {
                continue;
            }
            INT32 first = 0;
            INT32 last = 0;
            if (FAILED(range->get_FirstPageNumber(&first)) || FAILED(range->get_LastPageNumber(&last))) {
                continue;
            }
            first = std::max(1, first);
            last = std::min(engine->PageCount(), last);
            for (int pageNo = first; pageNo <= last; pageNo++) {
                pages.Append(pageNo);
            }
        }
        if (len(pages) == 0) {
            UseAllPages();
        }
    }

    HRESULT DrawPage(ID2D1DeviceContext* context, int pageNo, const Printing::PrintPageDescription& description,
                     float renderDpi) {
        float unitsPerDip = renderDpi / 96.f;
        Size paperSize((int)ceilf(description.PageSize.Width * unitsPerDip),
                       (int)ceilf(description.PageSize.Height * unitsPerDip));
        Rect printable((int)lroundf(description.ImageableRect.X * unitsPerDip),
                       (int)lroundf(description.ImageableRect.Y * unitsPerDip),
                       (int)lroundf(description.ImageableRect.Width * unitsPerDip),
                       (int)lroundf(description.ImageableRect.Height * unitsPerDip));
        PrintPageLayout layout;
        CalculatePrintPageLayout(*engine, pageNo, advanced, paperSize, printable, renderDpi, renderDpi,
                                 paperSize.dx < paperSize.dy, StrL("Windows print dialog"), layout);

        RenderPageArgs args(pageNo, layout.zoom, layout.rotation, nullptr, RenderTarget::Print);
        Pixmap* pixmap = ConvertToBgra(engine->RenderPage(args));
        if (!pixmap || !pixmap->data) {
            FreePixmap(pixmap);
            return E_FAIL;
        }

        D2D1_BITMAP_PROPERTIES1 bitmapProperties{};
        bitmapProperties.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
        bitmapProperties.dpiX = renderDpi;
        bitmapProperties.dpiY = renderDpi;
        ComPtr<ID2D1Bitmap1> bitmap;
        HRESULT hr = context->CreateBitmap(D2D1::SizeU((UINT32)pixmap->width, (UINT32)pixmap->height), pixmap->data,
                                           (UINT32)pixmap->stride, &bitmapProperties, &bitmap);
        if (SUCCEEDED(hr)) {
            Rect target;
            if (layout.isStretch) {
                target = Rect(printable.x, printable.y, printable.dx, printable.dy);
            } else {
                target =
                    Rect(printable.x + layout.offset.x, printable.y + layout.offset.y, pixmap->width, pixmap->height);
            }
            D2D1_RECT_F destination = D2D1::RectF(target.x / unitsPerDip, target.y / unitsPerDip,
                                                  target.BR().x / unitsPerDip, target.BR().y / unitsPerDip);
            context->DrawBitmap(bitmap.Get(), destination, 1.f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC, nullptr,
                                nullptr);
        }
        FreePixmap(pixmap);
        return hr;
    }

  public:
    HRESULT RuntimeClassInitialize(EngineBase* sourceEngine, int currentPage, PrintScaleAdv scale, float previewDpi) {
        if (!sourceEngine) {
            return E_INVALIDARG;
        }
        engine = sourceEngine;
        engine->AddRef();
        this->currentPage = currentPage;
        advanced = Print_Advanced_Data(PrintRangeAdv::All, scale);
        this->previewDpi = previewDpi;
        UseAllPages();
        return S_OK;
    }

    ~PrintDocumentSource() override { SafeEngineRelease(&engine); }

    HRESULT STDMETHODCALLTYPE GetPreviewPageCollection(IPrintDocumentPackageTarget* packageTarget,
                                                       IPrintPreviewPageCollection** collection) override {
        if (!packageTarget || !collection) {
            return E_INVALIDARG;
        }
        *collection = nullptr;
        HRESULT hr = packageTarget->GetPackageTarget(ID_PREVIEWPACKAGETARGET_DXGI, IID_PPV_ARGS(&previewTarget));
        if (SUCCEEDED(hr)) {
            hr = QueryInterface(IID_PPV_ARGS(collection));
        }
        return hr;
    }

    HRESULT STDMETHODCALLTYPE Paginate(UINT32, IInspectable* options) override {
        if (!options || !previewTarget) {
            return E_INVALIDARG;
        }
        ScopedMutex lock(&mutex);
        ReadPageRanges(options);
        HRESULT hr = options->QueryInterface(IID_PPV_ARGS(&previewOptions));
        if (SUCCEEDED(hr)) {
            hr = previewTarget->InvalidatePreview();
        }
        if (SUCCEEDED(hr)) {
            hr = previewTarget->SetJobPageCount(PageCountType::FinalPageCount, (UINT32)len(pages));
        }
        return hr;
    }

    HRESULT STDMETHODCALLTYPE MakePage(UINT32 desiredJobPage, float width, float height) override {
        if (!previewTarget || width <= 0 || height <= 0) {
            return E_INVALIDARG;
        }
        ScopedMutex lock(&mutex);
        UINT32 jobPage = desiredJobPage == JOB_PAGE_APPLICATION_DEFINED ? 1 : desiredJobPage;
        int pageNo = DocumentPage(jobPage);
        if (!pageNo || !previewOptions) {
            return E_INVALIDARG;
        }

        Printing::PrintPageDescription description{};
        HRESULT hr = previewOptions->GetPageDescription(jobPage, &description);
        if (FAILED(hr)) {
            return hr;
        }
        float scale = std::min(width / description.PageSize.Width, height / description.PageSize.Height);
        float renderDpi = std::max(24.f, previewDpi * scale);

        ComPtr<ID3D11Texture2D> texture;
        ComPtr<IDXGISurface> surface;
        ComPtr<ID2D1DeviceContext> context;
        hr = graphics.CreatePreviewSurface(width, height, previewDpi, &texture, &surface, &context);
        if (FAILED(hr)) {
            return hr;
        }
        context->BeginDraw();
        context->Clear(D2D1::ColorF(D2D1::ColorF::White));
        context->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale));
        hr = DrawPage(context.Get(), pageNo, description, renderDpi);
        HRESULT drawHr = context->EndDraw();
        if (SUCCEEDED(hr)) {
            hr = drawHr;
        }
        if (SUCCEEDED(hr)) {
            hr = previewTarget->DrawPage(jobPage, surface.Get(), previewDpi, previewDpi);
        }
        return hr;
    }

    HRESULT STDMETHODCALLTYPE MakeDocument(IInspectable* options, IPrintDocumentPackageTarget* packageTarget) override {
        if (!options || !packageTarget) {
            return E_INVALIDARG;
        }
        ScopedMutex lock(&mutex);
        ReadPageRanges(options);
        ComPtr<Printing::IPrintTaskOptionsCore> optionCore;
        HRESULT hr = options->QueryInterface(IID_PPV_ARGS(&optionCore));
        if (FAILED(hr)) {
            return hr;
        }

        Printing::PrintPageDescription firstDescription{};
        hr = optionCore->GetPageDescription(1, &firstDescription);
        float rasterDpi = (float)std::min(firstDescription.DpiX, firstDescription.DpiY);
        rasterDpi = std::max(96.f, std::min(300.f, rasterDpi));
        ComPtr<ID2D1PrintControl> printControl;
        if (SUCCEEDED(hr)) {
            hr = graphics.CreatePrintControl(packageTarget, rasterDpi, &printControl);
        }

        for (UINT32 jobPage = 1; SUCCEEDED(hr) && jobPage <= (UINT32)len(pages); jobPage++) {
            Printing::PrintPageDescription description{};
            hr = optionCore->GetPageDescription(jobPage, &description);
            ComPtr<ID2D1DeviceContext> context;
            if (SUCCEEDED(hr)) {
                hr = graphics.CreateDeviceContext(&context);
            }
            ComPtr<ID2D1CommandList> commands;
            if (SUCCEEDED(hr)) {
                hr = context->CreateCommandList(&commands);
            }
            if (SUCCEEDED(hr)) {
                context->SetTarget(commands.Get());
                context->BeginDraw();
                context->Clear(D2D1::ColorF(D2D1::ColorF::White));
                hr = DrawPage(context.Get(), DocumentPage(jobPage), description, rasterDpi);
                HRESULT drawHr = context->EndDraw();
                if (SUCCEEDED(hr)) {
                    hr = drawHr;
                }
            }
            if (SUCCEEDED(hr)) {
                hr = commands->Close();
            }
            if (SUCCEEDED(hr)) {
                D2D1_SIZE_F pageSize = D2D1::SizeF(description.PageSize.Width, description.PageSize.Height);
                hr = printControl->AddPage(commands.Get(), pageSize, nullptr);
            }
        }
        if (printControl) {
            HRESULT closeHr = printControl->Close();
            if (SUCCEEDED(hr)) {
                hr = closeHr;
            }
        }
        return hr;
    }
};

class Win11PrintSession {
    WinRtApi api;
    bool roInitialized = false;
    HWND hwnd = nullptr;
    EventRegistrationToken token{};
    ComPtr<IPrintManagerInterop> interop;
    ComPtr<Printing::IPrintManager> manager;
    ComPtr<PrintRequestedHandler> requestedHandler;
    ComPtr<PrintDocumentSource> source;
    WStr jobTitle;

    HRESULT OnPrintRequested(Printing::IPrintTaskRequestedEventArgs* args) {
        ComPtr<Printing::IPrintTaskRequest> request;
        HRESULT hr = args->get_Request(&request);
        ComPtr<Printing::IPrintTaskSourceRequestedHandler> sourceHandler;
        if (SUCCEEDED(hr)) {
            ComPtr<PrintDocumentSource> retainedSource = source;
            sourceHandler = Callback<Printing::IPrintTaskSourceRequestedHandler>(
                [retainedSource](Printing::IPrintTaskSourceRequestedArgs* sourceArgs) -> HRESULT {
                    ComPtr<Printing::IPrintDocumentSource> documentSource;
                    HRESULT sourceHr = retainedSource.As(&documentSource);
                    if (SUCCEEDED(sourceHr)) {
                        sourceHr = sourceArgs->SetSource(documentSource.Get());
                    }
                    return sourceHr;
                });
            if (!sourceHandler) {
                hr = E_OUTOFMEMORY;
            }
        }

        HSTRING title = nullptr;
        if (SUCCEEDED(hr)) {
            hr = api.windowsCreateString(jobTitle.s, (UINT32)len(jobTitle), &title);
        }
        ComPtr<Printing::IPrintTask> task;
        if (SUCCEEDED(hr)) {
            hr = request->CreatePrintTask(title, sourceHandler.Get(), &task);
        }
        if (title) {
            api.windowsDeleteString(title);
        }

        ComPtr<Printing::IPrintTaskOptionsCore> options;
        if (SUCCEEDED(hr)) {
            hr = task->get_Options(&options);
        }
        ComPtr<Printing::IPrintTaskOptions2> options2;
        if (SUCCEEDED(hr) && SUCCEEDED(options.As(&options2))) {
            ComPtr<Printing::IPrintPageRangeOptions> rangeOptions;
            if (SUCCEEDED(options2->get_PageRangeOptions(&rangeOptions))) {
                rangeOptions->put_AllowAllPages(true);
                rangeOptions->put_AllowCurrentPage(true);
                rangeOptions->put_AllowCustomSetOfPages(true);
            }
        }
        ComPtr<Printing::IPrintTask2> task2;
        if (SUCCEEDED(hr) && SUCCEEDED(task.As(&task2))) {
            task2->put_IsPreviewEnabled(true);
        }
        return hr;
    }

  public:
    ~Win11PrintSession() {
        if (manager && token.value) {
            manager->remove_PrintTaskRequested(token);
        }
        wstr::Free(jobTitle);
        if (roInitialized) {
            api.roUninitialize();
        }
    }

    HRESULT Initialize(HWND hwnd, EngineBase* engine, int currentPage, PrintScaleAdv scale, float previewDpi) {
        if (!api.Load()) {
            return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
        }
        HRESULT hr = api.roInitialize(RO_INIT_SINGLETHREADED);
        if (SUCCEEDED(hr)) {
            roInitialized = true;
        } else if (hr != RPC_E_CHANGED_MODE) {
            return hr;
        }

        EngineBase* printEngine = engine->Clone();
        if (!printEngine && engine->FilePath()) {
            printEngine = CreateEngineFromFile(engine->FilePath(), nullptr, false);
        }
        if (!printEngine) {
            printEngine = engine;
            printEngine->AddRef();
        }
        hr = MakeAndInitialize<PrintDocumentSource>(&source, printEngine, currentPage, scale, previewDpi);
        printEngine->Release();
        if (FAILED(hr)) {
            return hr;
        }

        this->hwnd = hwnd;
        TempStr baseName = path::GetBaseNameTemp(engine->FilePath());
        jobTitle = wstr::Dup(ToWStrTemp(baseName ? baseName : StrL("SumatraPDF document")));

        HSTRING className = nullptr;
        const WCHAR* runtimeClass = RuntimeClass_Windows_Graphics_Printing_PrintManager;
        hr = api.windowsCreateString(runtimeClass, (UINT32)wcslen(runtimeClass), &className);
        if (SUCCEEDED(hr)) {
            hr = api.roGetActivationFactory(className, IID_PPV_ARGS(&interop));
        }
        if (className) {
            api.windowsDeleteString(className);
        }
        if (SUCCEEDED(hr)) {
            hr = interop->GetForWindow(hwnd, IID_PPV_ARGS(&manager));
        }
        if (SUCCEEDED(hr)) {
            requestedHandler = Callback<PrintRequestedHandler>(
                [this](Printing::IPrintManager*, Printing::IPrintTaskRequestedEventArgs* args) -> HRESULT {
                    return OnPrintRequested(args);
                });
            if (!requestedHandler) {
                hr = E_OUTOFMEMORY;
            }
        }
        if (SUCCEEDED(hr)) {
            hr = manager->add_PrintTaskRequested(requestedHandler.Get(), &token);
        }
        return hr;
    }

    HRESULT Show() {
        ComPtr<__FIAsyncOperation_1_boolean> operation;
        return interop->ShowPrintUIForWindowAsync(hwnd, IID_PPV_ARGS(&operation));
    }
};

static Win11PrintSession* gPrintSession = nullptr;

bool TryPrintCurrentFileWin11(MainWindow* win, PrintScaleAdv defaultScale) {
    if (!win || !win->hwndFrame || !win->AsFixed()) {
        return false;
    }
    if (win->CurrentTab()->selectionOnPage) {
        return false;
    }
    EngineBase* engine = win->AsFixed()->GetEngine();
    if (!engine) {
        return false;
    }

    HDC hdc = GetDC(win->hwndFrame);
    float previewDpi = hdc ? (float)GetDeviceCaps(hdc, LOGPIXELSX) : 96.f;
    if (hdc) {
        ReleaseDC(win->hwndFrame, hdc);
    }
    previewDpi = std::max(96.f, std::min(240.f, previewDpi));

    delete gPrintSession;
    gPrintSession = new Win11PrintSession();
    HRESULT hr =
        gPrintSession->Initialize(win->hwndFrame, engine, win->AsFixed()->CurrentPageNo(), defaultScale, previewDpi);
    if (SUCCEEDED(hr)) {
        hr = gPrintSession->Show();
    }
    if (FAILED(hr)) {
        logf("Windows print dialog unavailable: 0x%08x\n", (uint)hr);
        delete gPrintSession;
        gPrintSession = nullptr;
        return false;
    }
    return true;
}

#else

#include "PrintWin11.h"

bool TryPrintCurrentFileWin11(MainWindow*, PrintScaleAdv) {
    return false;
}

#endif
