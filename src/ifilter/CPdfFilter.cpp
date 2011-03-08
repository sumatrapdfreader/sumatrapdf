/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "WStrUtil.h"
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

    STATSTG stat;
    HRESULT res = m_pStream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(res))
        return res;

    DWORD size = stat.cbSize.LowPart;
    fz_buffer *filedata = fz_newbuffer(size);
    filedata->len = size;

    LARGE_INTEGER zero;
    zero.QuadPart = 0;
    m_pStream->Seek(zero, STREAM_SEEK_SET, NULL);
    res = m_pStream->Read(filedata->data, filedata->len, NULL);
    if (FAILED(res)) {
        fz_dropbuffer(filedata);
        return res;
    }

    fz_stream *stm = fz_openbuffer(filedata);
    fz_dropbuffer(filedata);

    m_pdfEngine = PdfEngine::CreateFromStream(stm);
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
    if (wstr_startswith(pdfDate, L"D:"))
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
        str = tstr_to_wstr_q(m_pdfEngine->getPdfInfo("Author"));
        if (!wstr_empty(str)) {
            chunkValue.SetTextValue(PKEY_Author, str);
            free(str);
            return S_OK;
        }
        free(str);
        // fall through

    case STATE_PDF_TITLE:
        m_state = STATE_PDF_DATE;
        str = tstr_to_wstr_q(m_pdfEngine->getPdfInfo("Title"));
        if (!str) str = tstr_to_wstr_q(m_pdfEngine->getPdfInfo("Subject"));
        if (!wstr_empty(str)) {
            chunkValue.SetTextValue(PKEY_Title, str);
            free(str);
            return S_OK;
        }
        free(str);
        // fall through

    case STATE_PDF_DATE:
        m_state = STATE_PDF_CONTENT;
        str = tstr_to_wstr_q(m_pdfEngine->getPdfInfo("ModDate"));
        if (!str) str = tstr_to_wstr_q(m_pdfEngine->getPdfInfo("CreationDate"));
        if (!wstr_empty(str)) {
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
        if (++m_iPageNo <= m_pdfEngine->pageCount()) {
            str = tstr_to_wstr_q(m_pdfEngine->ExtractPageText(m_iPageNo));
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
