/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum PDF_FILTER_STATE { STATE_PDF_START, STATE_PDF_AUTHOR, STATE_PDF_TITLE, STATE_PDF_DATE, STATE_PDF_CONTENT, STATE_PDF_END };

class BaseEngine;

class CPdfFilter : public CFilterBase
{
public:
    CPdfFilter(long *plRefCount) : CFilterBase(plRefCount),
        m_state(STATE_PDF_END), m_iPageNo(-1), m_pdfEngine(nullptr) { }
    virtual ~CPdfFilter() { CleanUp(); }

    virtual HRESULT OnInit();
    virtual HRESULT GetNextChunkValue(CChunkValue &chunkValue);

    VOID CleanUp();

    // IPersist
    IFACEMETHODIMP GetClassID(CLSID *pClassID) {
        return CLSIDFromString(SZ_PDF_FILTER_HANDLER, pClassID);
    }

private:
    PDF_FILTER_STATE m_state;
    int m_iPageNo;
    BaseEngine *m_pdfEngine;
};
