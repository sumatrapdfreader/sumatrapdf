/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Ribbon_h
#define Ribbon_h

#include "UIRibbon.h"
#include "Scopes.h"
#include <shlwapi.h>

class WindowInfo;

class RibbonSupport : public IUIApplication, IUICommandHandler {
    ScopedComPtr<IUIFramework> driver;
    ScopedComQIPtr<IUIRibbon> ribbon;
    WindowInfo *win;
    int modes;
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
    void SetVisibility(bool show);
    void SetMinimized(bool expand);
    void UpdateState();
    void Reset();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) {
        static const QITAB qit[] = {
            QITABENT(RibbonSupport, IUIApplication),
            QITABENT(RibbonSupport, IUICommandHandler),
            { 0 }
        };
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&lRef); }
    IFACEMETHODIMP_(ULONG) Release() { return InterlockedDecrement(&lRef); }

    // IUIApplication
    IFACEMETHODIMP OnViewChanged(UINT32 viewId, UI_VIEWTYPE typeID, IUnknown *view, UI_VIEWVERB verb, INT32 uReasonCode);
    IFACEMETHODIMP OnCreateUICommand(UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler **commandHandler) {
        return QueryInterface(IID_PPV_ARGS(commandHandler));
    }
    IFACEMETHODIMP OnDestroyUICommand(UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler *commandHandler) {
        return S_OK;
    }

    // IUICommandHandler
    IFACEMETHODIMP Execute(UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties);
    IFACEMETHODIMP UpdateProperty(UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue);
};

class LabelPropertySet : public IUISimplePropertySet {
    ScopedMem<WCHAR> label;
    LONG lRef;

public:
    LabelPropertySet(const TCHAR *value);
    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) {
        static const QITAB qit[] = { QITABENT(LabelPropertySet, IUISimplePropertySet), { 0 } };
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() {
        return InterlockedIncrement(&lRef);
    }
    IFACEMETHODIMP_(ULONG) Release() {
        LONG ref = InterlockedDecrement(&lRef);
        if (0 == ref)
            delete this;
        return ref;
    }
    // IUISimplePropertySet
    IFACEMETHODIMP GetValue(const PROPERTYKEY &key, PROPVARIANT *value);
};

#endif // Ribbon_h
