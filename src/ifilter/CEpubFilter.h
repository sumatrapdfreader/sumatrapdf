/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "FilterBase.h"
#include "PdfFilter.h"

enum EPUB_FILTER_STATE { STATE_EPUB_START, STATE_EPUB_AUTHOR, STATE_EPUB_TITLE, STATE_EPUB_DATE, STATE_EPUB_CONTENT, STATE_EPUB_END };

class EpubDoc;

class CEpubFilter : public CFilterBase
{
public:
    CEpubFilter(long *plRefCount) : CFilterBase(plRefCount),
        m_state(STATE_EPUB_END), m_epubDoc(NULL) { }
    virtual ~CEpubFilter() { CleanUp(); }

    virtual HRESULT OnInit();
    virtual HRESULT GetNextChunkValue(CChunkValue &chunkValue);

    VOID CleanUp();

    // IPersist
    IFACEMETHODIMP GetClassID(CLSID *pClassID) {
        return CLSIDFromString(SZ_EPUB_FILTER_HANDLER, pClassID);
    }

private:
    EPUB_FILTER_STATE m_state;
    EpubDoc *m_epubDoc;
};
