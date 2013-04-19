#include "BaseUtil.h"
#include "UIAutomationTextRange.h"

#include "DisplayModel.h"
#include "UIAutomationDocumentProvider.h"
#include "UIAutomationPageProvider.h"
#include "UIAutomationConstants.h"
#include "TextSelection.h"

SumatraUIAutomationTextRange::SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document)
: m_ref_count(0), m_document(document)
{
	this->AddRef(); //We are the only copy
	m_document->AddRef(); // hold on to the document

	SetToNullRange();
}
SumatraUIAutomationTextRange::SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document, int page_num)
: m_ref_count(0), m_document(document)
{
	this->AddRef(); //We are the only copy
	m_document->AddRef(); // hold on to the document

	m_startPage = page_num;
	m_startGlyph = 0;
	m_endPage = page_num;
	m_endGlyph = GetPageGlyphCount(page_num);
}
SumatraUIAutomationTextRange::SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document, TextSelection* range)
: m_ref_count(0), m_document(document)
{
	this->AddRef(); //We are the only copy
	m_document->AddRef(); // hold on to the document

    range->GetGlyphRange(&m_startPage, &m_startGlyph, &m_endPage, &m_endGlyph);
	// null-range check
	if ( m_startPage == -1 || m_endPage == -1)
	{
		SetToNullRange();
	}
}
SumatraUIAutomationTextRange::SumatraUIAutomationTextRange(const SumatraUIAutomationTextRange&b)
: m_ref_count(0), m_document(b.m_document)
{
	this->AddRef(); //We are the only copy
	m_document->AddRef(); // hold on to the document

	m_startPage = b.m_startPage;
	m_startGlyph = b.m_startGlyph;
	m_endPage = b.m_endPage;
	m_endGlyph = b.m_endGlyph;
}
SumatraUIAutomationTextRange::~SumatraUIAutomationTextRange()
{
	m_document->Release();
}
void SumatraUIAutomationTextRange::DecreaseRefCount()
{
	--m_ref_count;
}
bool SumatraUIAutomationTextRange::operator==(const SumatraUIAutomationTextRange&b) const
{
	return  m_document == b.m_document && 
	        m_startPage == b.m_startPage && m_endPage == b.m_endPage &&
	        m_startGlyph == b.m_startGlyph && m_endGlyph == b.m_endGlyph;
}

void SumatraUIAutomationTextRange::SetToDocumentRange()
{
	m_startPage = 1;
	m_startGlyph = 0;
	m_endPage = m_document->GetDM()->PageCount();
	m_endGlyph = GetPageGlyphCount(m_endPage);
}
void SumatraUIAutomationTextRange::SetToNullRange()
{
	m_startPage = -1;
	m_startGlyph = 0;
	m_endPage = -1;
	m_endGlyph = 0;
}
bool SumatraUIAutomationTextRange::IsNullRange() const
{
	return (m_startPage == -1 && m_endPage == -1);
}
bool SumatraUIAutomationTextRange::IsEmptyRange() const
{
	return (m_startPage == m_endPage && m_startGlyph == m_endGlyph);
}
int SumatraUIAutomationTextRange::GetPageGlyphCount(int page_num)
{
	assert(m_document->IsDocumentLoaded());
	assert(page_num > 0);

	int pageLen;
	m_document->GetDM()->textCache->GetData(page_num, &pageLen);
	return pageLen;
}
int SumatraUIAutomationTextRange::GetPageCount()
{
	assert(m_document->IsDocumentLoaded());

	return m_document->GetDM()->PageCount();
}
void SumatraUIAutomationTextRange::ValidateStartEndpoint()
{
	// ensure correct ordering of endpoints
	if (m_startPage > m_endPage ||
		(m_startPage == m_endPage && m_startGlyph > m_endGlyph))
	{
		m_startPage = m_endPage;
		m_startGlyph = m_endGlyph;
	}
}
void SumatraUIAutomationTextRange::ValidateEndEndpoint()
{
	// ensure correct ordering of endpoints
	if (m_startPage > m_endPage ||
		(m_startPage == m_endPage && m_startGlyph > m_endGlyph))
	{
		m_endPage = m_startPage;
		m_endGlyph = m_startGlyph;
	}
}
int SumatraUIAutomationTextRange::FindPreviousWordEndpoint(int pageno, int idx, bool dont_return_initial)
{
	// based on TextSelection::SelectWordAt
	int textLen;
	const WCHAR *page_text = m_document->GetDM()->textCache->GetData(pageno, &textLen);
	
	if (dont_return_initial)
		for (; idx > 0; idx--)
			if (iswordchar(page_text[idx - 1]))
				break;

	for (; idx > 0; idx--)
		if (!iswordchar(page_text[idx - 1]))
			break;
	return idx;
}
int SumatraUIAutomationTextRange::FindNextWordEndpoint(int pageno, int idx, bool dont_return_initial)
{
	int textLen;
	const WCHAR *page_text = m_document->GetDM()->textCache->GetData(pageno, &textLen);

	if (dont_return_initial)
		for (; idx < textLen; idx++)
			if (iswordchar(page_text[idx]))
				break;

	for (; idx < textLen; idx++)
		if (!iswordchar(page_text[idx]))
			break;
	return idx;
}
int SumatraUIAutomationTextRange::FindPreviousLineEndpoint(int pageno, int idx, bool dont_return_initial)
{
	int textLen;
	const WCHAR *page_text = m_document->GetDM()->textCache->GetData(pageno, &textLen);
	
	if (dont_return_initial)
		for (; idx > 0; idx--)
			if (page_text[idx - 1] != L'\n')
				break;

	for (; idx > 0; idx--)
		if (page_text[idx - 1] == L'\n')
			break;
	return idx;
}
int SumatraUIAutomationTextRange::FindNextLineEndpoint(int pageno, int idx, bool dont_return_initial)
{
	int textLen;
	const WCHAR *page_text = m_document->GetDM()->textCache->GetData(pageno, &textLen);
	
	if (dont_return_initial)
		for (; idx < textLen; idx++)
			if (page_text[idx] != L'\n')
				break;

	for (; idx < textLen; idx++)
		if (page_text[idx] == L'\n')
			break;
	return idx;
}

//IUnknown
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::QueryInterface(const IID &iid,void **ppvObject)
{
	if (ppvObject == NULL)
		return E_POINTER;
	
	if (iid == IID_ITextRangeProvider)
	{
		*ppvObject = static_cast<ITextRangeProvider*>(this);
		this->AddRef(); //New copy has entered the universe
		return S_OK;
	}

	*ppvObject = NULL;
	return E_NOINTERFACE;
}
ULONG STDMETHODCALLTYPE SumatraUIAutomationTextRange::AddRef(void)
{
	return ++m_ref_count;
}
ULONG STDMETHODCALLTYPE SumatraUIAutomationTextRange::Release(void)
{
	assert(m_ref_count);
	if (--m_ref_count)
		return m_ref_count;

	//Suicide
	delete this;
	return 0;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::Clone(ITextRangeProvider **clonedRange)
{
	if (clonedRange == NULL)
		return E_POINTER;
	*clonedRange = new SumatraUIAutomationTextRange(*this);
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::Compare(ITextRangeProvider *range, BOOL *areSame)
{
	if (areSame == NULL)
		return E_POINTER;
	if (range == NULL)
		return E_POINTER;
	if (*((SumatraUIAutomationTextRange*)range) == *this)
		*areSame = TRUE;
	else
		*areSame = FALSE;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::CompareEndpoints(enum TextPatternRangeEndpoint srcEndPoint, ITextRangeProvider *range, enum TextPatternRangeEndpoint targetEndPoint, int *compValue)
{
	if (range == NULL)
		return E_POINTER;
	if (compValue == NULL)
		return E_POINTER;

	int comp_a_page, comp_a_idx;
	if (srcEndPoint == TextPatternRangeEndpoint_Start)
	{
		comp_a_page = this->m_startPage;
		comp_a_idx = this->m_startGlyph;
	}
	else if (srcEndPoint == TextPatternRangeEndpoint_End)
	{
		comp_a_page = this->m_endPage;
		comp_a_idx = this->m_endGlyph;
	}
	else
	{
		return E_INVALIDARG;
	}

	SumatraUIAutomationTextRange* target = (SumatraUIAutomationTextRange*)range;

	int comp_b_page, comp_b_idx;
	if (targetEndPoint == TextPatternRangeEndpoint_Start)
	{
		comp_b_page = target->m_startPage;
		comp_b_idx = target->m_startGlyph;
	}
	else if (targetEndPoint == TextPatternRangeEndpoint_End)
	{
		comp_b_page = target->m_endPage;
		comp_b_idx = target->m_endGlyph;
	}
	else
	{
		return E_INVALIDARG;
	}

	if (comp_a_page < comp_b_page)
		*compValue = -1;
	else if (comp_a_page > comp_b_page)
		*compValue = 1;
	else
		*compValue = comp_a_idx - comp_b_idx;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::ExpandToEnclosingUnit(enum TextUnit textUnit)
{
	//if document is closed, don't do anything
	if (!m_document->IsDocumentLoaded())
		return E_FAIL;

	//if not set, don't do anything
	if (IsNullRange())
		return S_OK;

	if (textUnit == TextUnit_Character)
	{
		//done
		return S_OK;
	}
	else if (textUnit == TextUnit_Format)
	{
		// what is a "format" anyway?
		return S_OK;
	}
	else if (textUnit == TextUnit_Word)
	{
		// select current word at start endpoint
		int word_beg = FindPreviousWordEndpoint(m_startPage, m_startGlyph);
		int word_end = FindNextWordEndpoint(m_startPage, m_startGlyph);

		m_endPage = m_startPage;

		m_startGlyph = word_beg;
		m_endGlyph = word_end;

		return S_OK;
	}
	else if (textUnit == TextUnit_Line || textUnit == TextUnit_Paragraph)
	{
		// select current line or current paragraph. In general case these cannot be differentiated? Right?
		int word_beg = FindPreviousLineEndpoint(m_startPage, m_startGlyph);
		int word_end = FindNextLineEndpoint(m_startPage, m_startGlyph);

		m_endPage = m_startPage;

		m_startGlyph = word_beg;
		m_endGlyph = word_end;

		return S_OK;
	}
	else if (textUnit == TextUnit_Page)
	{
		// select current page

		// start from the beginning of start page
		m_startGlyph = 0;
		
		// to the end of the end page
		m_endGlyph = GetPageGlyphCount(m_endPage);

		return S_OK;
	}
	else if (textUnit == TextUnit_Document)
	{
		SetToDocumentRange();
		return S_OK;
	}
	else
	{
		return E_INVALIDARG;
	}
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::FindAttribute(TEXTATTRIBUTEID attr, VARIANT val, BOOL backward, ITextRangeProvider **found)
{
	if (found == NULL)
		return E_POINTER;
	if (!m_document->IsDocumentLoaded())
		return E_FAIL;

	// raw text doesn't have attributes so just don't find anything
	*found = NULL;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::FindText(BSTR text, BOOL backward, BOOL ignoreCase, ITextRangeProvider **found)
{
	if (found == NULL)
		return E_POINTER;
	if (!m_document->IsDocumentLoaded())
		return E_FAIL;

	//TODO: Support text searching
	return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::GetAttributeValue(TEXTATTRIBUTEID attr, VARIANT *value)
{
	if (value == NULL)
		return E_POINTER;
	if (!m_document->IsDocumentLoaded())
		return E_FAIL;

	// text doesn't have attributes, we don't support those
	IUnknown* not_supported;
	UiaGetReservedNotSupportedValue( &not_supported );

	value->vt = VT_UNKNOWN;
	value->punkVal = not_supported;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::GetBoundingRectangles(SAFEARRAY * *boundingRects)
{
	if (boundingRects == NULL)
		return E_POINTER;
	if (!m_document->IsDocumentLoaded())
		return E_FAIL;

	if (IsNullRange())
	{
		SAFEARRAY* sarray = SafeArrayCreateVector(VT_R8,0,0);
		if (sarray)
			return E_OUTOFMEMORY;

		*boundingRects = sarray;
		return S_OK;
	}

	// TODO: support GetBoundingRectangles
	return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::GetEnclosingElement(IRawElementProviderSimple **enclosingElement)
{
	if (enclosingElement == NULL)
		return E_POINTER;
	if (!m_document->IsDocumentLoaded())
		return E_FAIL;

	*enclosingElement = m_document;
	(*enclosingElement)->AddRef();
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::GetText(int maxLength, BSTR *text)
{
	if (text == NULL)
		return E_POINTER;
	if (!m_document->IsDocumentLoaded())
		return E_FAIL;

	if (IsNullRange() || IsEmptyRange())
	{
		*text = SysAllocString(L""); // 0-sized not-null string
		return S_OK;
	}

	TextSelection selection(m_document->GetDM()->engine, m_document->GetDM()->textCache);
	selection.StartAt(m_startPage, m_startGlyph);
	selection.SelectUpTo(m_endPage, m_endGlyph);
	
	ScopedMem<WCHAR> selected_text(selection.ExtractText(L"\r\n"));
	int selected_text_length = str::Len(selected_text);

	if (maxLength > -2) // -1 and [0, inf) are allowed
	{
		if (maxLength != -1 && selected_text_length > maxLength)
			selected_text[maxLength] = '\0'; // truncate
		
		*text = SysAllocString(selected_text);
		if (*text)
			return S_OK;
		else
			return E_OUTOFMEMORY;
	}
	else
	{
		return E_INVALIDARG;
	}
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::Move(enum TextUnit unit,int count, int *moved)
{
	if (moved == NULL)
		return E_POINTER;

	// if document is closed, don't do anything
	if (!m_document->IsDocumentLoaded())
		return E_FAIL;

	// Just move the endpoints using other methods
	*moved = 0;
	this->ExpandToEnclosingUnit(unit);

	if (count > 0)
	{
		for (int i=0;i<count;++i)
		{
			int sub_moved;
			this->MoveEndpointByUnit(TextPatternRangeEndpoint_End, unit, 1, &sub_moved);

			// Move end first, other will succeed if this succeeds
			if (sub_moved == 0)
				break;

			this->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, unit, 1, &sub_moved);
			++*moved;
		}
	}
	else if (count < 0)
	{
		for (int i=0;i<-count;++i)
		{
			int sub_moved;
			this->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, unit, -1, &sub_moved);

			// Move start first, other will succeed if this succeeds
			if (sub_moved == 0)
				break;

			this->MoveEndpointByUnit(TextPatternRangeEndpoint_End, unit, -1, &sub_moved);
			++*moved;
		}
	}

	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::MoveEndpointByUnit(TextPatternRangeEndpoint endpoint, TextUnit unit, int count, int *moved)
{
	if (moved == NULL)
		return E_POINTER;

	// if document is closed, don't do anything
	if (!m_document->IsDocumentLoaded())
		return E_FAIL;

	// if not set, don't do anything
	if (IsNullRange())
		return S_OK;

	// what to move
	int *target_page, *target_glyph;
	if (endpoint == TextPatternRangeEndpoint_Start)
	{
		target_page = &m_startPage;
		target_glyph = &m_startGlyph;
	}
	else if (endpoint == TextPatternRangeEndpoint_End)
	{
		target_page = &m_endPage;
		target_glyph = &m_endGlyph;
	}
	else
	{
		return E_INVALIDARG;
	}

	class EndPointMover
	{
	protected:
		SumatraUIAutomationTextRange* m_target;
		int* m_target_page;
		int* m_target_glyph;

	public:
		// return false when cannot be moved
		virtual bool NextEndpoint() const
		{
			// HACK: Declaring these as pure virtual causes "unreferenced local variable" warnings ==> define a dummy body to get rid of warnings
			assert(false && "should not happen");
			return false;
		}
		virtual bool PrevEndpoint() const
		{
			assert(false && "should not happen");
			return false;
		}

		// return false when not appliable
		bool NextPage() const
		{
			int max_glyph = m_target->GetPageGlyphCount( *m_target_page );
			
			if (*m_target_glyph == max_glyph)
			{
				if (*m_target_page == m_target->GetPageCount())
				{
					// last page
					return false;
				}

				// go to next page
				(*m_target_page)++;
				(*m_target_glyph) = 0;
			}
			return true;
		}
		bool PreviousPage() const
		{
			if (*m_target_glyph == 0)
			{
				if (*m_target_page == 1)
				{
					// first page
					return false;
				}

				// go to next page
				(*m_target_page)--;
				(*m_target_glyph) = m_target->GetPageGlyphCount( *m_target_page );
			}
			return true;
		}

		// do the moving
		int Move(int count, SumatraUIAutomationTextRange* target, int* target_page, int* target_glyph)
		{
			m_target = target;
			m_target_page = target_page;
			m_target_glyph = target_glyph;

			int retVal = 0;
			if (count > 0)
			{
				for (int i=0;i<count && (NextPage() || NextEndpoint());++i)
					++retVal;
			}
			else
			{
				for (int i=0;i<-count && (PreviousPage() || PrevEndpoint());++i)
					++retVal;
			}

			return retVal;
		}
	};
	class CharEndPointMover : public EndPointMover
	{
		bool NextEndpoint() const 
		{
			(*m_target_glyph)++;
			return true;
		}
		bool PrevEndpoint() const 
		{
			(*m_target_glyph)--;
			return true;
		}
	};
	class WordEndPointMover : public EndPointMover
	{
		bool NextEndpoint() const 
		{
			(*m_target_glyph) = m_target->FindNextWordEndpoint( *m_target_page, *m_target_glyph, true);
			return true;
		}
		bool PrevEndpoint() const 
		{
			(*m_target_glyph) = m_target->FindPreviousWordEndpoint( *m_target_page, *m_target_glyph, true);
			(*m_target_glyph)--;
			return true;
		}
	};
	class LineEndPointMover : public EndPointMover
	{
		bool NextEndpoint() const 
		{
			(*m_target_glyph) = m_target->FindNextLineEndpoint( *m_target_page, *m_target_glyph, true);
			return true;
		}
		bool PrevEndpoint() const 
		{
			(*m_target_glyph) = m_target->FindPreviousLineEndpoint( *m_target_page, *m_target_glyph, true);
			(*m_target_glyph)--;
			return true;
		}
	};

	// how much to move
	if (unit == TextUnit_Character)
	{
		CharEndPointMover mover;
		*moved = mover.Move( count, this, target_page, target_glyph );
	}
	else if (unit == TextUnit_Word || unit == TextUnit_Format)
	{
		WordEndPointMover mover;
		*moved = mover.Move( count, this, target_page, target_glyph );
	}
	else if (unit == TextUnit_Line || unit == TextUnit_Paragraph)
	{
		LineEndPointMover mover;
		*moved = mover.Move( count, this, target_page, target_glyph );
	}
	else if (unit == TextUnit_Page)
	{
		*moved = 0;
		*target_glyph = 0;

		if (count > 0)
		{
			// GetPageCount()+1 => allow overflow momentarily
			for (int i=0;i<count && *target_page!=GetPageCount()+1;++i)
			{
				(*target_page)++;
				(*moved)++;
			}

			// fix overflow, allow seeking to the end this way
			if ( *target_page == GetPageCount()+1)
			{
				*target_page = GetPageCount();
				*target_glyph = GetPageGlyphCount( *target_page );
			}
		}
		else
		{
			for (int i=0;i<-count && *target_page!=1;++i)
			{
				(*target_page)--;
				(*moved)++;
			}
		}
	}
	else if (unit == TextUnit_Document)
	{
		if (count > 0)
		{
			int end_page = GetPageCount();
			int end_glyph = GetPageGlyphCount( *target_page );

			if (*target_page != end_page || *target_glyph != end_glyph)
			{
				*target_page = end_page;
				*target_glyph = end_glyph;
				*moved = 1;
			}
			else
			{
				*moved = 0;
			}
		}
		else
		{
			const int beg_page = 0;
			const int beg_glyph = 0;

			if (*target_page != beg_page || *target_glyph != beg_glyph)
			{
				*target_page = beg_page;
				*target_glyph = beg_glyph;
				*moved = 1;
			}
			else
			{
				*moved = 0;
			}
		}
	}
	else
	{
		return E_INVALIDARG;
	}

	// keep range valid
	if (endpoint == TextPatternRangeEndpoint_Start)
	{
		// drag end with start
		ValidateEndEndpoint();
	}
	else if (endpoint == TextPatternRangeEndpoint_End)
	{
		// drag start with end
		ValidateStartEndpoint();
	}

	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::MoveEndpointByRange(TextPatternRangeEndpoint srcEndPoint, ITextRangeProvider *range, TextPatternRangeEndpoint targetEndPoint)
{
	if (range == NULL)
		return E_POINTER;

	SumatraUIAutomationTextRange* target = (SumatraUIAutomationTextRange*)range;

	// extract target location
	int target_page, target_idx;
	if (targetEndPoint == TextPatternRangeEndpoint_Start)
	{
		target_page = target->m_startPage;
		target_idx = target->m_startGlyph;
	}
	else if (targetEndPoint == TextPatternRangeEndpoint_End)
	{
		target_page = target->m_endPage;
		target_idx = target->m_endGlyph;
	}
	else
	{
		return E_INVALIDARG;
	}


	// apply
	if (srcEndPoint == TextPatternRangeEndpoint_Start)
	{
		m_startPage = target_page;
		m_startGlyph = target_idx;

		// drag end with start
		ValidateEndEndpoint();
	}
	else if (srcEndPoint == TextPatternRangeEndpoint_End)
	{
		m_endPage = target_page;
		m_endGlyph = target_idx;

		// drag start with end
		ValidateStartEndpoint();
	}
	else
	{
		return E_INVALIDARG;
	}


	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::Select(void)
{
	if (!m_document->IsDocumentLoaded())
		return E_FAIL;

	if (IsNullRange() || IsEmptyRange())
	{
		m_document->GetDM()->textSelection->Reset();
	}
	else
	{
		m_document->GetDM()->textSelection->Reset();
		m_document->GetDM()->textSelection->StartAt(m_startPage, m_startGlyph);
		m_document->GetDM()->textSelection->SelectUpTo(m_endPage, m_endGlyph);
	}

	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::AddToSelection(void)
{
	return E_FAIL;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::RemoveFromSelection(void)
{
	return E_FAIL;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::ScrollIntoView(BOOL alignToTop)
{
	if (!m_document->IsDocumentLoaded())
		return E_FAIL;
	
	// extract target location
	int target_page, target_idx;
	if (IsNullRange())
	{
		target_page = 0;
		target_idx = 0;
	}
	else if (alignToTop)
	{
		target_page = m_startPage;
		target_idx = m_startGlyph;
	}
	else
	{
		target_page = m_endPage;
		target_idx = m_endGlyph;
	}

	// TODO: Scroll to target_page, target_idx
	//m_document->GetDM()->ScrollYTo()
	return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::GetChildren(SAFEARRAY **children)
{
	if (children == NULL)
		return E_POINTER;
	if (!m_document->IsDocumentLoaded())
		return E_FAIL;

	// return all children in range
	if (IsNullRange())
	{
		SAFEARRAY *psa = SafeArrayCreateVector(VT_UNKNOWN, 0, 0);
		if (!psa)
			return E_OUTOFMEMORY;

		*children = psa;
		return S_OK;
	}

	SAFEARRAY *psa = SafeArrayCreateVector(VT_UNKNOWN, 0, m_endPage - m_startPage + 1);
	if (!psa)
		return E_OUTOFMEMORY;

	SumatraUIAutomationPageProvider* it = m_document->GetFirstPage();
	while (it)
	{
		if (it->GetPageNum() >= m_startPage || it->GetPageNum() <= m_endPage)
		{	
			LONG index = it->GetPageNum() - m_startPage;

			SafeArrayPutElement(psa, &index, it);
			it->AddRef();
		}

		it = it->GetNextPage();
	}

	*children = psa;
	return S_OK;
}
