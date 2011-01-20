/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <windows.h>
#include <shlwapi.h>
#include <sddl.h>
#include <tchar.h>
#include <inttypes.h>
#include "tstr_util.h"
#include "CPdfFilter.h"
#include "PdfEngine.h"

extern HINSTANCE g_hInstance;

TCHAR *GetPasswordForFile(WindowInfo *win, const TCHAR *fileName, pdf_xref *xref, unsigned char *decryptionKey, bool *saveKey)
{
    return NULL;
}

static DWORD WINAPI UpdateDataForIndexing(LPVOID IFilterMMap);

HRESULT CPdfFilter::OnInit()
{
    CleanUp();

    STATSTG stat;
    HRESULT res = m_pStream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(res))
        return res;

    m_utd.len = stat.cbSize.LowPart;
    m_utd.data = (char *)malloc(m_utd.len + 1);

    m_utd.produce = CreateEvent(NULL, FALSE, TRUE, NULL);
    m_utd.consume = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!m_utd.data || !m_utd.produce || !m_utd.consume)
        return E_FAIL;

    LARGE_INTEGER zero;
    zero.QuadPart = 0;
    m_pStream->Seek(zero, STREAM_SEEK_SET, NULL);
    res = m_pStream->Read(m_utd.data, m_utd.len, NULL);
    if (FAILED(res))
        return res;

    m_hThread = CreateThread(NULL, 0, UpdateDataForIndexing, &m_utd, 0, NULL);
    return S_OK;
}

// adapted from SumatraProperties.cpp
static bool PdfDateParse(char *pdfDate, SYSTEMTIME *timeOut) {
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    // "D:" at the beginning is optional
    if (str_startswith(pdfDate, "D:"))
        pdfDate += 2;
    return 6 == sscanf(pdfDate, "%4d%2d%2d" "%2d%2d%2d",
        &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay,
        &timeOut->wHour, &timeOut->wMinute, &timeOut->wSecond);
    // don't bother about the day of week, we won't display it anyway
}

HRESULT CPdfFilter::GetNextChunkValue(CChunkValue &chunkValue)
{
    SYSTEMTIME systime;

    if (m_bDone)
        return FILTER_E_END_OF_CHUNKS;
    if (WaitForSingleObject(m_utd.consume, FILTER_TIMEOUT_IN_MS) != WAIT_OBJECT_0)
        m_bDone = true;
    if (!*m_utd.data)
        m_bDone = true;
    if (m_bDone)
        return FILTER_E_END_OF_CHUNKS;

    if (str_startswith(m_utd.data, "Type:") && m_utd.data[5]) {
        WCHAR *type = utf8_to_wstr(m_utd.data + 5);
        chunkValue.SetTextValue(PKEY_PerceivedType, type);
        free(type);
        goto Success;
    }

    if (str_startswith(m_utd.data, "Author:") && m_utd.data[7]) {
        WCHAR *author = utf8_to_wstr(m_utd.data + 7);
        chunkValue.SetTextValue(PKEY_Author, author);
        free(author);
        goto Success;
    }

    if (str_startswith(m_utd.data, "Title:") && m_utd.data[6]) {
        WCHAR *title = utf8_to_wstr(m_utd.data + 6);
        chunkValue.SetTextValue(PKEY_Title, title);
        free(title);
        goto Success;
    }

    if (str_startswith(m_utd.data, "Date:") && PdfDateParse(m_utd.data + 5, &systime)) {
        FILETIME filetime;
        SystemTimeToFileTime(&systime, &filetime);
        chunkValue.SetFileTimeValue(PKEY_ItemDate, filetime);
        goto Success;
    }

    if (str_startswith(m_utd.data, "Content:")) {
        WCHAR *content = utf8_to_wstr(m_utd.data + 8);
        chunkValue.SetTextValue(PKEY_Search_Contents, content, CHUNK_TEXT);
        free(content);
        goto Success;
    }

    // if (!str_endswith(m_utd.data, ":"))
    //     fprintf(stderr, "Unexpected data: %s\n", m_utd.data);

    *m_utd.data = '\0';
    SetEvent(m_utd.produce);
    return GetNextChunkValue(chunkValue);

Success:
    *m_utd.data = '\0';
    SetEvent(m_utd.produce);
    return S_OK;
}

static DWORD WINAPI UpdateDataForIndexing(LPVOID ThreadData)
{
    UpdateThreadData *utd = (UpdateThreadData *)ThreadData;
    PdfEngine engine;
    fz_buffer *filedata = NULL;

    filedata = fz_newbuffer(utd->len);
    filedata->len = utd->len;
    memcpy(filedata->data, utd->data, utd->len);

    fz_stream *stm = fz_openbuffer(filedata);
    bool success = engine.load(stm);
    fz_close(stm);
    if (!success)
        goto Error;
    fz_obj *info = engine.getPdfInfo();

    if (WaitForSingleObject(utd->produce, FILTER_TIMEOUT_IN_MS) != WAIT_OBJECT_0)
        goto Error;
    str_printf_s(utd->data, utd->len, "Type:document");
    SetEvent(utd->consume);

    if (WaitForSingleObject(utd->produce, FILTER_TIMEOUT_IN_MS) != WAIT_OBJECT_0)
        goto Error;
    char *author = pdf_toutf8(fz_dictgets(info, "Author"));
    str_printf_s(utd->data, utd->len, "Author:%s", author);
    free(author);
    SetEvent(utd->consume);

    if (WaitForSingleObject(utd->produce, FILTER_TIMEOUT_IN_MS) != WAIT_OBJECT_0)
        goto Error;
    char *title = pdf_toutf8(fz_dictgets(info, "Title"));
    if (str_empty(title)) {
        free(title);
        title = pdf_toutf8(fz_dictgets(info, "Subject"));
    }
    str_printf_s(utd->data, utd->len, "Title:%s", title);
    free(title);
    SetEvent(utd->consume);

    if (WaitForSingleObject(utd->produce, FILTER_TIMEOUT_IN_MS) != WAIT_OBJECT_0)
        goto Error;
    char *date = pdf_toutf8(fz_dictgets(info, "ModDate"));
    if (str_empty(date)) {
        free(date);
        date = pdf_toutf8(fz_dictgets(info, "CreationDate"));
    }
    str_printf_s(utd->data, utd->len, "Date:%s", date);
    free(date);
    SetEvent(utd->consume);

    for (int pageNo = 1; pageNo <= engine.pageCount(); pageNo++) {
        TCHAR *content = engine.ExtractPageText(pageNo);
        char *contentUTF8 = tstr_to_utf8(content);
        if (WaitForSingleObject(utd->produce, FILTER_TIMEOUT_IN_MS) != WAIT_OBJECT_0) {
            free(content);
            free(contentUTF8);
            goto Error;
        }
        str_printf_s(utd->data, utd->len, "Content:%s", contentUTF8);
        SetEvent(utd->consume);
        free(contentUTF8);
        free(content);
    }

    if (WaitForSingleObject(utd->produce, FILTER_TIMEOUT_IN_MS) != WAIT_OBJECT_0)
        goto Error;
    *utd->data = '\0';
    SetEvent(utd->consume);

    fz_dropbuffer(filedata);

    return 0;

Error:
    *utd->data = '\0';
    SetEvent(utd->consume);
    if (filedata)
        fz_dropbuffer(filedata);
    return 1;
}
