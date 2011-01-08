/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <windows.h>
#include <shlwapi.h>
#include <sddl.h>
#include <tchar.h>
#include <inttypes.h>
#include "tstr_util.h"
#include "CPdfFilter.h"

#ifdef IFILTER_BUILTIN_MUPDF
#include "PdfEngine.h"
#include "ExtHelpers.h"

TCHAR *GetPasswordForFile(WindowInfo *win, const TCHAR *fileName, pdf_xref *xref, unsigned char *decryptionKey, bool *saveKey)
{
    return NULL;
}
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
    TCHAR *uniqueName = tstr_printf(_T("SumatraPDF-%ul-%s"), size, uniqueStr);
    RpcStringFree(&uniqueStr);

    // allow all access to the Local System (and for debugging also to any authenticated user)
    // to the global objects we share with the UpdateMMapForIndexing thread
    SECURITY_ATTRIBUTES sa = { 0 };
    sa.nLength = sizeof(sa);
    if (!ConvertStringSecurityDescriptorToSecurityDescriptor(_T("D:P(A;OICI;GA;;;SY)(A;OICI;GA;;;AU)"),
        SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL))
        return E_FAIL;

    m_hMap = CreateFileMapping(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, size, uniqueName);
    if (m_hMap)
        m_pData = (char *)MapViewOfFile(m_hMap, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!m_pData)
        goto ErrorFail;

    TCHAR eventName[EVENT_NAME_SIZE_MAX];
    tstr_printf_s(eventName, EVENT_NAME_SIZE_MAX, _T("Produce-%s"), uniqueName);
    m_hProduce = CreateEvent(&sa, FALSE, TRUE, eventName);
    tstr_printf_s(eventName, EVENT_NAME_SIZE_MAX, _T("Consume-%s"), uniqueName);
    m_hConsume = CreateEvent(&sa, FALSE, FALSE, eventName);
    if (!m_hProduce || !m_hConsume)
        goto ErrorFail;

    LARGE_INTEGER zero;
    zero.QuadPart = 0;
    m_pStream->Seek(zero, STREAM_SEEK_SET, NULL);
    res = m_pStream->Read(m_pData, size, NULL);
    if (FAILED(res))
        goto ErrorFail;

#ifndef IFILTER_BUILTIN_MUPDF
    TCHAR exePath[MAX_PATH];
    GetModuleFileName(g_hInstance, exePath, MAX_PATH);
    *PathFindFileName(exePath) = '\0';
    tstr_cat_s(exePath, MAX_PATH, _T("SumatraPDF.exe"));
    if (!PathFileExists(exePath))
        goto ErrorFail;

    TCHAR cmdline[MAX_PATH * 2];
    tstr_printf_s(cmdline, MAX_PATH * 2, _T("\"%s\" -ifiltermmap %s"), exePath, uniqueName);
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    // TODO: This might not be possible at all, as SearchFilterHost.exe runs in a
    //       low-privilege environment which might prevent the creation of new threads.
    //       Currently, it fails with ERROR_NOT_ENOUGH_QUOTA.
    if (!CreateProcess(NULL, cmdline, &sa, &sa, FALSE, 0, NULL, NULL, &si, &pi))
        goto ErrorFail;
    CloseHandle(pi.hThread);
    LocalFree(sa.lpSecurityDescriptor);
    m_hProcess = pi.hProcess;
#else
    m_pUniqueName = tstr_dup(uniqueName);
    m_hProcess = CreateThread(NULL, 0, UpdateMMapForIndexing, m_pUniqueName, 0, NULL);
#endif

    return S_OK;

ErrorFail:
    LocalFree(sa.lpSecurityDescriptor);
    return E_FAIL;
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
    chunkValue.Clear();

    if (m_bDone)
        return FILTER_E_END_OF_CHUNKS;
    if (WaitForSingleObject(m_hConsume, FILTER_TIMEOUT_IN_MS) != WAIT_OBJECT_0)
        m_bDone = true;
    if (!*m_pData)
        m_bDone = true;
    if (m_bDone)
        return FILTER_E_END_OF_CHUNKS;

    if (str_startswith(m_pData, "Type:") && m_pData[5]) {
        WCHAR *type = utf8_to_wstr(m_pData + 5);
        chunkValue.SetTextValue(PKEY_PerceivedType, type);
        free(type);
        goto Success;
    }

    if (str_startswith(m_pData, "Author:") && m_pData[7]) {
        WCHAR *author = utf8_to_wstr(m_pData + 7);
        chunkValue.SetTextValue(PKEY_Author, author);
        free(author);
        goto Success;
    }

    if (str_startswith(m_pData, "Title:") && m_pData[6]) {
        WCHAR *title = utf8_to_wstr(m_pData + 6);
        chunkValue.SetTextValue(PKEY_Title, title);
        free(title);
        goto Success;
    }

    if (str_startswith(m_pData, "Date:") && PdfDateParse(m_pData + 5, &systime)) {
        FILETIME filetime;
        SystemTimeToFileTime(&systime, &filetime);
        chunkValue.SetFileTimeValue(PKEY_ItemDate, filetime);
        goto Success;
    }

    if (str_startswith(m_pData, "Content:")) {
        WCHAR *content = utf8_to_wstr(m_pData + 8);
        chunkValue.SetTextValue(PKEY_Search_Contents, content, CHUNK_TEXT);
        free(content);
        goto Success;
    }

    // if (!str_endswith(m_pData, ":"))
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
    if (pFlags)
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
