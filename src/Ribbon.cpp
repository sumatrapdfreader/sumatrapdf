/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "StrUtil.h"
#include "WinUtil.h"
#include "FileUtil.h"

#include "WindowInfo.h"
#include "Selection.h"
#include "Menu.h"
#include "Favorites.h"
#include "FileHistory.h"
#include "translations.h"
#include "Ribbon.h"

#include "resource.h"
#include "ribbon\ribbon-gen.h"

#ifndef __UIRibbon_h__

#define DEFINE_GUID_STATIC(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
DEFINE_GUID_STATIC(CLSID_UIRibbonFramework, 0x926749FA, 0x2615, 0x4987, 0x88, 0x45, 0xC3, 0x3E, 0x65, 0xF2, 0xB9, 0x57); 

DEFINE_UIPROPERTYKEY(UI_PKEY_Enabled,               VT_BOOL,    1);
DEFINE_UIPROPERTYKEY(UI_PKEY_Keytip,                VT_LPWSTR,  3);
DEFINE_UIPROPERTYKEY(UI_PKEY_Label,                 VT_LPWSTR,  4);
DEFINE_UIPROPERTYKEY(UI_PKEY_TooltipDescription,    VT_LPWSTR,  5);
DEFINE_UIPROPERTYKEY(UI_PKEY_TooltipTitle,          VT_LPWSTR,  6);
DEFINE_UIPROPERTYKEY(UI_PKEY_ItemsSource,           VT_UNKNOWN, 101);
DEFINE_UIPROPERTYKEY(UI_PKEY_CategoryId,            VT_UI4,     103);
DEFINE_UIPROPERTYKEY(UI_PKEY_SelectedItem,          VT_UI4,     104);
DEFINE_UIPROPERTYKEY(UI_PKEY_BooleanValue,          VT_BOOL,    200);
DEFINE_UIPROPERTYKEY(UI_PKEY_StringValue,           VT_LPWSTR,  202);
DEFINE_UIPROPERTYKEY(UI_PKEY_RecentItems,           VT_ARRAY|VT_UNKNOWN,    350);
DEFINE_UIPROPERTYKEY(UI_PKEY_Pinned,                VT_BOOL,    351);
DEFINE_UIPROPERTYKEY(UI_PKEY_Viewable,              VT_BOOL,    1000);
DEFINE_UIPROPERTYKEY(UI_PKEY_Minimized,             VT_BOOL,    1001);

#endif // __UIRibbon_h__

enum AppModes {
    MODE_ALWAYS = 1 << 0,
    MODE_REQ_DISK = 1 << 1,
    MODE_REQ_PRINTER = 1 << 2,
    MODE_REQ_COPYSEL = 1 << 3,
    MODE_HAS_TEXT = 1 << 4,
    MODE_DEBUG = 1 << 16
};

static HRESULT InitPropString(PROPVARIANT *var, const TCHAR *value);

bool RibbonSupport::Initialize(HINSTANCE hInst, const WCHAR *resourceName)
{
    CoCreateInstance(CLSID_UIRibbonFramework, NULL, CLSCTX_ALL,
                     __uuidof(IUIFramework), (void **)&driver);
    if (!driver)
        return false;

    modes = MODE_ALWAYS
        | (HasPermission(Perm_DiskAccess) ? MODE_REQ_DISK : 0)
        | (HasPermission(Perm_PrinterAccess) ? MODE_REQ_PRINTER : 0)
        | (HasPermission(Perm_CopySelection) ? MODE_REQ_COPYSEL : 0)
        | MODE_HAS_TEXT
#ifdef SHOW_DEBUG_MENU_ITEMS
        | MODE_DEBUG
#endif
        ;

    HRESULT hr = driver->Initialize(win->hwndFrame, static_cast<IUIApplication *>(this));
    if (FAILED(hr)) {
        driver = NULL;
        return false;
    }
    hr = driver->LoadUI(hInst, resourceName);
    if (SUCCEEDED(hr))
        driver->SetModes(modes);
    return SUCCEEDED(hr);
}

inline PROPVARIANT MakeBoolVariant(bool value)
{
    PROPVARIANT var = { 0 };
    var.vt = VT_BOOL;
    var.boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
    return var;
}

typedef HRESULT (WINAPI * DwmExtendFrameIntoClientAreaPtr)(HWND hwnd, const RECT *margins);

static void DisableClientAreaTransparency(HWND hwnd)
{
    static bool initialized = false;
    static DwmExtendFrameIntoClientAreaPtr SetTransparency;

    if (!initialized) {
        SetTransparency = (DwmExtendFrameIntoClientAreaPtr)LoadDllFunc(_T("Dwmapi.dll"), "DwmExtendFrameIntoClientArea");
        initialized = true;
    }
    if (SetTransparency)
        SetTransparency(hwnd, &RectI().ToRECT());
}

void RibbonSupport::SetVisibility(bool show)
{
    assert(ribbon);
    if (!ribbon) return;
    ScopedComQIPtr<IPropertyStore> store(ribbon);
    if (!store) return;

    store->SetValue(UI_PKEY_Viewable, MakeBoolVariant(show));
    store->Commit();

    if (!show) {
        DisableClientAreaTransparency(win->hwndFrame);
        win->RepaintAsync(10);
    }
}

void RibbonSupport::SetMinimized(bool expanded)
{
    assert(ribbon);
    if (!ribbon) return;
    ScopedComQIPtr<IPropertyStore> store(ribbon);
    if (!store) return;

    store->SetValue(UI_PKEY_Minimized, MakeBoolVariant(expanded));
    store->Commit();
}

void RibbonSupport::Reset()
{
    for (int i = 0; i < 600; i++) {
        // tab and group names are below 20, item ids between 400 and 600
        if (20 == i)
            i = 400;
        driver->InvalidateUICommand(i, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Label);
        driver->InvalidateUICommand(i, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_TooltipTitle);
        driver->InvalidateUICommand(i, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Keytip);
    }
    driver->FlushPendingInvalidations();
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

void RibbonSupport::UpdateState()
{
    // those menu items will be disabled if no document is opened, enabled otherwise
    static UINT menusToDisableIfNoDocument[] = {
        IDM_VIEW_ROTATE_LEFT, IDM_VIEW_ROTATE_RIGHT, IDM_GOTO_NEXT_PAGE, IDM_GOTO_PREV_PAGE,
        IDM_GOTO_FIRST_PAGE, IDM_GOTO_LAST_PAGE, IDM_GOTO_NAV_BACK, IDM_GOTO_NAV_FORWARD,
        IDM_GOTO_PAGE, IDM_FIND_FIRST, IDM_SAVEAS, IDM_SAVEAS_BOOKMARK, IDM_SEND_BY_EMAIL,
        IDM_VIEW_WITH_ACROBAT, IDM_VIEW_WITH_FOXIT, IDM_VIEW_WITH_PDF_XCHANGE, 
        IDM_SELECT_ALL, IDM_COPY_SELECTION, IDM_PROPERTIES, IDM_VIEW_PRESENTATION_MODE };

    driver->SetModes(modes & (win->dm && win->dm->engine && win->dm->engine->IsImageCollection() ? ~(MODE_REQ_COPYSEL | MODE_HAS_TEXT) : 0));
    driver->InvalidateUICommand(IDM_GOTO_PAGE, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Label);

#define SetToggleState(id, value) driver->SetUICommandProperty(id, UI_PKEY_BooleanValue, MakeBoolVariant(value))
#define EnableButton(id, value) driver->SetUICommandProperty(id, UI_PKEY_Enabled, MakeBoolVariant(value))

    bool enabled = win->IsDocLoaded();

    DisplayMode displayMode = enabled ? win->dm->displayMode() : gGlobalPrefs.defaultDisplayMode;
    SetToggleState(IDM_VIEW_SINGLE_PAGE, displayModeSingle(displayMode));
    SetToggleState(IDM_VIEW_FACING, displayModeFacing(displayMode));
    SetToggleState(IDM_VIEW_BOOK, displayModeShowCover(displayMode));
    SetToggleState(IDM_VIEW_CONTINUOUS, displayModeContinuous(displayMode));

    float zoom = enabled ? win->dm->ZoomVirtual() : gGlobalPrefs.defaultZoom;
    SetToggleState(IDM_ZOOM_FIT_PAGE, ZOOM_FIT_PAGE == zoom);
    SetToggleState(IDM_ZOOM_ACTUAL_SIZE, ZOOM_ACTUAL_SIZE == zoom);
    SetToggleState(IDM_ZOOM_FIT_WIDTH, ZOOM_FIT_WIDTH == zoom);
    SetToggleState(IDM_ZOOM_FIT_CONTENT, ZOOM_FIT_CONTENT == zoom);

    zoom = enabled ? win->dm->ZoomAbsolute() : zoom > 0 ? zoom : 0;
    ScopedMem<TCHAR> zoomStr(str::Format(_T("%i%%"), (int)zoom));
    PROPVARIANT varZoom;
    InitPropString(&varZoom, zoom ? zoomStr : _T(""));
    driver->SetUICommandProperty(IDM_ZOOM_CUSTOM, UI_PKEY_StringValue, varZoom);
    PropVariantClear(&varZoom);

    EnableButton(IDM_VIEW_BOOKMARKS, enabled && win->dm->HasTocTree());
    SetToggleState(IDM_VIEW_BOOKMARKS, enabled ? win->tocVisible : gGlobalPrefs.tocVisible);
    EnableButton(IDM_FAV_TOGGLE, gFavorites->Count() > 0);
    SetToggleState(IDM_FAV_TOGGLE, gGlobalPrefs.favVisible);

    EnableButton(IDM_CLOSE, IsFileCloseMenuEnabled());
    EnableButton(IDM_PRINT, enabled && win->dm->engine->IsPrintingAllowed());

    if (enabled) {
        EnableButton(IDM_GOTO_NEXT_PAGE, win->dm->CurrentPageNo() < win->dm->PageCount());
        EnableButton(IDM_GOTO_PREV_PAGE, win->dm->CurrentPageNo() > 1);
        EnableButton(IDM_GOTO_NAV_BACK, win->dm->CanNavigate(-1));
        EnableButton(IDM_GOTO_NAV_FORWARD, win->dm->CanNavigate(1));
    }

    for (int i = 0; i < dimof(menusToDisableIfNoDocument); i++)
        EnableButton(menusToDisableIfNoDocument[i], enabled);
    for (int i = IDM_VIEW_LAYOUT_FIRST; i <= IDM_VIEW_LAYOUT_LAST; i++)
        EnableButton(i, enabled);
    for (int i = IDM_ZOOM_FIRST; i <= IDM_ZOOM_LAST; i++)
        EnableButton(i, enabled);

    EnableButton(IDM_CHECK_UPDATE, HasPermission(Perm_InternetAccess));
    EnableButton(IDM_MANUAL, HasPermission(Perm_DiskAccess));

#ifdef SHOW_DEBUG_MENU_ITEMS
    SetToggleState(IDM_DEBUG_SHOW_LINKS, gDebugShowLinks);
    SetToggleState(IDM_DEBUG_GDI_RENDERER, gUseGdiRenderer);
#endif

#undef SetToggleState
#undef EnableButton

    driver->FlushPendingInvalidations();
}

IFACEMETHODIMP RibbonSupport::OnViewChanged(UINT32 viewId, UI_VIEWTYPE typeID, IUnknown *view, UI_VIEWVERB verb, INT32 uReasonCode)
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

static MenuDef menuDefView[] = {
    { _TRN("&Single Page\tCtrl+6"),         IDM_VIEW_SINGLE_PAGE,       0  },
    { _TRN("&Facing\tCtrl+7"),              IDM_VIEW_FACING,            0  },
    { _TRN("&Book View\tCtrl+8"),           IDM_VIEW_BOOK,              0  },
    { _TRN("Show &pages continuously"),     IDM_VIEW_CONTINUOUS,        0  },
    { SEP_ITEM, 0, 0  },
    { _TRN("Rotate &Left\tCtrl+Shift+-"),   IDM_VIEW_ROTATE_LEFT,       0  },
    { _TRN("Rotate &Right\tCtrl+Shift++"),  IDM_VIEW_ROTATE_RIGHT,      0  },
    { SEP_ITEM, 0, 0  },
    { _TRN("Pr&esentation\tCtrl+L"),        IDM_VIEW_PRESENTATION_MODE, 0  },
    { _TRN("F&ullscreen\tCtrl+Shift+L"),    IDM_VIEW_FULLSCREEN,        0  },
    { SEP_ITEM, 0, 0  },
    { _TRN("Book&marks\tF12"),              IDM_VIEW_BOOKMARKS,         0  },
    { _TRN("Show &Toolbar"),                IDM_VIEW_SHOW_HIDE_TOOLBAR, 0  },
    { SEP_ITEM,                             0,                          MF_REQ_ALLOW_COPY },
    { _TRN("Select &All\tCtrl+A"),          IDM_SELECT_ALL,             MF_REQ_ALLOW_COPY },
    { _TRN("&Copy Selection\tCtrl+C"),      IDM_COPY_SELECTION,         MF_REQ_ALLOW_COPY },
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

static MenuDef menuDefZoom[] = {
    { _TRN("Fit &Page\tCtrl+0"),            IDM_ZOOM_FIT_PAGE,          0  },
    { _TRN("&Actual Size\tCtrl+1"),         IDM_ZOOM_ACTUAL_SIZE,       0  },
    { _TRN("Fit &Width\tCtrl+2"),           IDM_ZOOM_FIT_WIDTH,         0  },
    { _TRN("Fit &Content\tCtrl+3"),         IDM_ZOOM_FIT_CONTENT,       0  },
    { _TRN("Custom &Zoom...\tCtrl+Y"),      IDM_ZOOM_CUSTOM,            0  },
    { SEP_ITEM },
    { "6400%",                              IDM_ZOOM_6400,              MF_NO_TRANSLATE  },
    { "3200%",                              IDM_ZOOM_3200,              MF_NO_TRANSLATE  },
    { "1600%",                              IDM_ZOOM_1600,              MF_NO_TRANSLATE  },
    { "800%",                               IDM_ZOOM_800,               MF_NO_TRANSLATE  },
    { "400%",                               IDM_ZOOM_400,               MF_NO_TRANSLATE  },
    { "200%",                               IDM_ZOOM_200,               MF_NO_TRANSLATE  },
    { "150%",                               IDM_ZOOM_150,               MF_NO_TRANSLATE  },
    { "125%",                               IDM_ZOOM_125,               MF_NO_TRANSLATE  },
    { "100%",                               IDM_ZOOM_100,               MF_NO_TRANSLATE  },
    { "50%",                                IDM_ZOOM_50,                MF_NO_TRANSLATE  },
    { "25%",                                IDM_ZOOM_25,                MF_NO_TRANSLATE  },
    { "12.5%",                              IDM_ZOOM_12_5,              MF_NO_TRANSLATE  },
    { "8.33%",                              IDM_ZOOM_8_33,              MF_NO_TRANSLATE  },
};

static MenuDef menuDefSettings[] = {
    { _TRN("Change Language"),              IDM_CHANGE_LANGUAGE,        0  },
#if 0
    { _TRN("Contribute Translation"),       IDM_CONTRIBUTE_TRANSLATION, MF_REQ_DISK_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
#endif
    { _TRN("&Options..."),                  IDM_SETTINGS,               MF_REQ_PREF_ACCESS },
};

static MenuDef menuDefFavorites[] = {
    { _TRN("Add to favorites"),             IDM_FAV_ADD,                0 },
    { _TRN("Remove from favorites"),        IDM_FAV_DEL,                0 },
    { _TRN("Show Favorites"),               IDM_FAV_TOGGLE,             MF_REQ_DISK_ACCESS },
};

static MenuDef menuDefHelp[] = {
    { _TRN("Visit &Website"),               IDM_VISIT_WEBSITE,          MF_REQ_DISK_ACCESS },
    { _TRN("&Manual"),                      IDM_MANUAL,                 MF_REQ_DISK_ACCESS },
    { _TRN("Check for &Updates"),           IDM_CHECK_UPDATE,           MF_REQ_INET_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("&About"),                       IDM_ABOUT,                  0 },
};

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

HRESULT LabelForMenu(PROPVARIANT *var, const TCHAR *value, bool appMenu=false)
{
    ScopedMem<TCHAR> label(str::Dup(value));
    if (!appMenu)
        str::RemoveChars(label, _T("&"));
    str::TransChars(label, _T("\t"), _T("\0"));
    return InitPropString(var, label);
}

HRESULT TooltipForMenu(PROPVARIANT *var, const TCHAR *value)
{
    const TCHAR *tab = str::FindChar(value, '\t');
    if (!tab)
        return InitPropString(var, value);

    ScopedMem<TCHAR> tip(str::DupN(value, tab - value));
    tip.Set(str::Format(_T("%s (%s)"), tip, tab + 1));
    return InitPropString(var, tip);
}

HRESULT KeytipForMenu(PROPVARIANT *var, const TCHAR *value)
{
    const TCHAR *tip = str::FindChar(value, '&');
    TCHAR tipChar[2] = { tip ? *(tip + 1) : *value, '\0' };
    return InitPropString(var, tipChar);
}

IFACEMETHODIMP RibbonSupport::Execute(UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties)
{
    if (IDM_ZOOM_CUSTOM == commandId && key && IsEqualPropertyKey(*key, UI_PKEY_SelectedItem) && currentValue) {
        float zoom = INVALID_ZOOM;
        switch (currentValue->uintVal) {
        case UI_COLLECTION_INVALIDINDEX:
            if (commandExecutionProperties) {
                PROPVARIANT var;
                commandExecutionProperties->GetValue(UI_PKEY_Label, &var);
                if ((var.vt == VT_LPWSTR || var.vt == VT_BSTR) && var.pwszVal)
                    zoom = (float)_wtof(var.pwszVal);
                PropVariantClear(&var);
                if (zoom > 0)
                    break;
            }
            return E_INVALIDARG;
        default:
            if (currentValue->uintVal >= 13)
                return E_INVALIDARG;
            zoom = (float)atof(menuDefZoom[6 + currentValue->uintVal].title);
            break;
        }
        ZoomToSelection(win, zoom, false);
        return S_OK;
    }

    if (cmdMRUList == commandId) {
        if (!currentValue || currentValue->vt != VT_UI4 || currentValue->intVal > FILE_HISTORY_MAX_RECENT)
            return E_INVALIDARG;
        commandId = IDM_FILE_HISTORY_FIRST + currentValue->intVal;
    }

    PostMessage(win->hwndFrame, WM_COMMAND, commandId, 0);
    return S_OK;
}

IFACEMETHODIMP RibbonSupport::UpdateProperty(UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
{
    if (IsEqualPropertyKey(key, UI_PKEY_Keytip)) {
        switch (commandId) {
        case tabStart: return InitPropString(newValue, _T("S"));
        case tabMore: return InitPropString(newValue, _T("M"));
#define KEYTIP(id, menudef) case id: return KeytipForMenu(newValue, _TR(menudef.title))
        KEYTIP(IDM_VIEW_SINGLE_PAGE, menuDefView[0]);
        KEYTIP(IDM_VIEW_FACING, menuDefView[1]);
        KEYTIP(IDM_VIEW_BOOK, menuDefView[2]);
        KEYTIP(IDM_VIEW_CONTINUOUS, menuDefView[3]);
        KEYTIP(IDM_VIEW_ROTATE_LEFT, menuDefView[5]);
        KEYTIP(IDM_VIEW_ROTATE_RIGHT, menuDefView[6]);
        KEYTIP(IDM_VIEW_PRESENTATION_MODE, menuDefView[8]);
        KEYTIP(IDM_VIEW_FULLSCREEN, menuDefView[9]);
        KEYTIP(IDM_VIEW_BOOKMARKS, menuDefView[11]);
        KEYTIP(IDM_SELECT_ALL, menuDefView[14]);
        KEYTIP(IDM_COPY_SELECTION, menuDefView[15]);
        KEYTIP(IDM_GOTO_NEXT_PAGE, menuDefGoTo[0]);
        KEYTIP(IDM_GOTO_PREV_PAGE, menuDefGoTo[1]);
        KEYTIP(IDM_GOTO_FIRST_PAGE, menuDefGoTo[2]);
        KEYTIP(IDM_GOTO_LAST_PAGE, menuDefGoTo[3]);
        KEYTIP(IDM_GOTO_PAGE, menuDefGoTo[4]);
        KEYTIP(IDM_GOTO_NAV_BACK, menuDefGoTo[6]);
        KEYTIP(IDM_GOTO_NAV_FORWARD, menuDefGoTo[7]);
        KEYTIP(IDM_FIND_FIRST, menuDefGoTo[9]);
        KEYTIP(IDM_ZOOM_FIT_PAGE, menuDefZoom[0]);
        KEYTIP(IDM_ZOOM_ACTUAL_SIZE, menuDefZoom[1]);
        KEYTIP(IDM_ZOOM_FIT_WIDTH, menuDefZoom[2]);
        KEYTIP(IDM_ZOOM_FIT_CONTENT, menuDefZoom[3]);
        KEYTIP(IDM_ZOOM_CUSTOM, menuDefZoom[4]);
        KEYTIP(IDM_CHANGE_LANGUAGE, menuDefSettings[0]);
        KEYTIP(IDM_SETTINGS, menuDefSettings[1]);
        KEYTIP(IDM_FAV_TOGGLE, menuDefFavorites[2]);
        KEYTIP(IDM_MANUAL, menuDefHelp[1]);
        KEYTIP(IDM_CHECK_UPDATE, menuDefHelp[2]);
        KEYTIP(IDM_ABOUT, menuDefHelp[4]);
#undef KEYTIP
        }
    }
    else if (IsEqualPropertyKey(key, UI_PKEY_Label)) {
        switch (commandId) {
#define LABEL(id, menudef) case id: return LabelForMenu(newValue, _TR(menudef.title), true)
        case cmdMRUList: return InitPropString(newValue, _T("Most recently used documents"));
        LABEL(IDM_OPEN, menuDefFile[0]);
        LABEL(IDM_CLOSE, menuDefFile[1]);
        LABEL(IDM_SAVEAS, menuDefFile[2]);
        LABEL(IDM_PRINT, menuDefFile[3]);
        LABEL(IDM_EXIT, menuDefFile[13]);
        LABEL(IDM_SAVEAS_BOOKMARK, menuDefFile[5]);
        LABEL(IDM_SEND_BY_EMAIL, menuDefFile[9]);
        LABEL(IDM_PROPERTIES, menuDefFile[11]);
#undef LABEL
#define LABEL(id, menudef) case id: return LabelForMenu(newValue, _TR(menudef.title))
        case tabStart: return InitPropString(newValue, _T("Start"));
        case tabMore: return InitPropString(newValue, _T("More"));
        case grpView: return InitPropString(newValue, _T("View Mode"));
        LABEL(IDM_VIEW_SINGLE_PAGE, menuDefView[0]);
        LABEL(IDM_VIEW_FACING, menuDefView[1]);
        LABEL(IDM_VIEW_BOOK, menuDefView[2]);
        LABEL(IDM_VIEW_CONTINUOUS, menuDefView[3]);
        case grpView2: return InitPropString(newValue, _T("View"));
        LABEL(IDM_VIEW_ROTATE_LEFT, menuDefView[5]);
        LABEL(IDM_VIEW_ROTATE_RIGHT, menuDefView[6]);
        LABEL(IDM_VIEW_PRESENTATION_MODE, menuDefView[8]);
        LABEL(IDM_VIEW_FULLSCREEN, menuDefView[9]);
        LABEL(IDM_VIEW_BOOKMARKS, menuDefView[11]);
        case grpEdit: return InitPropString(newValue, _T("Edit"));
        LABEL(IDM_SELECT_ALL, menuDefView[14]);
        LABEL(IDM_COPY_SELECTION, menuDefView[15]);
        case grpNav: return InitPropString(newValue, _T("Navigation"));
        LABEL(IDM_GOTO_NEXT_PAGE, menuDefGoTo[0]);
        LABEL(IDM_GOTO_PREV_PAGE, menuDefGoTo[1]);
        LABEL(IDM_GOTO_FIRST_PAGE, menuDefGoTo[2]);
        LABEL(IDM_GOTO_LAST_PAGE, menuDefGoTo[3]);
        case IDM_GOTO_PAGE:
            if (win->IsDocLoaded()) {
                int pageNo = win->dm->CurrentPageNo();
                ScopedMem<TCHAR> pageInfo(str::Format(_T("%s %d / %d"), _TR("Page:"), pageNo, win->dm->PageCount()));
                if (win->dm->engine && win->dm->engine->HasPageLabels()) {
                    ScopedMem<TCHAR> label(win->dm->engine->GetPageLabel(pageNo));
                    pageInfo.Set(str::Format(_T("%s %s (%d / %d)"), _TR("Page:"), label, pageNo, win->dm->PageCount()));
                }
                return InitPropString(newValue, pageInfo);
            }
            return LabelForMenu(newValue, _TR(menuDefGoTo[4].title));
        LABEL(IDM_GOTO_NAV_BACK, menuDefGoTo[6]);
        LABEL(IDM_GOTO_NAV_FORWARD, menuDefGoTo[7]);
        case grpFind: return InitPropString(newValue, _T("Find"));
        LABEL(IDM_FIND_FIRST, menuDefGoTo[9]);
        case grpZoom: return InitPropString(newValue, _T("Zoom"));
        LABEL(IDM_ZOOM_FIT_PAGE, menuDefZoom[0]);
        LABEL(IDM_ZOOM_ACTUAL_SIZE, menuDefZoom[1]);
        LABEL(IDM_ZOOM_FIT_WIDTH, menuDefZoom[2]);
        LABEL(IDM_ZOOM_FIT_CONTENT, menuDefZoom[3]);
        case IDM_ZOOM_CUSTOM: return InitPropString(newValue, _T("Zoom"));
        case grpOptions: return InitPropString(newValue, _T("Options"));
        LABEL(IDM_CHANGE_LANGUAGE, menuDefSettings[0]);
        LABEL(IDM_SETTINGS, menuDefSettings[1]);
        LABEL(IDM_FAV_TOGGLE, menuDefFavorites[2]);
        LABEL(IDM_MANUAL, menuDefHelp[1]);
        LABEL(IDM_CHECK_UPDATE, menuDefHelp[2]);
        LABEL(IDM_ABOUT, menuDefHelp[4]);
        case grpDebug: return InitPropString(newValue, _T("Debug"));
        case IDM_DEBUG_SHOW_LINKS: return InitPropString(newValue, _T("Highlight links"));
        case IDM_DEBUG_GDI_RENDERER: return InitPropString(newValue, _T("Toggle PDF/XPS renderer"));
#undef LABEL
        }
    }
    else if (IsEqualPropertyKey(key, UI_PKEY_TooltipTitle)) {
        switch (commandId) {
#define TOOLTIP(id, menudef) case id: return TooltipForMenu(newValue, _TR(menudef.title))
        TOOLTIP(IDM_OPEN, menuDefFile[0]);
        TOOLTIP(IDM_CLOSE, menuDefFile[1]);
        TOOLTIP(IDM_SAVEAS, menuDefFile[2]);
        TOOLTIP(IDM_PRINT, menuDefFile[3]);
        TOOLTIP(IDM_EXIT, menuDefFile[13]);
        TOOLTIP(IDM_SAVEAS_BOOKMARK, menuDefFile[5]);
        TOOLTIP(IDM_SEND_BY_EMAIL, menuDefFile[9]);
        TOOLTIP(IDM_PROPERTIES, menuDefFile[11]);
        TOOLTIP(IDM_VIEW_SINGLE_PAGE, menuDefView[0]);
        TOOLTIP(IDM_VIEW_FACING, menuDefView[1]);
        TOOLTIP(IDM_VIEW_BOOK, menuDefView[2]);
        TOOLTIP(IDM_VIEW_CONTINUOUS, menuDefView[3]);
        TOOLTIP(IDM_VIEW_ROTATE_LEFT, menuDefView[5]);
        TOOLTIP(IDM_VIEW_ROTATE_RIGHT, menuDefView[6]);
        TOOLTIP(IDM_VIEW_PRESENTATION_MODE, menuDefView[8]);
        TOOLTIP(IDM_VIEW_FULLSCREEN, menuDefView[9]);
        TOOLTIP(IDM_VIEW_BOOKMARKS, menuDefView[11]);
        TOOLTIP(IDM_SELECT_ALL, menuDefView[14]);
        TOOLTIP(IDM_COPY_SELECTION, menuDefView[15]);
        TOOLTIP(IDM_GOTO_NEXT_PAGE, menuDefGoTo[0]);
        TOOLTIP(IDM_GOTO_PREV_PAGE, menuDefGoTo[1]);
        TOOLTIP(IDM_GOTO_FIRST_PAGE, menuDefGoTo[2]);
        TOOLTIP(IDM_GOTO_LAST_PAGE, menuDefGoTo[3]);
        TOOLTIP(IDM_GOTO_PAGE, menuDefGoTo[4]);
        TOOLTIP(IDM_GOTO_NAV_BACK, menuDefGoTo[6]);
        TOOLTIP(IDM_GOTO_NAV_FORWARD, menuDefGoTo[7]);
        TOOLTIP(IDM_FIND_FIRST, menuDefGoTo[9]);
        TOOLTIP(IDM_ZOOM_FIT_PAGE, menuDefZoom[0]);
        TOOLTIP(IDM_ZOOM_ACTUAL_SIZE, menuDefZoom[1]);
        TOOLTIP(IDM_ZOOM_FIT_WIDTH, menuDefZoom[2]);
        TOOLTIP(IDM_ZOOM_FIT_CONTENT, menuDefZoom[3]);
        TOOLTIP(IDM_ZOOM_CUSTOM, menuDefZoom[4]);
        TOOLTIP(IDM_CHANGE_LANGUAGE, menuDefSettings[0]);
        TOOLTIP(IDM_SETTINGS, menuDefSettings[1]);
        TOOLTIP(IDM_FAV_TOGGLE, menuDefFavorites[2]);
        TOOLTIP(IDM_MANUAL, menuDefHelp[1]);
        TOOLTIP(IDM_CHECK_UPDATE, menuDefHelp[2]);
        TOOLTIP(IDM_ABOUT, menuDefHelp[4]);
        default: return UpdateProperty(commandId, UI_PKEY_Label, currentValue, newValue);
#undef TOOLTIP
        }
    }
    else if (IsEqualPropertyKey(key, UI_PKEY_ItemsSource) && IDM_ZOOM_CUSTOM == commandId) {
        if (!currentValue || !currentValue->punkVal)
            return E_INVALIDARG;
        ScopedComQIPtr<IUICollection> coll(currentValue->punkVal);
        if (!coll)
            return E_INVALIDARG;
        for (int i = 6; i < 19; i++) {
            ScopedMem<TCHAR> label(str::conv::FromAnsi(menuDefZoom[i].title));
            ScopedComPtr<IUISimplePropertySet> item(new LabelPropertySet(label));
            coll->Add(item);
        }
    }
    else if (IsEqualPropertyKey(key, UI_PKEY_RecentItems) && cmdMRUList == commandId && !gFileHistory.IsEmpty()) {
        if (!newValue)
            return E_INVALIDARG;
        SAFEARRAY *sa = SafeArrayCreateVector(VT_UNKNOWN, 0, FILE_HISTORY_MAX_RECENT);
        if (!sa) return E_OUTOFMEMORY;
        for (long i = 0; i < FILE_HISTORY_MAX_RECENT; i++) {
            DisplayState *state = gFileHistory.Get(i);
            if (!state)
                break;
            assert(state->filePath);
            ScopedComPtr<IUnknown> item(new LabelPropertySet(path::GetBaseName(state->filePath)));
            SafeArrayPutElement(sa, &i, item);
        }
        newValue->vt = VT_ARRAY | VT_UNKNOWN;
        newValue->parray = sa;
    }
    return S_OK;
}

LabelPropertySet::LabelPropertySet(const TCHAR *value) :
    lRef(1), label(str::Dup(value)) { }

IFACEMETHODIMP LabelPropertySet::GetValue(const PROPERTYKEY &key, PROPVARIANT *value)
{
    if (IsEqualPropertyKey(key, UI_PKEY_Label))
        return InitPropString(value, label);

    PropVariantInit(value);
    if (IsEqualPropertyKey(key, UI_PKEY_CategoryId) && value) {
        value->vt = VT_UI4;
        value->uintVal = UI_COLLECTION_INVALIDINDEX;
    }
    return S_OK;
}
