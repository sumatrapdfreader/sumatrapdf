/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <windows.h>
#include <shlwapi.h>
#include <tchar.h>
#include <inttypes.h>
#include "wstr_util.h"
#include "CPdfFilter.h"

#ifdef _MSC_VER
#pragma warning(disable: 4995)
#endif

extern HINSTANCE g_hInstance;

#define MMAP_HEADER_SIZE 64

HRESULT CPdfFilter::OnInit()
{
    STATSTG stat;
    HRESULT res = m_pStream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(res))
        return res;

    ULONG size = stat.cbSize.LowPart;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    m_hMap = CreateFileMapping(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, size + MMAP_HEADER_SIZE, NULL);
    if (!m_hMap)
        return E_OUTOFMEMORY;
    m_pData = (char *)MapViewOfFile(m_hMap, FILE_MAP_ALL_ACCESS, 0, 0, size + MMAP_HEADER_SIZE);
    if (!m_pData)
        return E_OUTOFMEMORY;

    sprintf(m_pData, "IFilterMMap 1.3 %ul", size);
    char *out = m_pData + strlen(m_pData) + 1;
    LARGE_INTEGER zero = { 0 };
    m_pStream->Seek(zero, STREAM_SEEK_SET, NULL);
    res = m_pStream->Read(out, size, NULL);
    if (FAILED(res))
        return E_FAIL;

    TCHAR cmdline[MAX_PATH * 2];
    _tcscpy(cmdline,_T( "\""));
    GetModuleFileName(g_hInstance, cmdline + 1, MAX_PATH);
    _tcscpy(PathFindFileName(cmdline + 1), _T("SumatraPDF.exe"));
    if (!PathFileExists(cmdline + 1))
        return E_FAIL;

    PROCESS_INFORMATION pi;
    STARTUPINFO si = { 0 };
    si.cb = sizeof(si);
    _stprintf(cmdline + lstrlen(cmdline), _T("\" -ifiltermmap %ul"), m_hMap);
    if (!CreateProcess(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
        return E_FAIL;
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);

    if (!*(int32_t *)m_pData || strcmp(m_pData, "IFilterMMap 1.3") != 0)
        return E_FAIL;
    m_pSection = m_pData;

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

    m_pSection += strlen(m_pSection) + 1;
    if (!*m_pSection)
        return FILTER_E_END_OF_CHUNKS;

    if (!strncmp(m_pSection, "Author:", 7) && m_pSection[7]) {
        WCHAR *author = utf8_to_wstr(m_pSection + 7);
        chunkValue.SetTextValue(PKEY_Author, author);
        free(author);
        return S_OK;
    }

    if (!strncmp(m_pSection, "Title:", 6) && m_pSection[6]) {
        WCHAR *title = utf8_to_wstr(m_pSection + 6);
        chunkValue.SetTextValue(PKEY_Title, title);
        free(title);
        return S_OK;
    }

    if (!strncmp(m_pSection, "Date:", 5) && PdfDateParse(m_pSection + 5, &systime)) {
        FILETIME filetime;
        SystemTimeToFileTime(&systime, &filetime);
        chunkValue.SetFileTimeValue(PKEY_ItemDate, filetime);
        return S_OK;
    }

    if (!strncmp(m_pSection, "Content:", 8)) {
        WCHAR *content = utf8_to_wstr(m_pSection + 8);
        chunkValue.SetTextValue(PKEY_Search_Contents, content, CHUNK_TEXT);
        free(content);
        return S_OK;
    }

    // if (m_pSection[strlen(m_pSection)-1] != ':')
    //     fprintf(stderr, "Unexpected data: %s\n", m_pSection);
    return GetNextChunkValue(chunkValue);
}



HRESULT CFilterBase::Init(ULONG grfFlags, ULONG cAttributes, const FULLPROPSPEC *aAttributes, ULONG *pFlags)
{
    m_dwChunkId = 0;
    m_iText = 0;
    m_currentChunk.Clear();
    *pFlags = 0;

    if (m_pStream)
        return OnInit();
    return S_OK;
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

        m_currentChunk.CopyChunk(pStat);
        pStat->idChunk = ++m_dwChunkId;
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
    if (m_currentChunk.GetChunkType() != CHUNK_VALUE)
        return FILTER_E_NO_MORE_VALUES;

    if (ppPropValue == NULL)
        return E_INVALIDARG;

    if (!m_currentChunk.IsValid())
        return FILTER_E_NO_MORE_VALUES;

    HRESULT hr = m_currentChunk.GetValue(ppPropValue);
    m_currentChunk.Clear();

    return hr;
}
