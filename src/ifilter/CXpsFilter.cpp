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

// adapted from SumatraProperties.cpp
static bool XpsDateParse(const WCHAR *xpsDate, SYSTEMTIME *timeOut)
{
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    return 6 == swscanf(xpsDate, L"%4d-%2d-%2d" L"T%2d:%2d:%2dZ",
        &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay,
        &timeOut->wHour, &timeOut->wMinute, &timeOut->wSecond) ||
        swscanf(xpsDate, L"%4d-%2d-%2d", &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay);
    // don't bother about the day of week, we won't display it anyway
}

HRESULT CXpsFilter::GetNextChunkValue(CChunkValue &chunkValue)
{
    WCHAR *str;

    switch (m_state) {
    case STATE_XPS_START:
        m_state = STATE_XPS_AUTHOR;
        chunkValue.SetTextValue(PKEY_PerceivedType, L"document");
        return S_OK;

    case STATE_XPS_AUTHOR:
        m_state = STATE_XPS_TITLE;
        str = str::conv::ToWStrQ(m_xpsEngine->GetProperty("Author"));
        if (!str::IsEmpty(str)) {
            chunkValue.SetTextValue(PKEY_Author, str);
            free(str);
            return S_OK;
        }
        free(str);
        // fall through

    case STATE_XPS_TITLE:
        m_state = STATE_XPS_DATE;
        str = str::conv::ToWStrQ(m_xpsEngine->GetProperty("Title"));
        if (!str) str = str::conv::ToWStrQ(m_xpsEngine->GetProperty("Subject"));
        if (!str::IsEmpty(str)) {
            chunkValue.SetTextValue(PKEY_Title, str);
            free(str);
            return S_OK;
        }
        free(str);
        // fall through

    case STATE_XPS_DATE:
        m_state = STATE_XPS_CONTENT;
        str = str::conv::ToWStrQ(m_xpsEngine->GetProperty("ModDate"));
        if (!str) str = str::conv::ToWStrQ(m_xpsEngine->GetProperty("CreationDate"));
        if (!str::IsEmpty(str)) {
            SYSTEMTIME systime;
            if (XpsDateParse(str, &systime)) {
                FILETIME filetime;
                SystemTimeToFileTime(&systime, &filetime);
                chunkValue.SetFileTimeValue(PKEY_ItemDate, filetime);
                free(str);
                return S_OK;
            }
        }
        free(str);
        // fall through

    case STATE_XPS_CONTENT:
        if (++m_iPageNo <= m_xpsEngine->PageCount()) {
            str = str::conv::ToWStrQ(m_xpsEngine->ExtractPageText(m_iPageNo, _T("\r\n")));
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
