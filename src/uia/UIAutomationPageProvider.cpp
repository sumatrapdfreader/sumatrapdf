#include "BaseUtil.h"
#include "UIAutomationPageProvider.h"

#include <UIAutomation.h>
#include "UIAutomationProvider.h"
#include "UIAutomationDocumentProvider.h"
#include "DisplayModel.h"
#include "TextSelection.h"


SumatraUIAutomationPageProvider::SumatraUIAutomationPageProvider(int page_num,HWND canvas_hwnd, DisplayModel*dm, SumatraUIAutomationDocumentProvider* root)
: m_ref_count(0), m_page_num(page_num),
m_canvas_hwnd(canvas_hwnd), m_dm(dm),
m_root(root), m_sibling_prev(NULL), m_sibling_next(NULL),
m_released(false)
{
	this->AddRef(); //We are the only copy
	//m_root->AddRef(); Don't add refs to our parent & owner. 
}
SumatraUIAutomationPageProvider::~SumatraUIAutomationPageProvider()
{
}
int SumatraUIAutomationPageProvider::GetPageNum() const
{
	return m_page_num;
}
SumatraUIAutomationPageProvider* SumatraUIAutomationPageProvider::GetNextPage()
{
	return m_sibling_next;
}
SumatraUIAutomationPageProvider* SumatraUIAutomationPageProvider::GetPreviousPage()
{
	return m_sibling_prev;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::QueryInterface(const IID &iid,void **ppvObject)
{
	if (ppvObject == NULL)
		return E_POINTER;

	if (iid == IID_IRawElementProviderFragment)
	{
		*ppvObject = static_cast<IRawElementProviderFragment*>(this);
		this->AddRef(); //New copy has entered the universe
		return S_OK;
	}
	else if (iid == IID_IRawElementProviderSimple)
	{
		*ppvObject = static_cast<IRawElementProviderSimple*>(this);
		this->AddRef(); //New copy has entered the universe
		return S_OK;
	}
	else if (iid == IID_IValueProvider)
	{
		*ppvObject = static_cast<IValueProvider*>(this);
		this->AddRef(); //New copy has entered the universe
		return S_OK;
	}

	*ppvObject = NULL;
	return E_NOINTERFACE;
}
ULONG STDMETHODCALLTYPE SumatraUIAutomationPageProvider::AddRef(void)
{
	return ++m_ref_count;
}
ULONG STDMETHODCALLTYPE SumatraUIAutomationPageProvider::Release(void)
{
	if (--m_ref_count)
		return m_ref_count;

	//Suicide
	delete this;
	return 0;
}


HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::Navigate(enum NavigateDirection direction, IRawElementProviderFragment **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	
	// disallow traverse if we are lingering
	if (m_released)
		return E_FAIL;
	
	// siblings
	if (direction == NavigateDirection_PreviousSibling)
	{
		*pRetVal = m_sibling_prev;
		if (*pRetVal)
			(*pRetVal)->AddRef();
		return S_OK;
	}
	else if (direction == NavigateDirection_NextSibling)
	{
		*pRetVal = m_sibling_next;
		if (*pRetVal)
			(*pRetVal)->AddRef();
		return S_OK;
	}
	// no children
	else if (direction == NavigateDirection_FirstChild ||
	         direction == NavigateDirection_LastChild)
	{
	    *pRetVal = NULL;
		return S_OK;
	}
	else if (direction == NavigateDirection_Parent)
	{
		*pRetVal = m_root;
		(*pRetVal)->AddRef();
		return S_OK;
	}
	else
	{
		return E_INVALIDARG;
	}
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::GetRuntimeId(SAFEARRAY **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	SAFEARRAY *psa = SafeArrayCreateVector(VT_I4, 0, 2);
	if (!psa)
		return E_OUTOFMEMORY;
    
	//RuntimeID magic, use hwnd to differentiate providers of different windows
	int rId[] = { (int)m_canvas_hwnd, SUMATRA_UIA_PAGE_RUNTIME_ID(m_page_num) };
	for (LONG i = 0; i < 2; i++)
		SafeArrayPutElement(psa, &i, (void*)&(rId[i]));
    
	*pRetVal = psa;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::GetEmbeddedFragmentRoots(SAFEARRAY **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	//No other roots => return NULL
	*pRetVal = NULL;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::SetFocus(void)
{
	//okay
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::get_BoundingRectangle(struct UiaRect *pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	if (m_released)
		return E_FAIL;

	// some engines might not support GetPageInfo
	const PageInfo* page = m_dm->GetPageInfo(m_page_num);
	if (!page)
		return E_FAIL;

	RECT canvas_rect;
	GetWindowRect(m_canvas_hwnd, &canvas_rect);

	pRetVal->left   = canvas_rect.left + page->pageOnScreen.x;
	pRetVal->top    = canvas_rect.top + page->pageOnScreen.y;
	pRetVal->width  = page->pageOnScreen.dx;
	pRetVal->height = page->pageOnScreen.dy;

	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::get_FragmentRoot(IRawElementProviderFragmentRoot **pRetVal)
{
	if (m_released)
		return E_FAIL;

	//Let our parent to handle this
    return m_root->get_FragmentRoot(pRetVal);
}


HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::GetPatternProvider(PATTERNID patternId,IUnknown **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	if (patternId == UIA_ValuePatternId)
	{
		*pRetVal = static_cast<IValueProvider*>(this);
		this->AddRef(); //New copy has entered the universe
		return S_OK;
	}

	*pRetVal = NULL;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::GetPropertyValue(PROPERTYID propertyId,VARIANT *pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	if (propertyId == UIA_NamePropertyId)
	{
		pRetVal->vt = VT_BSTR;
		pRetVal->bstrVal = SysAllocString(ScopedMem<WCHAR> (str::Format(L"Page %d",m_page_num)));
		return S_OK;
	}
	else if (propertyId == UIA_IsValuePatternAvailablePropertyId)
	{
		pRetVal->vt = VT_BOOL;
		pRetVal->boolVal = TRUE;
		return S_OK;
	}

	pRetVal->vt = VT_EMPTY;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::get_HostRawElementProvider(IRawElementProviderSimple **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	*pRetVal = NULL;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::get_ProviderOptions(ProviderOptions *pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	*pRetVal = ProviderOptions_ServerSideProvider;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::SetValue(LPCWSTR val)
{
	return E_ACCESSDENIED;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::get_Value(BSTR *pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	if (m_released)
		return E_FAIL;

	const WCHAR * page_content = m_dm->textCache->GetData(m_page_num);
	if (!page_content)
	{
		*pRetVal = NULL;
		return S_OK;
	}

	*pRetVal = SysAllocString(page_content);
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::get_IsReadOnly(BOOL *pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	*pRetVal = TRUE;
	return S_OK;
}
