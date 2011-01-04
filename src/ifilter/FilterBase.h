// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include <strsafe.h>
#include <shlwapi.h>
#include <propkey.h>
#include <propsys.h>
#include <filter.h>
#include <filterr.h>
#include "PdfFilter.h"

// This is a class which simplifies both chunk and property value pair logic
// To use, you simply create a ChunkValue class of the right kind
// Example:
//      CChunkValue chunk;
//      hr = chunk.SetBoolValue(PKEY_IsAttachment, true);
//      or
//      hr = chunk.SetFileTimeValue(PKEY_ItemDate, ftLastModified);
class CChunkValue
{
public:
    CChunkValue() : m_fIsValid(false), m_pszValue(NULL)
    {
        PropVariantInit(&m_propVariant);
        Clear();
    }

    ~CChunkValue() { Clear(); };

    void Clear()
    {
        m_fIsValid = false;
        ZeroMemory(&m_chunk, sizeof(m_chunk));
        PropVariantClear(&m_propVariant);
        CoTaskMemFree(m_pszValue);
        m_pszValue = NULL;
    }

    BOOL IsValid() { return m_fIsValid; }


    HRESULT GetValue(PROPVARIANT **ppPropVariant)
    {
        if (!ppPropVariant)
            return E_INVALIDARG;

        *ppPropVariant = NULL;

        PROPVARIANT *pPropVariant = static_cast<PROPVARIANT*>(CoTaskMemAlloc(sizeof(PROPVARIANT)));
        if (!pPropVariant)
            return E_OUTOFMEMORY;

        HRESULT hr = PropVariantCopy(pPropVariant, &m_propVariant);
        if (SUCCEEDED(hr))
            *ppPropVariant = pPropVariant;
        else
            CoTaskMemFree(pPropVariant);

        return hr;
    }

    PWSTR GetString() { return m_pszValue; };

    HRESULT CopyChunk(STAT_CHUNK *pStatChunk)
    {
        if (!pStatChunk)
            return E_INVALIDARG;

        *pStatChunk = m_chunk;
        return S_OK;
    }

    CHUNKSTATE GetChunkType() { return m_chunk.flags; }

    HRESULT SetTextValue(REFPROPERTYKEY pkey, PCWSTR pszValue, CHUNKSTATE chunkType = CHUNK_VALUE,
                         LCID locale = 0, DWORD cwcLenSource = 0, DWORD cwcStartSource = 0,
                         CHUNK_BREAKTYPE chunkBreakType = CHUNK_NO_BREAK)
    {
        if (pszValue == NULL)
            return E_INVALIDARG;

        HRESULT hr = SetChunk(pkey, chunkType, locale, cwcLenSource, cwcStartSource, chunkBreakType);
        if (FAILED(hr))
            return hr;

        size_t cch = wcslen(pszValue) + 1;
        PWSTR pszCoTaskValue = static_cast<PWSTR>(CoTaskMemAlloc(cch * sizeof(WCHAR)));
        if (!pszCoTaskValue)
            return E_OUTOFMEMORY;

        StringCchCopy(pszCoTaskValue, cch, pszValue);
        m_fIsValid = true;
        if (chunkType == CHUNK_VALUE)
        {
            m_propVariant.vt = VT_LPWSTR;
            m_propVariant.pwszVal = pszCoTaskValue;
        }
        else
        {
            m_pszValue = pszCoTaskValue;
        }

        return hr;
    };

    HRESULT SetFileTimeValue(REFPROPERTYKEY pkey, FILETIME dtVal, CHUNKSTATE chunkType = CHUNK_VALUE,
                             LCID locale = 0, DWORD cwcLenSource = 0, DWORD cwcStartSource = 0,
                             CHUNK_BREAKTYPE chunkBreakType = CHUNK_NO_BREAK)
    {
        HRESULT hr = SetChunk(pkey, chunkType, locale, cwcLenSource, cwcStartSource, chunkBreakType);
        if (FAILED(hr))
            return hr;

        m_propVariant.vt = VT_FILETIME;
        m_propVariant.filetime = dtVal;
        m_fIsValid = true;

        return hr;
    };

protected:
    HRESULT SetChunk(REFPROPERTYKEY pkey, CHUNKSTATE chunkType=CHUNK_VALUE, LCID locale=0, DWORD cwcLenSource=0, DWORD cwcStartSource=0, CHUNK_BREAKTYPE chunkBreakType=CHUNK_NO_BREAK);

private:
    bool m_fIsValid;
    STAT_CHUNK  m_chunk;
    PROPVARIANT m_propVariant;
    PWSTR m_pszValue;

};

inline HRESULT CChunkValue::SetChunk(REFPROPERTYKEY pkey,
                                     CHUNKSTATE chunkType/*=CHUNK_VALUE*/,
                                     LCID locale /*=0*/,
                                     DWORD cwcLenSource /*=0*/,
                                     DWORD cwcStartSource /*=0*/,
                                     CHUNK_BREAKTYPE chunkBreakType /*= CHUNK_NO_BREAK */)
{
    Clear();

    m_chunk.attribute.psProperty.ulKind = PRSPEC_PROPID;
    m_chunk.attribute.psProperty.propid = pkey.pid;
    m_chunk.attribute.guidPropSet = pkey.fmtid;
    m_chunk.flags = chunkType;
    m_chunk.locale = locale;
    m_chunk.cwcLenSource = cwcLenSource;
    m_chunk.cwcStartSource = cwcStartSource;
    m_chunk.breakType = chunkBreakType;

    return S_OK;
}

class CFilterBase : public IFilter, public IInitializeWithStream, public IPersistStream, public IPersistFile
{
public:
    // OnInit() is called after the IStream is valid
    virtual HRESULT OnInit() = 0;

    // When GetNextChunkValue() is called you should fill in the ChunkValue by calling SetXXXValue() with the property.
    // example:  chunkValue.SetTextValue(PKYE_ItemName,L"blah de blah");
    // return FILTER_E_END_OF_CHUNKS when there are no more chunks
    virtual HRESULT GetNextChunkValue(CChunkValue &chunkValue) = 0;

protected:
    inline DWORD GetChunkId() const { return m_dwChunkId; }

public:
    CFilterBase() : m_dwChunkId(0), m_iText(0), m_pStream(NULL) { }

    virtual ~CFilterBase()
    {
        if (m_pStream)
            m_pStream->Release();
    }

    // IFilter
    IFACEMETHODIMP Init(ULONG grfFlags, ULONG cAttributes, const FULLPROPSPEC *aAttributes, ULONG *pFlags);
    IFACEMETHODIMP GetChunk(STAT_CHUNK *pStat);
    IFACEMETHODIMP GetText(ULONG *pcwcBuffer, WCHAR *awcBuffer);
    IFACEMETHODIMP GetValue(PROPVARIANT **ppPropValue);
    IFACEMETHODIMP BindRegion(FILTERREGION, REFIID, void **) { return E_NOTIMPL; }

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream *pStm, DWORD grfMode)
    {
        if (m_pStream)
            m_pStream->Release();
        m_pStream = pStm;
        m_pStream->AddRef();

        return OnInit();  // derived class inits now
    };

    // IPersist
    IFACEMETHODIMP GetClassID(CLSID *pClassID) {
        return CLSIDFromString(SZ_PDF_FILTER_HANDLER, pClassID);
    }
    // IPersistStream
    IFACEMETHODIMP IsDirty(void) { return E_NOTIMPL; }
    IFACEMETHODIMP Load(IStream *pStm) { return Initialize(pStm, 0); }
    IFACEMETHODIMP Save(IStream *pStm, BOOL fClearDirty) { return E_NOTIMPL; }
    IFACEMETHODIMP GetSizeMax(ULARGE_INTEGER *pcbSize) { return E_NOTIMPL; }

    // IPersistFile
    IFACEMETHODIMP Load(LPCOLESTR pszFileName, DWORD dwMode) {
        HANDLE hFile = CreateFileW(pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
            return E_INVALIDARG;
        DWORD size = GetFileSize(hFile, NULL), read;
        HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, size);
        ReadFile(hFile, GlobalLock(data), size, &read, NULL);
        GlobalUnlock(data);
        CloseHandle(hFile);

        IStream *pStm;
        if (FAILED(CreateStreamOnHGlobal(data, TRUE, &pStm)))
            return E_FAIL;
        HRESULT res = Initialize(pStm, 0);
        pStm->Release();
        return res;
    }
    IFACEMETHODIMP Save(LPCOLESTR pszFileName, BOOL bRemember) { return E_NOTIMPL; }
    IFACEMETHODIMP SaveCompleted(LPCOLESTR pszFileName) { return E_NOTIMPL; }
    IFACEMETHODIMP GetCurFile(LPOLESTR *ppszFileName) { return E_NOTIMPL; }

protected:
    IStream*                    m_pStream;

private:
    DWORD                       m_dwChunkId;
    DWORD                       m_iText;

    CChunkValue                 m_currentChunk;
};
