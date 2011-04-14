/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "StrUtil.h"
#include "CPdfFilter.h"
#include "PdfEngine.h"

extern HINSTANCE g_hInstance;

VOID CPdfFilter::CleanUp()
{
    if (m_pdfEngine) {
        delete m_pdfEngine;
        m_pdfEngine = NULL;
    }
    m_state = STATE_PDF_END;
}

HRESULT CPdfFilter::OnInit()
{
    CleanUp();

    m_pdfEngine = PdfEngine::CreateFromStream(m_pStream);
    if (!m_pdfEngine)
        return E_FAIL;

    m_state = STATE_PDF_START;
    m_iPageNo = 0;
    return S_OK;
}

// adapted from SumatraProperties.cpp
static bool PdfDateParse(WCHAR *pdfDate, SYSTEMTIME *timeOut) {
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    // "D:" at the beginning is optional
    if (Str::StartsWith(pdfDate, L"D:"))
        pdfDate += 2;
    return 6 == swscanf(pdfDate, L"%4d%2d%2d" L"%2d%2d%2d",
        &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay,
        &timeOut->wHour, &timeOut->wMinute, &timeOut->wSecond);
    // don't bother about the day of week, we won't display it anyway
}

HRESULT CPdfFilter::GetNextChunkValue(CChunkValue &chunkValue)
{
    WCHAR *str;

    switch (m_state) {
    case STATE_PDF_START:
        m_state = STATE_PDF_AUTHOR;
        chunkValue.SetTextValue(PKEY_PerceivedType, L"document");
        return S_OK;

    case STATE_PDF_AUTHOR:
        m_state = STATE_PDF_TITLE;
        str = Str::Conv::ToWStrQ(m_pdfEngine->GetProperty("Author"));
        if (!Str::IsEmpty(str)) {
            chunkValue.SetTextValue(PKEY_Author, str);
            free(str);
            return S_OK;
        }
        free(str);
        // fall through

    case STATE_PDF_TITLE:
        m_state = STATE_PDF_DATE;
        str = Str::Conv::ToWStrQ(m_pdfEngine->GetProperty("Title"));
        if (!str) str = Str::Conv::ToWStrQ(m_pdfEngine->GetProperty("Subject"));
        if (!Str::IsEmpty(str)) {
            chunkValue.SetTextValue(PKEY_Title, str);
            free(str);
            return S_OK;
        }
        free(str);
        // fall through

    case STATE_PDF_DATE:
        m_state = STATE_PDF_CONTENT;
        str = Str::Conv::ToWStrQ(m_pdfEngine->GetProperty("ModDate"));
        if (!str) str = Str::Conv::ToWStrQ(m_pdfEngine->GetProperty("CreationDate"));
        if (!Str::IsEmpty(str)) {
            SYSTEMTIME systime;
            if (PdfDateParse(str, &systime)) {
                FILETIME filetime;
                SystemTimeToFileTime(&systime, &filetime);
                chunkValue.SetFileTimeValue(PKEY_ItemDate, filetime);
                free(str);
                return S_OK;
            }
        }
        free(str);
        // fall through

    case STATE_PDF_CONTENT:
        if (++m_iPageNo <= m_pdfEngine->PageCount()) {
            str = Str::Conv::ToWStrQ(m_pdfEngine->ExtractPageText(m_iPageNo, _T("\r\n")));
            chunkValue.SetTextValue(PKEY_Search_Contents, str, CHUNK_TEXT);
            free(str);
            return S_OK;
        }
        m_state = STATE_PDF_END;
        // fall through

    case STATE_PDF_END:
    default:
        return FILTER_E_END_OF_CHUNKS;
    }
}
