/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <windows.h>
#include <shlwapi.h>
#include <tchar.h>
#include <inttypes.h>
#include "wstr_util.h"
#include "CPdfFilter.h"

#ifdef IFILTER_BUILTIN_MUPDF
#include "PdfEngine.h"
#include "ExtHelpers.h"

TCHAR *GetPasswordForFile(WindowInfo *win, const TCHAR *fileName, pdf_xref *xref, unsigned char *decryptionKey, bool *saveKey)
{
    return NULL;
}

struct MMapHandles {
    TCHAR id[4];
    HANDLE hProduce, hConsume, hMMap;
    ULONG size;
};
#endif

#ifdef _MSC_VER
#pragma warning(disable: 4995)
#endif

extern HINSTANCE g_hInstance;

HRESULT CPdfFilter::OnInit()
{
    CleanUp();

    STATSTG stat;
    HRESULT res = m_pStream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(res))
        return res;
    ULONG size = stat.cbSize.LowPart;

    GUID unique;
    UuidCreate(&unique);
    RPC_WSTR uniqueStr;
    UuidToString(&unique, &uniqueStr);
    TCHAR uniqueName[72];
    wsprintf(uniqueName, _T("SumatraPDF-%ul-%s"), size, uniqueStr);
    RpcStringFree(&uniqueStr);

    // TODO: create a DACL for the file mapping and the events,
    //       else they can't be opened by another process/thread
    m_hMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, uniqueName);
    if (!m_hMap)
        return E_OUTOFMEMORY;
    m_pData = (char *)MapViewOfFile(m_hMap, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!m_pData)
        return E_OUTOFMEMORY;

    LARGE_INTEGER zero = { 0 };
    m_pStream->Seek(zero, STREAM_SEEK_SET, NULL);
    res = m_pStream->Read(m_pData, size, NULL);
    if (FAILED(res))
        return E_FAIL;

    TCHAR eventName[96];
    wsprintf(eventName, _T("%s-Produce"), uniqueName);
    m_hProduce = CreateEvent(NULL, FALSE, TRUE, eventName);
    wsprintf(eventName, _T("%s-Consume"), uniqueName);
    m_hConsume = CreateEvent(NULL, FALSE, FALSE, eventName);
    if (!m_hProduce || !m_hConsume)
        return E_FAIL;

#ifndef IFILTER_BUILTIN_MUPDF
    TCHAR exePath[MAX_PATH], args[MAX_PATH];
    GetModuleFileName(g_hInstance, exePath, MAX_PATH);
    _tcscpy(PathFindFileName(exePath), _T("SumatraPDF.exe"));
    if (!PathFileExists(exePath))
        return E_FAIL;

    SHELLEXECUTEINFO sei = { 0 };
    sei.cbSize = sizeof(sei);
#ifndef SEE_MASK_NOZONECHECKS
#define SEE_MASK_NOZONECHECKS 0x00800000
#endif
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOZONECHECKS;
    sei.lpVerb = _T("open");
    sei.lpFile = exePath;
    wsprintf((LPTSTR)(sei.lpParameters = args), _T("-ifiltermmap %s"), uniqueName);
    sei.nShow = SW_HIDE;
    sei.hInstApp = g_hInstance;
    if (!ShellExecuteEx(&sei) || !sei.hProcess || (int)sei.hInstApp <= 32)
        return E_FAIL;
    m_hProcess = sei.hProcess;
#else
    m_pHandleInfo = malloc(sizeof(struct MMapHandles));
    struct MMapHandles *mmh = (struct MMapHandles *)m_pHandleInfo;
    lstrcpy(mmh->id, _T("MMH"));
    mmh->hProduce = m_hProduce;
    mmh->hConsume = m_hConsume;
    mmh->hMMap = m_hMap;
    mmh->size = size;
    m_hProcess = CreateThread(NULL, 0, UpdateMMapForIndexing, m_pHandleInfo, 0, NULL);
#endif

    return S_OK;
}

// adapted from SumatraProperties.cpp
static bool PdfDateParse(char *pdfDate, SYSTEMTIME *timeOut) {
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    // "D:" at the beginning is optional
    if (pdfDate[0] == 'D' && pdfDate[1] == ':')
        pdfDate += 2;
    return 6 == sscanf(pdfDate, "%4d%2d%2d" "%2d%2d%2d",
        &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay,
        &timeOut->wHour, &timeOut->wMinute, &timeOut->wSecond);
    // don't bother about the day of week, we won't display it anyway
}

HRESULT CPdfFilter::GetNextChunkValue(CChunkValue &chunkValue)
{
    SYSTEMTIME systime;
    chunkValue.Clear();

    if (m_bDone)
        return FILTER_E_END_OF_CHUNKS;
    if (WaitForSingleObject(m_hConsume, 1000) != WAIT_OBJECT_0)
        m_bDone = true;
    if (!*m_pData)
        m_bDone = true;
    if (m_bDone)
        return FILTER_E_END_OF_CHUNKS;

    if (!strncmp(m_pData, "Type:", 5) && m_pData[5]) {
        WCHAR *type = utf8_to_wstr(m_pData + 5);
        chunkValue.SetTextValue(PKEY_PerceivedType, type);
        free(type);
        goto Success;
    }

    if (!strncmp(m_pData, "Author:", 7) && m_pData[7]) {
        WCHAR *author = utf8_to_wstr(m_pData + 7);
        chunkValue.SetTextValue(PKEY_Author, author);
        free(author);
        goto Success;
    }

    if (!strncmp(m_pData, "Title:", 6) && m_pData[6]) {
        WCHAR *title = utf8_to_wstr(m_pData + 6);
        chunkValue.SetTextValue(PKEY_Title, title);
        free(title);
        goto Success;
    }

    if (!strncmp(m_pData, "Date:", 5) && PdfDateParse(m_pData + 5, &systime)) {
        FILETIME filetime;
        SystemTimeToFileTime(&systime, &filetime);
        chunkValue.SetFileTimeValue(PKEY_ItemDate, filetime);
        goto Success;
    }

    if (!strncmp(m_pData, "Content:", 8)) {
        WCHAR *content = utf8_to_wstr(m_pData + 8);
        chunkValue.SetTextValue(PKEY_Search_Contents, content, CHUNK_TEXT, 0, 0, 0, CHUNK_EOW);
        free(content);
        goto Success;
    }

    // if (m_pData[strlen(m_pData)-1] != ':')
    //     fprintf(stderr, "Unexpected data: %s\n", m_pData);

    *m_pData = '\0';
    SetEvent(m_hProduce);
    return GetNextChunkValue(chunkValue);

Success:
    *m_pData = '\0';
    SetEvent(m_hProduce);
    return S_OK;
}



HRESULT CFilterBase::Init(ULONG grfFlags, ULONG cAttributes, const FULLPROPSPEC *aAttributes, ULONG *pFlags)
{
    if (cAttributes > 0 && !aAttributes)
        return E_INVALIDARG;

    m_dwChunkId = 0;
    m_iText = 0;
    m_currentChunk.Clear();
    *pFlags = 0;

    return OnInit();
}

HRESULT CFilterBase::GetChunk(STAT_CHUNK *pStat)
{
    HRESULT hr = S_FALSE;
    int cIterations = 0;

    while (S_FALSE == hr && cIterations < 256)
    {
        pStat->idChunk = m_dwChunkId;
        hr = GetNextChunkValue(m_currentChunk);
        ++cIterations;
    }

    if (hr == S_OK)
    {
        if (!m_currentChunk.IsValid())
             return E_INVALIDARG;

        m_iText = 0;
        m_currentChunk.CopyChunk(pStat);
        pStat->idChunk = ++m_dwChunkId;
        if (pStat->flags == CHUNK_TEXT)
            pStat->idChunkSource = pStat->idChunk;
    }

    return hr;
}

HRESULT CFilterBase::GetText(ULONG *pcwcBuffer, WCHAR *awcBuffer)
{
    if (!pcwcBuffer || !*pcwcBuffer)
        return E_INVALIDARG;

    if (!m_currentChunk.IsValid())
        return FILTER_E_NO_MORE_TEXT;

    if (m_currentChunk.GetChunkType() != CHUNK_TEXT)
        return FILTER_E_NO_TEXT;

    ULONG cchTotal = static_cast<ULONG>(wcslen(m_currentChunk.GetString()));
    ULONG cchLeft = cchTotal - m_iText;
    ULONG cchToCopy = min(*pcwcBuffer - 1, cchLeft);

    if (!cchToCopy)
        return FILTER_E_NO_MORE_TEXT;

    PCWSTR psz = m_currentChunk.GetString() + m_iText;
    StringCchCopyN(awcBuffer, *pcwcBuffer, psz, cchToCopy);
    awcBuffer[cchToCopy] = '\0';

    *pcwcBuffer = cchToCopy;
    m_iText += cchToCopy;
    cchLeft -= cchToCopy;

    if (!cchLeft)
        return FILTER_S_LAST_TEXT;

    return S_OK;
}

HRESULT CFilterBase::GetValue(PROPVARIANT **ppPropValue)
{
    if (!m_currentChunk.IsValid())
        return FILTER_E_NO_MORE_VALUES;

    if (m_currentChunk.GetChunkType() != CHUNK_VALUE)
        return FILTER_E_NO_VALUES;

    if (ppPropValue == NULL)
        return E_INVALIDARG;

    HRESULT hr = m_currentChunk.GetValue(ppPropValue);
    m_currentChunk.Clear();

    return hr;
}
