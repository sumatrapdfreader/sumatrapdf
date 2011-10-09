// code copied from the Windows 7 SDK (to allow building in pre-Windows 7 SDKs)

#if !defined(UIRibbon_h) && !defined(__UIRibbon_h__)
#define UIRibbon_h

#include "BaseUtil.h"
#include <propkeydef.h>

// copied from UIRibbon.h

typedef enum UI_COMMANDTYPE {
    UI_COMMANDTYPE_UNKNOWN = 0, UI_COMMANDTYPE_GROUP, UI_COMMANDTYPE_ACTION,
    UI_COMMANDTYPE_ANCHOR, UI_COMMANDTYPE_CONTEXT, UI_COMMANDTYPE_COLLECTION,
    UI_COMMANDTYPE_COMMANDCOLLECTION, UI_COMMANDTYPE_DECIMAL, UI_COMMANDTYPE_BOOLEAN,
    UI_COMMANDTYPE_FONT, UI_COMMANDTYPE_RECENTITEMS, UI_COMMANDTYPE_COLORANCHOR,
    UI_COMMANDTYPE_COLORCOLLECTION
} UI_COMMANDTYPE;
typedef enum UI_VIEWTYPE { UI_VIEWTYPE_RIBBON = 1 } UI_VIEWTYPE;
typedef enum UI_VIEWVERB {
    UI_VIEWVERB_CREATE = 0, UI_VIEWVERB_DESTROY, UI_VIEWVERB_SIZE, UI_VIEWVERB_ERROR
} UI_VIEWVERB;
typedef enum UI_EXECUTIONVERB {
    UI_EXECUTIONVERB_EXECUTE = 0, UI_EXECUTIONVERB_PREVIEW,
    UI_EXECUTIONVERB_CANCELPREVIEW
} UI_EXECUTIONVERB;
typedef enum UI_INVALIDATIONS {
    UI_INVALIDATIONS_STATE = 0x1, UI_INVALIDATIONS_VALUE = 0x2,
    UI_INVALIDATIONS_PROPERTY = 0x4, UI_INVALIDATIONS_ALLPROPERTIES = 0x8
} UI_INVALIDATIONS;

MIDL_INTERFACE("C205BB48-5B1C-4219-A106-15BD0A5F24E2") IUISimplePropertySet : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE GetValue(REFPROPERTYKEY key, PROPVARIANT *value) = 0;
};

MIDL_INTERFACE("75AE0A2D-DC03-4C9F-8883-069660D0BEB6") IUICommandHandler : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE Execute(UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties) = 0;
    virtual HRESULT STDMETHODCALLTYPE UpdateProperty(UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue) = 0;
};

MIDL_INTERFACE("D428903C-729A-491D-910D-682A08FF2522") IUIApplication : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE OnViewChanged(UINT32 viewId, UI_VIEWTYPE typeID, IUnknown *view, UI_VIEWVERB verb, INT32 uReasonCode) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnCreateUICommand(UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler **commandHandler) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnDestroyUICommand(UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler *commandHandler) = 0;
};

MIDL_INTERFACE("F4F0385D-6872-43A8-AD09-4C339CB3F5C5") IUIFramework : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE Initialize(HWND frameWnd, IUIApplication *application) = 0;
    virtual HRESULT STDMETHODCALLTYPE Destroy(void) = 0;
    virtual HRESULT STDMETHODCALLTYPE LoadUI(HINSTANCE instance, LPCWSTR resourceName) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetView(UINT32 viewId, REFIID riid, void **ppv) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetUICommandProperty(UINT32 commandId, REFPROPERTYKEY key, PROPVARIANT *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetUICommandProperty(UINT32 commandId, REFPROPERTYKEY key, REFPROPVARIANT value) = 0;
    virtual HRESULT STDMETHODCALLTYPE InvalidateUICommand(UINT32 commandId, UI_INVALIDATIONS flags, const PROPERTYKEY *key) = 0;
    virtual HRESULT STDMETHODCALLTYPE FlushPendingInvalidations(void) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetModes(INT32 iModes) = 0;
};

MIDL_INTERFACE("803982AB-370A-4F7E-A9E7-8784036A6E26") IUIRibbon : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE GetHeight(UINT32 *cy) = 0;
    virtual HRESULT STDMETHODCALLTYPE LoadSettingsFromStream(IStream *pStream) = 0;
    virtual HRESULT STDMETHODCALLTYPE SaveSettingsToStream(IStream *pStream) = 0;
};

MIDL_INTERFACE("DF4F45BF-6F9D-4DD7-9D68-D8F9CD18C4DB") IUICollection : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE GetCount(UINT32 *count) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetItem(UINT32 index, IUnknown **item) = 0;
    virtual HRESULT STDMETHODCALLTYPE Add(IUnknown *item) = 0;
    virtual HRESULT STDMETHODCALLTYPE Insert(UINT32 index, IUnknown *item) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveAt(UINT32 index) = 0;
    virtual HRESULT STDMETHODCALLTYPE Replace(UINT32 indexReplaced, IUnknown *itemReplaceWith) = 0;
    virtual HRESULT STDMETHODCALLTYPE Clear(void) = 0;
};

#define UI_COLLECTION_INVALIDINDEX 0xFFFFFFFF

// from UIRibbonKeydef.h

#define DEFINE_UIPROPERTYKEY(name, type, index) extern "C" const PROPERTYKEY DECLSPEC_SELECTANY name = { { 0x00000000 + index, 0x7363, 0x696e, { 0x84, 0x41, 0x79, 0x8a, 0xcf, 0x5a, 0xeb, 0xb7 } }, type }

#endif // UIRibbon_h
