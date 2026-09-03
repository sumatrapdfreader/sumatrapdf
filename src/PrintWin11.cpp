/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
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
#include "Translations.h"
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
#include <windows.foundation.collections.h>
#include <windows.graphics.printing.h>
#include <windows.graphics.printing.optiondetails.h>
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

// Windows 11 is 10.0.22000 and up
constexpr DWORD kWin11Build = 22000;

// the cap keeps the bitmap of a large page within reason
constexpr float kMaxRasterDpi = 300.f;

// peak bytes of a rendered band, matching the GDI print path. Bounds what the
// engine has to allocate for one render, whatever the page size and print DPI
constexpr i64 kMaxBandBytes = 16LL * 1024 * 1024;

namespace Printing = ABI::Windows::Graphics::Printing;
namespace Foundation = ABI::Windows::Foundation;

namespace OptDetails = ABI::Windows::Graphics::Printing::OptionDetails;

using PrintRequestedHandler =
    Foundation::ITypedEventHandler<Printing::PrintManager*, Printing::PrintTaskRequestedEventArgs*>;

using OptionChangedHandler =
    Foundation::ITypedEventHandler<OptDetails::PrintTaskOptionDetails*, OptDetails::PrintTaskOptionChangedEventArgs*>;

// ids of the options we add to the dialog's "More settings" pane. They are ours,
// so they only have to be unique within the print task
static const WCHAR* kOptCenterHorizontally = L"sumatraCenterHorizontally";
static const WCHAR* kOptExtraRotation = L"sumatraExtraRotation";

// item ids of the rotation option, which is also how its value comes back
static const WCHAR* kRotationItems[] = {L"0", L"90", L"180", L"270"};

struct WinRtApi {
    decltype(&RoInitialize) roInitialize = nullptr;
    decltype(&RoGetActivationFactory) roGetActivationFactory = nullptr;
    decltype(&WindowsCreateString) windowsCreateString = nullptr;
    decltype(&WindowsDeleteString) windowsDeleteString = nullptr;
    decltype(&WindowsGetStringRawBuffer) windowsGetStringRawBuffer = nullptr;

    bool Load() {
        // combase is already in the process and is never unloaded, so prefer the
        // handle we can take without adding a reference on every print
        HMODULE combase = GetModuleHandleW(L"combase.dll");
        if (!combase) {
            combase = LoadLibraryW(L"combase.dll");
        }
        if (!combase) {
            return false;
        }
        roInitialize = (decltype(roInitialize))GetProcAddress(combase, "RoInitialize");
        roGetActivationFactory = (decltype(roGetActivationFactory))GetProcAddress(combase, "RoGetActivationFactory");
        windowsCreateString = (decltype(windowsCreateString))GetProcAddress(combase, "WindowsCreateString");
        windowsDeleteString = (decltype(windowsDeleteString))GetProcAddress(combase, "WindowsDeleteString");
        windowsGetStringRawBuffer =
            (decltype(windowsGetStringRawBuffer))GetProcAddress(combase, "WindowsGetStringRawBuffer");
        return roInitialize && roGetActivationFactory && windowsCreateString && windowsDeleteString &&
               windowsGetStringRawBuffer;
    }
};

// loaded and initialized once, by EnsureWinRt()
static WinRtApi gWinRt;

// an HSTRING that frees itself; the WinRT calls below need a lot of them
struct ScopedHStr {
    HSTRING h = nullptr;

    ScopedHStr() = default;
    explicit ScopedHStr(const WCHAR* s) {
        if (s) {
            gWinRt.windowsCreateString(s, (UINT32)wcslen(s), &h);
        }
    }
    ScopedHStr(const ScopedHStr&) = delete;
    ScopedHStr& operator=(const ScopedHStr&) = delete;

    ~ScopedHStr() {
        if (h) {
            gWinRt.windowsDeleteString(h);
        }
    }
};

template <typename T>
static HRESULT GetActivationFactory(const WCHAR* runtimeClass, ComPtr<T>& factory) {
    ScopedHStr cls(runtimeClass);
    if (!cls.h) {
        return E_OUTOFMEMORY;
    }
    return gWinRt.roGetActivationFactory(cls.h, IID_PPV_ARGS(&factory));
}

// the option details hang off the print task's options, so both the dialog side
// (creating the options) and the render side (reading them) start here
static HRESULT GetOptionDetails(Printing::IPrintTaskOptionsCore* options,
                                ComPtr<OptDetails::IPrintTaskOptionDetails>& details) {
    ComPtr<OptDetails::IPrintTaskOptionDetailsStatic> statics;
    HRESULT hr =
        GetActivationFactory(RuntimeClass_Windows_Graphics_Printing_OptionDetails_PrintTaskOptionDetails, statics);
    if (FAILED(hr)) {
        return hr;
    }
    return statics->GetFromPrintTaskOptions(options, &details);
}

static HRESULT GetOptionValue(OptDetails::IPrintTaskOptionDetails* details, const WCHAR* optionId,
                              ComPtr<IInspectable>& value) {
    ComPtr<__FIMapView_2_HSTRING_Windows__CGraphics__CPrinting__COptionDetails__CIPrintOptionDetails> options;
    HRESULT hr = details->get_Options(&options);
    if (FAILED(hr)) {
        return hr;
    }
    ScopedHStr key(optionId);
    ComPtr<OptDetails::IPrintOptionDetails> option;
    hr = options->Lookup(key.h, &option);
    if (FAILED(hr)) {
        return hr;
    }
    return option->get_Value(&value);
}

static void SetOptionValue(OptDetails::IPrintOptionDetails* option, IInspectable* value) {
    boolean ok = false;
    option->TrySetValue(value, &ok);
}

static void SetOptionBool(OptDetails::IPrintOptionDetails* option, bool value) {
    ComPtr<Foundation::IPropertyValueStatics> statics;
    if (FAILED(GetActivationFactory(RuntimeClass_Windows_Foundation_PropertyValue, statics))) {
        return;
    }
    ComPtr<IInspectable> boxed;
    if (SUCCEEDED(statics->CreateBoolean((boolean)value, &boxed))) {
        SetOptionValue(option, boxed.Get());
    }
}

static void SetOptionStr(OptDetails::IPrintOptionDetails* option, const WCHAR* value) {
    ComPtr<Foundation::IPropertyValueStatics> statics;
    if (FAILED(GetActivationFactory(RuntimeClass_Windows_Foundation_PropertyValue, statics))) {
        return;
    }
    ScopedHStr str(value);
    ComPtr<IInspectable> boxed;
    if (SUCCEEDED(statics->CreateString(str.h, &boxed))) {
        SetOptionValue(option, boxed.Get());
    }
}

static bool UnboxBool(IInspectable* value, bool defVal) {
    ComPtr<Foundation::IPropertyValue> prop;
    if (!value || FAILED(value->QueryInterface(IID_PPV_ARGS(&prop)))) {
        return defVal;
    }
    boolean res = 0;
    if (FAILED(prop->GetBoolean(&res))) {
        return defVal;
    }
    return res != 0;
}

// the rotation option's value is the item id, i.e. "0", "90", "180" or "270"
static int UnboxRotation(IInspectable* value, int defVal) {
    ComPtr<Foundation::IPropertyValue> prop;
    if (!value || FAILED(value->QueryInterface(IID_PPV_ARGS(&prop)))) {
        return defVal;
    }
    HSTRING hstr = nullptr;
    if (FAILED(prop->GetString(&hstr)) || !hstr) {
        return defVal;
    }
    const WCHAR* str = gWinRt.windowsGetStringRawBuffer(hstr, nullptr);
    int res = defVal;
    for (int i = 0; str && i < dimofi(kRotationItems); i++) {
        if (wstr::Eq(str, kRotationItems[i])) {
            res = i * 90;
            break;
        }
    }
    gWinRt.windowsDeleteString(hstr);
    return res;
}

// the Advanced page's labels carry an accelerator marker the print dialog has
// no use for
static TempWStr OptionLabelTemp(Str label) {
    TempStr noAccel = str::DupTemp(label);
    str::RemoveCharsInPlace(noAccel, StrL("&"));
    return ToWStrTemp(Str(noAccel.s));
}

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
        if (!gWinRt.windowsCreateString) {
            return E_NOTIMPL;
        }
        const WCHAR* name = L"Windows.Graphics.Printing.IPrintDocumentSource";
        return gWinRt.windowsCreateString(name, (UINT32)wcslen(name), runtimeName);
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

    // the preview asks for JOB_PAGE_APPLICATION_DEFINED to let us pick the page
    // it opens on: the one the user is looking at. It's also what "Current page"
    // in the dialog prints, since that's the page the preview is showing.
    UINT32 FirstJobPage() const {
        int idx = VecFind(pages, currentPage);
        if (idx < 0) {
            return 1;
        }
        return (UINT32)idx + 1;
    }

    void UseAllPages() {
        EnsureFullLayout(engine);
        VecClear(pages);
        for (int pageNo = 1; pageNo <= engine->PageCount(); pageNo++) {
            VecAppend(pages, pageNo);
        }
    }

    // what the user picked in the dialog's "More settings" pane; the rest of
    // Print_Advanced_Data has no equivalent there and keeps its default
    void ReadAdvancedOptions(IInspectable* options) {
        ComPtr<Printing::IPrintTaskOptionsCore> core;
        if (FAILED(options->QueryInterface(IID_PPV_ARGS(&core)))) {
            return;
        }
        ComPtr<OptDetails::IPrintTaskOptionDetails> details;
        if (FAILED(GetOptionDetails(core.Get(), details))) {
            return;
        }
        ComPtr<IInspectable> value;
        if (SUCCEEDED(GetOptionValue(details.Get(), kOptCenterHorizontally, value))) {
            advanced.centerHorizontally = UnboxBool(value.Get(), advanced.centerHorizontally);
        }
        value.Reset();
        if (SUCCEEDED(GetOptionValue(details.Get(), kOptExtraRotation, value))) {
            advanced.extraRotation = UnboxRotation(value.Get(), advanced.extraRotation);
        }
    }

    void ReadPageRanges(IInspectable* options) {
        EnsureFullLayout(engine);
        VecClear(pages);
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
                VecAppend(pages, pageNo);
            }
        }
        if (len(pages) == 0) {
            UseAllPages();
        }
    }

    // Rasterizes pageNo into a bitmap of the laid-out page. The page is rendered
    // in horizontal device-pixel bands and copied into the bitmap band by band,
    // so the engine never allocates a whole page at print DPI -- the GDI path
    // bands for the same reason (see PrintPageInBands())
    HRESULT RenderPageBitmap(ID2D1DeviceContext* context, int pageNo, const PrintPageLayout& layout, float renderDpi,
                             ID2D1Bitmap1** bitmapOut, Size& sizeOut) {
        *bitmapOut = nullptr;
        RectF mediabox = engine->PageMediabox(pageNo);
        RectF devFull = engine->Transform(mediabox, pageNo, layout.zoom, layout.rotation);
        int fullW = (int)lroundf(devFull.dx);
        int fullH = (int)lroundf(devFull.dy);
        if (fullW <= 0 || fullH <= 0) {
            return E_FAIL;
        }

        D2D1_BITMAP_PROPERTIES1 bitmapProperties{};
        bitmapProperties.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
        bitmapProperties.dpiX = renderDpi;
        bitmapProperties.dpiY = renderDpi;
        ComPtr<ID2D1Bitmap1> bitmap;
        HRESULT hr =
            context->CreateBitmap(D2D1::SizeU((UINT32)fullW, (UINT32)fullH), nullptr, 0, bitmapProperties, &bitmap);
        if (FAILED(hr)) {
            return hr;
        }

        int bandH = (int)std::max((i64)1, kMaxBandBytes / ((i64)fullW * 4));
        bandH = std::min(bandH, fullH);
        int dy = 0;
        while (dy < fullH) {
            int h = std::min(bandH, fullH - dy);
            // the page-space rectangle that renders to exactly these device rows;
            // the inverse transform takes care of rotation
            RectF devBand(devFull.x, devFull.y + (float)dy, devFull.dx, (float)h);
            RectF pageBand = engine->Transform(devBand, pageNo, layout.zoom, layout.rotation, /* inverse */ true);
            RenderPageArgs args(pageNo, layout.zoom, layout.rotation, &pageBand, RenderTarget::Print);
            Pixmap* band = ConvertToBgra(engine->RenderPage(args));
            if (!band || !band->data) {
                FreePixmap(band);
                // couldn't allocate even a band: try thinner ones before giving
                // up, so the page still prints at full resolution
                if (bandH > 1) {
                    bandH = std::max(1, bandH / 2);
                    continue; // retry the same rows with a thinner band
                }
                return E_FAIL;
            }
            // the engine can be a pixel off on either axis; copy what both agree on
            UINT32 w = (UINT32)std::min(band->width, fullW);
            UINT32 rows = (UINT32)std::min(band->height, fullH - dy);
            D2D1_RECT_U dst = D2D1::RectU(0, (UINT32)dy, w, (UINT32)dy + rows);
            hr = bitmap->CopyFromMemory(&dst, band->data, (UINT32)band->stride);
            FreePixmap(band);
            if (FAILED(hr)) {
                return hr;
            }
            dy += h;
        }
        sizeOut = Size(fullW, fullH);
        *bitmapOut = bitmap.Detach();
        return S_OK;
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
        PrintPageLayout layout =
            CalculatePrintPageLayout(*engine, pageNo, advanced, paperSize, printable, renderDpi, renderDpi,
                                     paperSize.dx < paperSize.dy, StrL("Windows print dialog"));

        ComPtr<ID2D1Bitmap1> bitmap;
        Size pageSize;
        HRESULT hr = RenderPageBitmap(context, pageNo, layout, renderDpi, &bitmap, pageSize);
        if (FAILED(hr)) {
            return hr;
        }

        Rect target;
        if (layout.isStretch) {
            target = Rect(printable.x, printable.y, printable.dx, printable.dy);
        } else {
            target = Rect(printable.x + layout.offset.x, printable.y + layout.offset.y, pageSize.dx, pageSize.dy);
        }
        D2D1_RECT_F destination = D2D1::RectF(target.x / unitsPerDip, target.y / unitsPerDip,
                                              target.BR().x / unitsPerDip, target.BR().y / unitsPerDip);
        context->DrawBitmap(bitmap.Get(), destination, 1.f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC, nullptr,
                            nullptr);
        return S_OK;
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

    // a custom option changed: nothing re-paginates on its own, so ask for the
    // preview to be built again with the new value
    void InvalidatePreview() {
        ScopedMutex lock(&mutex);
        if (previewTarget) {
            previewTarget->InvalidatePreview();
        }
    }

    const Print_Advanced_Data& Advanced() const { return advanced; }

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
        ReadAdvancedOptions(options);
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
        UINT32 jobPage = desiredJobPage == JOB_PAGE_APPLICATION_DEFINED ? FirstJobPage() : desiredJobPage;
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
        ReadAdvancedOptions(options);
        ReadPageRanges(options);
        ComPtr<Printing::IPrintTaskOptionsCore> optionCore;
        HRESULT hr = options->QueryInterface(IID_PPV_ARGS(&optionCore));
        if (FAILED(hr)) {
            return hr;
        }

        Printing::PrintPageDescription firstDescription{};
        hr = optionCore->GetPageDescription(1, &firstDescription);
        if (FAILED(hr)) {
            // without it rasterDpi would come from a zeroed description, i.e. we'd
            // silently print everything at the 96 dpi floor
            return hr;
        }
        float rasterDpi = (float)std::min(firstDescription.DpiX, firstDescription.DpiY);
        rasterDpi = std::max(96.f, std::min(kMaxRasterDpi, rasterDpi));
        ComPtr<ID2D1PrintControl> printControl;
        hr = graphics.CreatePrintControl(packageTarget, rasterDpi, &printControl);

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

// WinRT is initialized on the UI thread (the only thread that opens the print
// dialog) and left initialized: the print pipeline outlives the session that
// started it, so uninitializing when a session goes away would pull the ground
// from under a job that is still running.
static HRESULT EnsureWinRt() {
    if (!gWinRt.Load()) {
        return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
    }
    static bool initialized = false;
    if (initialized) {
        return S_OK;
    }
    HRESULT hr = gWinRt.roInitialize(RO_INIT_SINGLETHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return hr;
    }
    initialized = true;
    return S_OK;
}

class Win11PrintSession {
    HWND hwnd = nullptr;
    EventRegistrationToken token{};
    ComPtr<IPrintManagerInterop> interop;
    ComPtr<Printing::IPrintManager> manager;
    ComPtr<PrintRequestedHandler> requestedHandler;
    ComPtr<PrintDocumentSource> source;
    ComPtr<OptDetails::IPrintTaskOptionDetails> optionDetails;
    EventRegistrationToken optionToken{};
    WStr jobTitle;

    // Sumatra's Advanced print options go into the dialog's "More settings"
    // pane as custom options; PrintDocumentSource reads them back when it lays
    // the pages out. The paper tray is a standard option the dialog already
    // knows how to show, it just isn't among the ones it shows by default.
    // The rest of Print_Advanced_Data has no equivalent here: autoRotate is on
    // in both paths and settable in neither, and the two paper-size options
    // work by rewriting a DEVMODE, which the system owns in this path.
    HRESULT AddAdvancedOptions(Printing::IPrintTaskOptionsCore* options) {
        ComPtr<OptDetails::IPrintTaskOptionDetails> details;
        HRESULT hr = GetOptionDetails(options, details);
        if (FAILED(hr)) {
            return hr;
        }
        const Print_Advanced_Data& advanced = source->Advanced();

        ComPtr<OptDetails::IPrintTaskOptionDetails2> details2;
        hr = details.As(&details2);
        ComPtr<OptDetails::IPrintOptionDetails> centerOption;
        if (SUCCEEDED(hr)) {
            ScopedHStr id(kOptCenterHorizontally);
            ScopedHStr name(OptionLabelTemp(_TRA("Center page hori&zontally on the paper")).s);
            hr = details2->CreateToggleOption(id.h, name.h, &centerOption);
        }
        if (SUCCEEDED(hr)) {
            SetOptionBool(centerOption.Get(), advanced.centerHorizontally);
        }

        ComPtr<OptDetails::IPrintOptionDetails> rotateOption;
        if (SUCCEEDED(hr)) {
            ScopedHStr id(kOptExtraRotation);
            ScopedHStr name(OptionLabelTemp(_TRA("&Rotate printout:")).s);
            hr = details->CreateItemListOption(id.h, name.h, &rotateOption);
        }
        if (SUCCEEDED(hr)) {
            ComPtr<OptDetails::IPrintCustomItemListOptionDetails> items;
            hr = rotateOption.As(&items);
            for (int i = 0; SUCCEEDED(hr) && i < dimofi(kRotationItems); i++) {
                ScopedHStr itemId(kRotationItems[i]);
                // "None", then the degrees, matching the Advanced page
                ScopedHStr name(i == 0 ? ToWStrTemp(_TRA("None")).s : ToWStrTemp(fmt("%d°", i * 90)).s);
                hr = items->AddItem(itemId.h, name.h);
            }
        }
        if (SUCCEEDED(hr)) {
            int idx = (advanced.extraRotation / 90) % dimofi(kRotationItems);
            SetOptionStr(rotateOption.Get(), kRotationItems[idx]);
        }
        if (FAILED(hr)) {
            return hr;
        }

        hr = ShowAdvancedOptions(options);
        if (FAILED(hr)) {
            return hr;
        }

        PrintDocumentSource* src = source.Get();
        auto handler = Callback<OptionChangedHandler>(
            [src](OptDetails::IPrintTaskOptionDetails*, OptDetails::IPrintTaskOptionChangedEventArgs*) -> HRESULT {
                src->InvalidatePreview();
                return S_OK;
            });
        if (!handler) {
            return E_OUTOFMEMORY;
        }
        optionDetails = details;
        return details->add_OptionChanged(handler.Get(), &optionToken);
    }

    // adds our options, plus the printer's paper tray, to what the dialog shows
    HRESULT ShowAdvancedOptions(Printing::IPrintTaskOptionsCore* options) {
        ComPtr<Printing::IPrintTaskOptionsCoreUIConfiguration> config;
        HRESULT hr = options->QueryInterface(IID_PPV_ARGS(&config));
        ComPtr<__FIVector_1_HSTRING> displayed;
        if (SUCCEEDED(hr)) {
            hr = config->get_DisplayedOptions(&displayed);
        }
        if (FAILED(hr)) {
            return hr;
        }

        ComPtr<Printing::IStandardPrintTaskOptionsStatic> standard;
        HRESULT stdHr = GetActivationFactory(RuntimeClass_Windows_Graphics_Printing_StandardPrintTaskOptions, standard);
        if (SUCCEEDED(stdHr)) {
            HSTRING inputBin = nullptr;
            if (SUCCEEDED(standard->get_InputBin(&inputBin)) && inputBin) {
                displayed->Append(inputBin);
                gWinRt.windowsDeleteString(inputBin);
            }
        }
        ScopedHStr center(kOptCenterHorizontally);
        displayed->Append(center.h);
        ScopedHStr rotate(kOptExtraRotation);
        displayed->Append(rotate.h);
        return S_OK;
    }

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
            hr = gWinRt.windowsCreateString(jobTitle.s, (UINT32)len(jobTitle), &title);
        }
        ComPtr<Printing::IPrintTask> task;
        if (SUCCEEDED(hr)) {
            hr = request->CreatePrintTask(title, sourceHandler.Get(), &task);
        }
        if (title) {
            gWinRt.windowsDeleteString(title);
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
        if (SUCCEEDED(hr)) {
            // losing these costs the extra options, not the print job
            HRESULT optHr = AddAdvancedOptions(options.Get());
            if (FAILED(optHr)) {
                logf("Win11 print: advanced options unavailable: 0x%08x\n", (uint)optHr);
            }
        }
        ComPtr<Printing::IPrintTask2> task2;
        if (SUCCEEDED(hr) && SUCCEEDED(task.As(&task2))) {
            task2->put_IsPreviewEnabled(true);
        }
        return hr;
    }

  public:
    // Only our own references go away here. The WinRT print pipeline keeps its
    // own on the document source, so this is safe even while a job is still
    // spooling -- as long as we don't tear WinRT down under it, which is why
    // EnsureWinRt() initializes once per process and never uninitializes.
    ~Win11PrintSession() {
        if (manager && token.value) {
            manager->remove_PrintTaskRequested(token);
        }
        if (optionDetails && optionToken.value) {
            optionDetails->remove_OptionChanged(optionToken);
        }
        wstr::Free(jobTitle);
    }

    HRESULT Initialize(HWND hwnd, EngineBase* engine, int currentPage, PrintScaleAdv scale, float previewDpi) {
        HRESULT hr = EnsureWinRt();
        if (FAILED(hr)) {
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

        hr = GetActivationFactory(RuntimeClass_Windows_Graphics_Printing_PrintManager, interop);
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

// The modern print dialog with the preview pane is Windows 11 (build 22000+).
// Windows 10 shows the old UWP print flyout for the same API, which is a step
// down from PrintDlgEx, so everything before 11 keeps the classic dialog.
static bool IsWin11OrGreater() {
    OSVERSIONINFOEX ver{};
    if (!GetOsVersion(ver)) {
        return false;
    }
    if (ver.dwMajorVersion != 10) {
        return ver.dwMajorVersion > 10;
    }
    return ver.dwBuildNumber >= kWin11Build;
}

bool TryPrintCurrentFileWin11(MainWindow* win, PrintScaleAdv defaultScale) {
    if (!IsWin11OrGreater()) {
        return false;
    }
    if (!win || !win->hwndFrame || !win->AsFixed() || !win->CurrentTab()) {
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

void ShutdownWin11Printing() {
    delete gPrintSession;
    gPrintSession = nullptr;
}

#else

#include "PrintWin11.h"

bool TryPrintCurrentFileWin11(MainWindow*, PrintScaleAdv) {
    return false;
}

void ShutdownWin11Printing() {}

#endif
