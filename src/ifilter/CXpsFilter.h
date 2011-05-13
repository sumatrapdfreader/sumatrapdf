/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "FilterBase.h"

enum XPS_FILTER_STATE { STATE_XPS_START, STATE_XPS_AUTHOR, STATE_XPS_TITLE, STATE_XPS_DATE, STATE_XPS_CONTENT, STATE_XPS_END };

class XpsEngine;

class CXpsFilter : public CFilterBase
{
public:
    CXpsFilter(long *plRefCount) : CFilterBase(plRefCount),
        m_state(STATE_XPS_END), m_iPageNo(-1), m_xpsEngine(NULL) { }
    virtual ~CXpsFilter() { CleanUp(); }

    virtual HRESULT OnInit();
    virtual HRESULT GetNextChunkValue(CChunkValue &chunkValue);

    VOID CleanUp();

private:
	XPS_FILTER_STATE m_state;
    int m_iPageNo;
    XpsEngine *m_xpsEngine;
};
