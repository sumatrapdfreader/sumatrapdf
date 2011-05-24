/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "FilterBase.h"
#include "PdfFilter.h"

enum PDF_FILTER_STATE { STATE_PDF_START, STATE_PDF_AUTHOR, STATE_PDF_TITLE, STATE_PDF_DATE, STATE_PDF_CONTENT, STATE_PDF_END };

class PdfEngine;

class CPdfFilter : public CFilterBase
{
public:
    CPdfFilter(long *plRefCount) : CFilterBase(plRefCount),
        m_state(STATE_PDF_END), m_iPageNo(-1), m_pdfEngine(NULL) { }
    virtual ~CPdfFilter() { CleanUp(); }

    virtual HRESULT OnInit();
    virtual HRESULT GetNextChunkValue(CChunkValue &chunkValue);

    VOID CleanUp();

    // IPersist
    IFACEMETHODIMP GetClassID(CLSID *pClassID) {
        return CLSIDFromTString(SZ_PDF_FILTER_HANDLER, pClassID);
    }

private:
    PDF_FILTER_STATE m_state;
    int m_iPageNo;
    PdfEngine *m_pdfEngine;
};
