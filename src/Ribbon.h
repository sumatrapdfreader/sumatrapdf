/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Ribbon_h
#define Ribbon_h

#include "UIRibbon.h"
#include "Scopes.h"

class WindowInfo;

class RibbonSupport : public IUIApplication, IUICommandHandler {
    ScopedComPtr<IUIFramework> driver;
    ScopedComQIPtr<IUIRibbon> ribbon;
    WindowInfo *win;

public:
    UINT32 ribbonDy;

    RibbonSupport(WindowInfo *win) : win(win), ribbonDy(0) { }
    ~RibbonSupport() { if (driver) driver->Destroy(); }

    bool Initialize(HINSTANCE hInst, const WCHAR *resourceName);
    bool SetState(const char *state);
    char *GetState();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) {
        if (!ppv)
            return E_INVALIDARG;
        if (riid == __uuidof(IUIApplication)) {
            *ppv = static_cast<IUIApplication *>(this);
            AddRef();
            return S_OK;
        }
        if (riid == __uuidof(IUICommandHandler)) {
            *ppv = static_cast<IUICommandHandler *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    // dummy implementations, owned by WindowInfo (for now)
    IFACEMETHODIMP_(ULONG) AddRef() { return 2; }
    IFACEMETHODIMP_(ULONG) Release() { return 1; }

    // IUIApplication
    STDMETHODIMP OnViewChanged(UINT32 viewId, UI_VIEWTYPE typeID, IUnknown *view, UI_VIEWVERB verb, INT32 uReasonCode);
    STDMETHODIMP OnCreateUICommand(UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler **commandHandler) {
        return QueryInterface(IID_PPV_ARGS(commandHandler));
    }
    STDMETHODIMP OnDestroyUICommand(UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler *commandHandler) {
        return S_OK;
    }

    // IUICommandHandler
    STDMETHODIMP Execute(UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties);
    STDMETHODIMP UpdateProperty(UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue) {
        return E_NOTIMPL;
    }
};

#endif // Ribbon_h
