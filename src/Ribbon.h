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
    LONG lRef;

public:
    UINT32 ribbonDy;

    RibbonSupport(WindowInfo *win) : win(win), ribbonDy(0), lRef(1) { }
    ~RibbonSupport() {
        if (driver)
            driver->Destroy();
        assert(1 == lRef);
    }

    bool Initialize(HINSTANCE hInst, const WCHAR *resourceName);
    bool SetState(const char *state);
    char *GetState();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) {
        if (!ppv)
            return E_INVALIDARG;
        if (riid == __uuidof(IUIApplication))
            *ppv = static_cast<IUIApplication *>(this);
        else if (riid == __uuidof(IUICommandHandler))
            *ppv = static_cast<IUICommandHandler *>(this);
        else
            *ppv = NULL;
        if (!*ppv)
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    IFACEMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&lRef); }
    IFACEMETHODIMP_(ULONG) Release() { return InterlockedDecrement(&lRef); }

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
    STDMETHODIMP UpdateProperty(UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue);
};

// per default, collapse the ribbon, add some common commands to the
// quick access toolbar and display the QAT below the ribbon
#define RIBBON_DEFAULT_STATE ((unsigned char *)"<customUI xmlns='http://schemas.microsoft.com/windows/2009/ribbon/qat'><ribbon minimized='true'><qat position='1'><sharedControls><control idQ='431'/><control idQ='430'/></sharedControls></qat></ribbon></customUI>")

#endif // Ribbon_h
