/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "StrUtil.h"
#include "WinUtil.h"
#include "Ribbon.h"
#include "WindowInfo.h"
#include "resource.h"
#include <shlwapi.h>

#ifndef __UIRibbon_h__

#define DEFINE_GUID_STATIC(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
DEFINE_GUID_STATIC(CLSID_UIRibbonFramework, 0x926749FA, 0x2615, 0x4987, 0x88, 0x45, 0xC3, 0x3E, 0x65, 0xF2, 0xB9, 0x57); 

DEFINE_UIPROPERTYKEY(UI_PKEY_Enabled,               VT_BOOL,    1);
DEFINE_UIPROPERTYKEY(UI_PKEY_Keytip,                VT_LPWSTR,  3);
DEFINE_UIPROPERTYKEY(UI_PKEY_Label,                 VT_LPWSTR,  4);
DEFINE_UIPROPERTYKEY(UI_PKEY_TooltipDescription,    VT_LPWSTR,  5);

#endif // __UIRibbon_h__

bool RibbonSupport::Initialize(HINSTANCE hInst, const WCHAR *resourceName)
{
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

bool RibbonSupport::SetState(const char *state)
{
    size_t len = str::Len(state) / 2;
    ScopedMem<unsigned char> binary(SAZA(unsigned char, len + 1));
    str::HexToMem(state, binary, len);
    binary[len] = '\0';
    ScopedComPtr<IStream> stm(CreateStreamFromData(binary, len));
    if (!stm || !ribbon)
        return false;
    return SUCCEEDED(ribbon->LoadSettingsFromStream(stm));
}

char *RibbonSupport::GetState()
{
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

STDMETHODIMP RibbonSupport::OnViewChanged(UINT32 viewId, UI_VIEWTYPE typeID, IUnknown *view, UI_VIEWVERB verb, INT32 uReasonCode)
{
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

STDMETHODIMP RibbonSupport::Execute(UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties)
{
    SendMessage(win->hwndFrame, WM_COMMAND, commandId, 0);
    return S_OK;
}

#include "ribbon\ribbon-en-ids.h"
#include "Menu.h"
#include "translations.h"
#define SEP_ITEM "-----"

static MenuDef menuDefFile[] = {
    { _TRN("&Open\tCtrl+O"),                IDM_OPEN ,                  MF_REQ_DISK_ACCESS },
    { _TRN("&Close\tCtrl+W"),               IDM_CLOSE,                  MF_REQ_DISK_ACCESS },
    { _TRN("&Save As...\tCtrl+S"),          IDM_SAVEAS,                 MF_REQ_DISK_ACCESS },
    { _TRN("&Print...\tCtrl+P"),            IDM_PRINT,                  MF_REQ_PRINTER_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("Save S&hortcut...\tCtrl+Shift+S"), IDM_SAVEAS_BOOKMARK,     MF_REQ_DISK_ACCESS },
    { _TRN("Open in &Adobe Reader"),        IDM_VIEW_WITH_ACROBAT,      MF_REQ_DISK_ACCESS },
    { _TRN("Open in &Foxit Reader"),        IDM_VIEW_WITH_FOXIT,        MF_REQ_DISK_ACCESS },
    { _TRN("Open in PDF-XChange"),          IDM_VIEW_WITH_PDF_XCHANGE,  MF_REQ_DISK_ACCESS },
    { _TRN("Send by &E-mail..."),           IDM_SEND_BY_EMAIL,          MF_REQ_DISK_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("P&roperties\tCtrl+D"),          IDM_PROPERTIES,             0 },
    { SEP_ITEM,                             0,                          0 },
    { _TRN("E&xit\tCtrl+Q"),                IDM_EXIT,                   0 }
};

static MenuDef menuDefGoTo[] = {
    { _TRN("&Next Page\tRight Arrow"),      IDM_GOTO_NEXT_PAGE,         0  },
    { _TRN("&Previous Page\tLeft Arrow"),   IDM_GOTO_PREV_PAGE,         0  },
    { _TRN("&First Page\tHome"),            IDM_GOTO_FIRST_PAGE,        0  },
    { _TRN("&Last Page\tEnd"),              IDM_GOTO_LAST_PAGE,         0  },
    { _TRN("Pa&ge...\tCtrl+G"),             IDM_GOTO_PAGE,              0  },
    { SEP_ITEM,                             0,                          0  },
    { _TRN("&Back\tAlt+Left Arrow"),        IDM_GOTO_NAV_BACK,          0  },
    { _TRN("F&orward\tAlt+Right Arrow"),    IDM_GOTO_NAV_FORWARD,       0  },
    { SEP_ITEM,                             0,                          0  },
    { _TRN("Fin&d...\tCtrl+F"),             IDM_FIND_FIRST,             0  },
};

#define guid_eq(a, b) (memcmp(&(a), &(b), sizeof(a)) == 0)

static HRESULT InitPropString(PROPVARIANT *var, const TCHAR *value)
{
    if (!var)
        return E_INVALIDARG;
#ifdef UNICODE
    HRESULT hr = SHStrDupW(value, &var->pwszVal);
#else
    HRESULT hr = SHStrDupW(ScopedMem<WCHAR>(str::conv::FromAnsi(value)), &var->pwszVal);
#endif
    if (SUCCEEDED(hr))
        var->vt = VT_LPWSTR;
    else
        var->pwszVal = NULL;
    return hr;
}

HRESULT KeytipForMenu(PROPVARIANT *var, const TCHAR *value)
{
    const TCHAR *tip = str::FindChar(value, '&');
    if (!tip)
        E_FAIL;
    TCHAR tipChar[2] = { *(tip + 1), '\0' };
    return InitPropString(var, tipChar);
}

STDMETHODIMP RibbonSupport::UpdateProperty(UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
{
    if (guid_eq(key, UI_PKEY_Keytip)) {
        switch (commandId) {
        case tabStart: return InitPropString(newValue, _T("S"));
        case IDM_GOTO_NEXT_PAGE: return KeytipForMenu(newValue, _TR(menuDefGoTo[0].title));
        case IDM_GOTO_PREV_PAGE: return KeytipForMenu(newValue, _TR(menuDefGoTo[1].title));
        }
    }
    else if (guid_eq(key, UI_PKEY_Label) || guid_eq(key, UI_PKEY_TooltipDescription)) {
        switch (commandId) {
        case cmdMRUList: return InitPropString(newValue, _T("(MRU history goes here)"));
        case tabStart: return InitPropString(newValue, _T("Start"));
        case grpNav: return InitPropString(newValue, _T("Navigation"));
        case IDM_OPEN: return InitPropString(newValue, _TR(menuDefFile[0].title));
        case IDM_CLOSE: return InitPropString(newValue, _TR(menuDefFile[1].title));
        case IDM_SAVEAS: return InitPropString(newValue, _TR(menuDefFile[2].title));
        case IDM_PRINT: return InitPropString(newValue, _TR(menuDefFile[3].title));
        case IDM_EXIT: return InitPropString(newValue, _TR(menuDefFile[13].title));
        case IDM_SAVEAS_BOOKMARK: return InitPropString(newValue, _TR(menuDefFile[5].title));
        case IDM_SEND_BY_EMAIL: return InitPropString(newValue, _TR(menuDefFile[9].title));
        case IDM_PROPERTIES: return InitPropString(newValue, _TR(menuDefFile[11].title));
        case IDM_ABOUT: return InitPropString(newValue, _T("&About"));
        case IDM_GOTO_NEXT_PAGE: return InitPropString(newValue, _TR(menuDefGoTo[0].title));
        case IDM_GOTO_PREV_PAGE: return InitPropString(newValue, _TR(menuDefGoTo[1].title));
        }
    }
    return S_OK;
}
