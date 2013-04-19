#include "BaseUtil.h"
#include "UIAutomationDocumentProvider.h"

#include <UIAutomation.h>
#include "DisplayModel.h"
#include "FileUtil.h"
#include "UIAutomationProvider.h"
#include "UIAutomationPageProvider.h"
#include "UIAutomationTextRange.h"


SumatraUIAutomationDocumentProvider::SumatraUIAutomationDocumentProvider(HWND canvas_hwnd, SumatraUIAutomationProvider* root)
: m_ref_count(0), m_canvas_hwnd(canvas_hwnd), m_root(root), m_released(true), m_child_first(NULL), m_child_last(NULL), m_dm(NULL)
{
	this->AddRef(); //We are the only copy
	//m_root->AddRef(); Don't add refs to our parent & owner. 
}
SumatraUIAutomationDocumentProvider::~SumatraUIAutomationDocumentProvider()
{
	this->FreeDocument();
}

void SumatraUIAutomationDocumentProvider::LoadDocument(DisplayModel* dm)
{
	this->FreeDocument();

	// no mutexes needed, this function is called from thread that created dm

	// create page element for each page
	SumatraUIAutomationPageProvider* prev_page = NULL;
	for (int i=1; i<=dm->PageCount(); ++i)
	{
		SumatraUIAutomationPageProvider* cur_page = new SumatraUIAutomationPageProvider(i, m_canvas_hwnd, dm, this);
		cur_page->m_sibling_prev = prev_page;
		if (prev_page)
			prev_page->m_sibling_next = cur_page;
		prev_page = cur_page;

		if (i == 1)
			m_child_first = cur_page;
	}
	m_child_last = prev_page;

	m_dm = dm;
	m_released = false;
}
void SumatraUIAutomationDocumentProvider::FreeDocument()
{
	// release our refs to the page elements
	if (!m_released)
	{
		m_released = true;
		m_dm = NULL;

		SumatraUIAutomationPageProvider* it = m_child_first;
		while (it)
		{
			SumatraUIAutomationPageProvider* current = it;
			it = it->m_sibling_next;

			current->m_released = true; // disallow DisplayModel access
			current->Release();
		}

		// we have released our refs from these objects
		// we are not allowed to access them anymore
		m_child_first = NULL;
		m_child_last = NULL;
	}
}
bool SumatraUIAutomationDocumentProvider::IsDocumentLoaded() const
{
	return !m_released;
}
DisplayModel* SumatraUIAutomationDocumentProvider::GetDM()
{
	assert(IsDocumentLoaded());
	assert(m_dm);
	return m_dm;
}
SumatraUIAutomationPageProvider* SumatraUIAutomationDocumentProvider::GetFirstPage()
{
	assert(IsDocumentLoaded());
	return m_child_first;
}
SumatraUIAutomationPageProvider* SumatraUIAutomationDocumentProvider::GetLastPage()
{
	assert(IsDocumentLoaded());
	return m_child_last;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::QueryInterface(const IID &iid,void **ppvObject)
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
	else if (iid == IID_ITextProvider)
	{
		*ppvObject = static_cast<ITextProvider*>(this);
		this->AddRef(); //New copy has entered the universe
		return S_OK;
	}
	else if (iid == IID_IAccIdentity)
	{
		*ppvObject = static_cast<IAccIdentity*>(this);
		this->AddRef(); //New copy has entered the universe
		return S_OK;
	}
	
	*ppvObject = NULL;
	return E_NOINTERFACE;
}
ULONG STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::AddRef(void)
{
	return ++m_ref_count;
}
ULONG STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::Release(void)
{
	if (--m_ref_count)
		return m_ref_count;

	//Suicide
	delete this;
	return 0;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::Navigate(enum NavigateDirection direction, IRawElementProviderFragment **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	
	// no siblings
	if (direction == NavigateDirection_NextSibling ||
	    direction == NavigateDirection_PreviousSibling)
	{
	    *pRetVal = NULL;
		return S_OK;
	}
	// but has children
	else if (direction == NavigateDirection_FirstChild ||
	         direction == NavigateDirection_LastChild)
	{
		// don't allow traversion to enter invalid nodes
		if (m_released)
		{
			*pRetVal = NULL;
			return S_OK;
		}

		if (direction == NavigateDirection_FirstChild)
			*pRetVal = m_child_first;
		else
			*pRetVal = m_child_last;
		(*pRetVal)->AddRef();
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
HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::GetRuntimeId(SAFEARRAY **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	SAFEARRAY *psa = SafeArrayCreateVector(VT_I4, 0, 2);
	if (!psa)
		return E_OUTOFMEMORY;
    
	//RuntimeID magic, use hwnd to differentiate providers of different windows
	int rId[] = { (int)m_canvas_hwnd, SUMATRA_UIA_DOCUMENT_RUNTIME_ID };
	for (LONG i = 0; i < 2; i++)
		SafeArrayPutElement(psa, &i, (void*)&(rId[i]));
    
	*pRetVal = psa;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::GetEmbeddedFragmentRoots(SAFEARRAY **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	//No other roots => return NULL
	*pRetVal = NULL;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::SetFocus(void)
{
	//okay
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::get_BoundingRectangle(struct UiaRect *pRetVal)
{
	//Share area with the canvas uia provider
	return m_root->get_BoundingRectangle(pRetVal);
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::get_FragmentRoot(IRawElementProviderFragmentRoot **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	//Return the root node
	*pRetVal = m_root;
	m_root->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::GetPatternProvider(PATTERNID patternId,IUnknown **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	if (patternId == UIA_TextPatternId)
	{
		*pRetVal = static_cast<ITextProvider*>(this);
		this->AddRef(); //New copy has entered the universe
		return S_OK;
	}

	*pRetVal = NULL;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::GetPropertyValue(PROPERTYID propertyId,VARIANT *pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	if (m_released)
		return E_FAIL;

	if (propertyId == UIA_NamePropertyId)
	{
		// typically filename
		pRetVal->vt = VT_BSTR;
		pRetVal->bstrVal = SysAllocString(path::GetBaseName(m_dm->FilePath()));
		return S_OK;
	}
	else if (propertyId == UIA_IsTextPatternAvailablePropertyId)
	{
		pRetVal->vt = VT_BOOL;
		pRetVal->boolVal = TRUE;
		return S_OK;
	}
	else if (propertyId == UIA_ControlTypePropertyId)
	{
		pRetVal->vt = VT_I4;
		pRetVal->lVal = UIA_DocumentControlTypeId;
		return S_OK;
	}
	else if (propertyId == UIA_IsContentElementPropertyId || propertyId == UIA_IsControlElementPropertyId)
	{
		pRetVal->vt = VT_BOOL;
		pRetVal->boolVal = TRUE;
		return S_OK;
	}
	else if (propertyId == UIA_NativeWindowHandlePropertyId)
	{
		pRetVal->vt = VT_I4;
		pRetVal->lVal = 0;
		return S_OK;
	}
	else if (propertyId == UIA_AutomationIdPropertyId)
	{
		pRetVal->vt = VT_BSTR;
		pRetVal->bstrVal = SysAllocString(L"Document");
		return S_OK;
	}

	pRetVal->vt = VT_EMPTY;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::get_HostRawElementProvider(IRawElementProviderSimple **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	*pRetVal = NULL;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::get_ProviderOptions(ProviderOptions *pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	*pRetVal = ProviderOptions_ServerSideProvider;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::GetSelection(SAFEARRAY * *pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	if (m_released)
		return E_FAIL;

	SAFEARRAY *psa = SafeArrayCreateVector(VT_UNKNOWN, 0, 1);
	if (!psa)
		return E_OUTOFMEMORY;
	
	SumatraUIAutomationTextRange* selection = new SumatraUIAutomationTextRange( this, m_dm->textSelection );
	selection->DecreaseRefCount(); // magic, UIA seems to expect 0 refcount when selections are given to it

	LONG index = 0;
	SafeArrayPutElement(psa, &index, selection);

	*pRetVal = psa;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::GetVisibleRanges(SAFEARRAY * *pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	if (m_released)
		return E_FAIL;

	// return all pages' ranges that are even partially visible
	Vec<SumatraUIAutomationTextRange*> rangeArray;
	SumatraUIAutomationPageProvider* it = m_child_first;
	while (it)
	{
		if (it->m_dm->GetPageInfo(it->m_page_num) &&
			it->m_dm->GetPageInfo(it->m_page_num)->shown &&
			it->m_dm->GetPageInfo(it->m_page_num)->visibleRatio > 0.0f)
		{
			rangeArray.Append( new SumatraUIAutomationTextRange(this, it->m_page_num) );
		}
		
		// go to next element
		it = it->m_sibling_next;
	}

	// create safe array
	SAFEARRAY *psa = SafeArrayCreateVector(VT_UNKNOWN, 0, rangeArray.Size());
	if (!psa)
	{
		for (size_t i=0;i<rangeArray.Size();++i)
			delete rangeArray[i];
		return E_OUTOFMEMORY;
	}
    
	for (LONG i=0;i<(LONG)rangeArray.Size();++i)
		SafeArrayPutElement(psa, &i,rangeArray[i]);
    

	*pRetVal = psa;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::RangeFromChild(IRawElementProviderSimple *childElement, ITextRangeProvider **pRetVal)
{
	if (pRetVal == NULL || childElement == NULL)
		return E_POINTER;
	if (m_released)
		return E_FAIL;
	
	// get page range
	*pRetVal = new SumatraUIAutomationTextRange( this, ((SumatraUIAutomationPageProvider*)childElement)->m_page_num );
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::RangeFromPoint(struct UiaPoint point, ITextRangeProvider **pRetVal)
{
	// TODO: Is this even used? We wont support editing either way
	// so there won't be even a caret visible. Hence empty ranges are useless?
	return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::get_DocumentRange(ITextRangeProvider **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	if (m_released)
		return E_FAIL;
	
	SumatraUIAutomationTextRange* document_range = new SumatraUIAutomationTextRange(this);
	document_range->SetToDocumentRange();

	*pRetVal = document_range;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::get_SupportedTextSelection(enum SupportedTextSelection *pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	*pRetVal = SupportedTextSelection_Single;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::GetIdentityString(DWORD dwIDChild, BYTE **ppIDString, DWORD *pdwIDStringLen)
{
	if (ppIDString == NULL || pdwIDStringLen == NULL)
		return E_POINTER;
	if (m_released)
		return E_FAIL;

	for (SumatraUIAutomationPageProvider* it = m_child_first; it; it = it->m_sibling_next)
	{
		if (it->m_page_num == (int)dwIDChild + 1)
		{
			// Use memory address as identification. Use 8 bytes just in case
			*ppIDString = (BYTE*) CoTaskMemAlloc(8);
			if (!(*ppIDString))
				return E_OUTOFMEMORY;

			memset(*ppIDString, 0, 8);
			memcpy(*ppIDString, &it, sizeof( void* )); // copy the pointer to the allocated array
			*pdwIDStringLen = 8;
			return S_OK;
		}
	}

	return E_FAIL;
}
