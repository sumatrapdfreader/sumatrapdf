/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "UIAutomationProvider.h"

#include <UIAutomation.h>
#include <assert.h>

#include "UIAutomationConstants.h"
#include "UIAutomationDocumentProvider.h"
#include "UIAutomationStartPageProvider.h"
#include "WindowInfo.h"

SumatraUIAutomationProvider::SumatraUIAutomationProvider(const WindowInfo* win) :
    refCount(0),
    win(win), canvasHwnd(win->hwndCanvas),
    startpage(NULL), document(NULL)
{
    this->AddRef(); //We are the only copy

    startpage = new SumatraUIAutomationStartPageProvider(canvasHwnd, this);
}
SumatraUIAutomationProvider::~SumatraUIAutomationProvider()
{
    if (document) {
        document->FreeDocument(); // tell that the dm is now invalid
        document->Release(); // release our hooks
        document = NULL;
    }

    startpage->Release();
    startpage = NULL;
}

void SumatraUIAutomationProvider::OnDocumentLoad()
{
    assert(!document);

    document = new SumatraUIAutomationDocumentProvider(canvasHwnd, this);
    document->LoadDocument(win->dm);
    UiaRaiseStructureChangedEvent(this, StructureChangeType_ChildrenInvalidated, NULL, 0);
}
void SumatraUIAutomationProvider::OnDocumentUnload()
{
    if (document) {
        document->FreeDocument(); // tell that the dm is now invalid
        document->Release(); // release our hooks
        document = NULL;
        UiaRaiseStructureChangedEvent(this, StructureChangeType_ChildrenInvalidated, NULL, 0);
    }
}
void SumatraUIAutomationProvider::OnSelectionChanged()
{
    if (document)
        UiaRaiseAutomationEvent(document, UIA_Text_TextSelectionChangedEventId);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::QueryInterface(const IID & iid,void ** ppvObject)
{
    if (ppvObject == NULL)
        return E_POINTER;

    if (iid == IID_IRawElementProviderSimple) {
        *ppvObject = static_cast<IRawElementProviderSimple*>(this);
        this->AddRef(); //New copy has entered the universe
        return S_OK;
    } else if (iid == IID_IRawElementProviderFragment) {
        *ppvObject = static_cast<IRawElementProviderFragment*>(this);
        this->AddRef(); //New copy has entered the universe
        return S_OK;
    } else if (iid == IID_IRawElementProviderFragmentRoot) {
        *ppvObject = static_cast<IRawElementProviderFragmentRoot*>(this);
        this->AddRef(); //New copy has entered the universe
        return S_OK;
    }

    *ppvObject = NULL;
    return E_NOINTERFACE;
}
ULONG STDMETHODCALLTYPE SumatraUIAutomationProvider::AddRef(void)
{
    return ++refCount;
}
ULONG STDMETHODCALLTYPE SumatraUIAutomationProvider::Release(void)
{
    if (--refCount)
        return refCount;

    //Suicide
    delete this;
    return 0;
}


HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetPatternProvider(PATTERNID patternId,IUnknown **pRetVal)
{
    *pRetVal = NULL;
    return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetPropertyValue(PROPERTYID propertyId,VARIANT *pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;

    if (propertyId == UIA_NamePropertyId) {
        pRetVal->vt = VT_BSTR;
        pRetVal->bstrVal = SysAllocString(L"Canvas");
        return S_OK;
    } else if (propertyId == UIA_IsKeyboardFocusablePropertyId) {
        pRetVal->vt = VT_BOOL;
        pRetVal->boolVal = TRUE;
        return S_OK;
    } else if (propertyId == UIA_ControlTypePropertyId) {
        pRetVal->vt = VT_I4;
        pRetVal->lVal = UIA_CustomControlTypeId;
        return S_OK;
    } else if (propertyId == UIA_NativeWindowHandlePropertyId) {
        pRetVal->vt = VT_I4;
        pRetVal->lVal = (LONG)canvasHwnd;
        return S_OK;
    }
    

    pRetVal->vt = VT_EMPTY;
    return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::get_HostRawElementProvider(IRawElementProviderSimple **pRetVal)
{
    return UiaHostProviderFromHwnd(canvasHwnd,pRetVal);
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::get_ProviderOptions(ProviderOptions *pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;
    *pRetVal = ProviderOptions_ServerSideProvider;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::Navigate(enum NavigateDirection direction, IRawElementProviderFragment **pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;
    
    //No siblings, no parent
    if (direction == NavigateDirection_Parent ||
        direction == NavigateDirection_NextSibling ||
        direction == NavigateDirection_PreviousSibling) {
        *pRetVal = NULL;
        return S_OK;
    } else if (direction == NavigateDirection_FirstChild ||
             direction == NavigateDirection_LastChild) {
        // return document content element, or the start page element
        if (document)
            *pRetVal = document;
        else
            *pRetVal = startpage;

        if (*pRetVal)
            (*pRetVal)->AddRef();
        return S_OK;
    } else {
        return E_INVALIDARG;
    }
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetRuntimeId(SAFEARRAY **pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;

    //Top-level elements should return NULL
    *pRetVal = NULL;
    return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetEmbeddedFragmentRoots(SAFEARRAY **pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;

    //No other roots => return NULL
    *pRetVal = NULL;
    return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::SetFocus(void)
{
    //okay
    return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::get_BoundingRectangle(struct UiaRect *pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;

    //Return Bounding Rect of the Canvas area
    RECT canvas_rect;
    GetWindowRect(canvasHwnd, &canvas_rect);

    pRetVal->left   = canvas_rect.left;
    pRetVal->top    = canvas_rect.top;
    pRetVal->width  = canvas_rect.right - canvas_rect.left;
    pRetVal->height = canvas_rect.bottom - canvas_rect.top;

    return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::get_FragmentRoot(IRawElementProviderFragmentRoot **pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;

    *pRetVal = this;
    (*pRetVal)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::ElementProviderFromPoint(double x,double y,IRawElementProviderFragment **pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;

    // traverse the tree
    *pRetVal = this->GetElementFromPoint(x,y,this);
    return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetFocus(IRawElementProviderFragment **pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;

    // whichever child exists in the tree has the focus
    if (document)
        *pRetVal = document;
    else
        *pRetVal = startpage;
    if (*pRetVal)
        (*pRetVal)->AddRef();
    return S_OK;
}
IRawElementProviderFragment* SumatraUIAutomationProvider::GetElementFromPoint(double x,double y,IRawElementProviderFragment * root)
{
    if (!root)
        return NULL;

    //Check the children
    IRawElementProviderFragment* it;
    root->Navigate(NavigateDirection_FirstChild,&it);

    while (it) {
        UiaRect rect;
        it->get_BoundingRectangle(&rect);

        // step into
        if (rect.left <= x && x <= rect.left+rect.width &&
            rect.top <= y && y <= rect.top+rect.height) {
            IRawElementProviderFragment* leaf = GetElementFromPoint(x,y,it);
            it->Release(); //release our copy
            return leaf;
        }
        
        // go to next element, release old one
        IRawElementProviderFragment* old_it = it;
        old_it->Navigate(NavigateDirection_NextSibling,&it);
        old_it->Release();
    }

    //No such child
    root->AddRef();
    return root;
}
