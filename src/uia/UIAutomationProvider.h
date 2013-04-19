/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef UIAutomationProvider_h
#define UIAutomationProvider_h

#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>

const int SUMATRA_UIA_STARTPAGE_RUNTIME_ID = 1;
const int SUMATRA_UIA_DOCUMENT_RUNTIME_ID = 2;

#define SUMATRA_UIA_PAGE_RUNTIME_ID(X) (100 + (X))

class WindowInfo;
class SumatraUIAutomationStartPageProvider;
class SumatraUIAutomationDocumentProvider;

class SumatraUIAutomationProvider : public IRawElementProviderSimple, public IRawElementProviderFragment, public IRawElementProviderFragmentRoot {
    ULONG                                   refCount;

    const WindowInfo*                       win;
    HWND                                    canvasHwnd;
    SumatraUIAutomationStartPageProvider*   startpage;
    SumatraUIAutomationDocumentProvider*    document;

public:
    SumatraUIAutomationProvider(const WindowInfo*);
private: //ensure no accidental destruction of this class and bypassing refcounting
    ~SumatraUIAutomationProvider();

public:
    void OnDocumentLoad();
    void OnDocumentUnload();
    void OnSelectionChanged();

    //IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(const IID &,void **);
    ULONG   STDMETHODCALLTYPE AddRef(void);
    ULONG   STDMETHODCALLTYPE Release(void);

    //IRawElementProviderSimple
    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID patternId,IUnknown **pRetVal);
    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId,VARIANT *pRetVal);
    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple **pRetVal);
    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions *pRetVal);

    //IRawElementProviderFragment
    HRESULT STDMETHODCALLTYPE Navigate(enum NavigateDirection direction, IRawElementProviderFragment **pRetVal);
    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY **pRetVal);
    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY **pRetVal);
    HRESULT STDMETHODCALLTYPE SetFocus(void);
    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(struct UiaRect *pRetVal);
    HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot **pRetVal);

    //IRawElementProviderFragmentRoot
    HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(double,double,IRawElementProviderFragment **);
    HRESULT STDMETHODCALLTYPE GetFocus(IRawElementProviderFragment **);

private:
    IRawElementProviderFragment* GetElementFromPoint(double,double,IRawElementProviderFragment *);
};

namespace uia {

LRESULT ReturnRawElementProvider(HWND hwnd, WPARAM wParam, LPARAM lParam, IRawElementProviderSimple *);
HRESULT HostProviderFromHwnd(HWND hwnd, IRawElementProviderSimple ** pProvider);
HRESULT RaiseAutomationEvent(IRawElementProviderSimple * pProvider, EVENTID id);
HRESULT RaiseStructureChangedEvent(IRawElementProviderSimple * pProvider, StructureChangeType structureChangeType, int * pRuntimeId, int cRuntimeIdLen);
HRESULT GetReservedNotSupportedValue(IUnknown **punkNotSupportedValue);

};

#endif //UIAutomationProvider_h
