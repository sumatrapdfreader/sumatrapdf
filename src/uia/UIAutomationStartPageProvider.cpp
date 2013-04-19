#include "BaseUtil.h"
#include "UIAutomationStartPageProvider.h"

#include <UIAutomation.h>

#include "UIAutomationConstants.h"

SumatraUIAutomationStartPageProvider::SumatraUIAutomationStartPageProvider(HWND canvas_hwnd, SumatraUIAutomationProvider* root)
: m_ref_count(0), m_canvas_hwnd(canvas_hwnd), m_root(root)
{
	this->AddRef(); //We are the only copy
	//m_root->AddRef(); Don't add refs to our parent & owner. 
}
SumatraUIAutomationStartPageProvider::~SumatraUIAutomationStartPageProvider()
{
}
	
//IUnknown
HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::QueryInterface(const IID &iid,void **ppvObject)
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

	*ppvObject = NULL;
	return E_NOINTERFACE;
}
ULONG STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::AddRef(void)
{
	return ++m_ref_count;
}
ULONG STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::Release(void)
{
	if (--m_ref_count)
		return m_ref_count;

	//Suicide
	delete this;
	return 0;
}


HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::Navigate(enum NavigateDirection direction, IRawElementProviderFragment **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	
	//No siblings, no children
	if (direction == NavigateDirection_NextSibling ||
	    direction == NavigateDirection_PreviousSibling ||
		direction == NavigateDirection_FirstChild ||
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
HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::GetRuntimeId(SAFEARRAY **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	SAFEARRAY *psa = SafeArrayCreateVector(VT_I4, 0, 2);
	if (!psa)
		return E_OUTOFMEMORY;
    
	//RuntimeID magic, use hwnd to differentiate providers of different windows
	int rId[] = { (int)m_canvas_hwnd, SUMATRA_UIA_STARTPAGE_RUNTIME_ID };
	for (LONG i = 0; i < 2; i++)
		SafeArrayPutElement(psa, &i, (void*)&(rId[i]));
    
	*pRetVal = psa;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::GetEmbeddedFragmentRoots(SAFEARRAY **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	//No other roots => return NULL
	*pRetVal = NULL;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::SetFocus(void)
{
	//okay
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::get_BoundingRectangle(struct UiaRect *pRetVal)
{
	//Share area with the canvas uia provider
	return m_root->get_BoundingRectangle(pRetVal);
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::get_FragmentRoot(IRawElementProviderFragmentRoot **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	//Return the root node
	*pRetVal = m_root;
	m_root->AddRef();
    return S_OK;
}


HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::GetPatternProvider(PATTERNID patternId,IUnknown **pRetVal)
{
	*pRetVal = NULL;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::GetPropertyValue(PROPERTYID propertyId,VARIANT *pRetVal)
{
	if (propertyId == UIA_NamePropertyId)
	{
		pRetVal->vt = VT_BSTR;
		pRetVal->bstrVal = SysAllocString(L"Start Page");
		return S_OK;
	}

	pRetVal->vt = VT_EMPTY;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::get_HostRawElementProvider(IRawElementProviderSimple **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	*pRetVal = NULL;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::get_ProviderOptions(ProviderOptions *pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	*pRetVal = ProviderOptions_ServerSideProvider;
	return S_OK;
}
