/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "StrUtil.h"
#include "WinUtil.h"
#include "Ribbon.h"
#include "WindowInfo.h"

#ifndef __UIRibbon_h__

#define DEFINE_GUID_STATIC(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
DEFINE_GUID_STATIC(CLSID_UIRibbonFramework, 0x926749FA, 0x2615, 0x4987, 0x88, 0x45, 0xC3, 0x3E, 0x65, 0xF2, 0xB9, 0x57); 

#endif // __UIRibbon_h__

bool RibbonSupport::Initialize(HINSTANCE hInst, const WCHAR *resourceName) {
    CoCreateInstance(CLSID_UIRibbonFramework, NULL, CLSCTX_ALL,
                     __uuidof(IUIFramework), (void **)&driver);
    if (!driver)
        return false;

    HRESULT hr = driver->Initialize(win->hwndFrame, static_cast<IUIApplication *>(this));
    if (FAILED(hr)) {
        driver = NULL;
        return false;
    }
    hr = driver->LoadUI(hInst, resourceName);
    return SUCCEEDED(hr);
}

bool RibbonSupport::SetState(const char *state) {
    size_t len = str::Len(state) / 2;
    ScopedMem<unsigned char> binary(SAZA(unsigned char, len + 1));
    str::HexToMem(state, binary, len);
    binary[len] = '\0';
    ScopedComPtr<IStream> stm(CreateStreamFromData(binary, len));
    if (!stm || !ribbon)
        return false;
    return SUCCEEDED(ribbon->LoadSettingsFromStream(stm));
}

char *RibbonSupport::GetState() {
    ScopedComPtr<IStream> stm(CreateStreamFromData(NULL, 0));
    if (!stm || !ribbon)
        return false;
    HRESULT hr = ribbon->SaveSettingsToStream(stm);
    if (FAILED(hr))
        return false;

    void *binary;
    size_t len;
    hr = GetDataFromStream(stm, &binary, &len);
    if (FAILED(hr))
        return false;

    return str::MemToHex((unsigned char *)ScopedMem<void>(binary).Get(), len);
}

STDMETHODIMP RibbonSupport::OnViewChanged(UINT32 viewId, UI_VIEWTYPE typeID, IUnknown *view, UI_VIEWVERB verb, INT32 uReasonCode) {
    switch (verb) {
    case UI_VIEWVERB_CREATE:
        ribbon = view;
        break;
    case UI_VIEWVERB_DESTROY:
        // the ribbon is destroyed together with RibbonSupport
        break;
    case UI_VIEWVERB_SIZE:
        if (ribbon && SUCCEEDED(ribbon->GetHeight(&ribbonDy))) {
            if (!win->fullScreen && !win->presentation) {
                ClientRect rc(win->hwndFrame);
                MoveWindow(win->hwndCanvas, 0, ribbonDy, rc.dx, rc.dy - ribbonDy, TRUE);
            }
        }
        break;
    }
    return S_OK;
}

STDMETHODIMP RibbonSupport::Execute(UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties) {
    SendMessage(win->hwndFrame, WM_COMMAND, commandId, 0);
    return S_OK;
}
