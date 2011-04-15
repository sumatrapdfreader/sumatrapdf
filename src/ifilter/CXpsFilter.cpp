/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "StrUtil.h"
#include "CXpsFilter.h"
#include "PdfEngine.h"

extern HINSTANCE g_hInstance;

VOID CXpsFilter::CleanUp()
{
    if (m_xpsEngine) {
        delete m_xpsEngine;
        m_xpsEngine = NULL;
    }
    m_state = STATE_XPS_END;
}

HRESULT CXpsFilter::OnInit()
{
    CleanUp();

    m_xpsEngine = XpsEngine::CreateFromStream(m_pStream);
    if (!m_xpsEngine)
        return E_FAIL;

    m_state = STATE_XPS_START;
    m_iPageNo = 0;
    return S_OK;
}

HRESULT CXpsFilter::GetNextChunkValue(CChunkValue &chunkValue)
{
    WCHAR *str;

    switch (m_state) {
    case STATE_XPS_START:
        m_state = STATE_XPS_CONTENT;
        chunkValue.SetTextValue(PKEY_PerceivedType, L"document");
        return S_OK;

    case STATE_XPS_CONTENT:
        if (++m_iPageNo <= m_xpsEngine->PageCount()) {
            str = Str::Conv::ToWStrQ(m_xpsEngine->ExtractPageText(m_iPageNo, _T("\r\n")));
            chunkValue.SetTextValue(PKEY_Search_Contents, str, CHUNK_TEXT);
            free(str);
            return S_OK;
        }
        m_state = STATE_XPS_END;
        // fall through

    case STATE_XPS_END:
    default:
        return FILTER_E_END_OF_CHUNKS;
    }
}
