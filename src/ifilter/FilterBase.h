// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#ifndef __FILTERBASE_H__
#define __FILTERBASE_H__

#include <strsafe.h>
#include <propkey.h>
#include <propsys.h>
#include <filter.h>
#include <filterr.h>

class ChunkValue
{
public:
    ChunkValue() : m_fIsValid(false), m_pszValue(nullptr)
    {
        PropVariantInit(&m_propVariant);
        Clear();
    }

    ~ChunkValue() { Clear(); };

    void Clear()
    {
        m_fIsValid = false;
        ZeroMemory(&m_chunk, sizeof(m_chunk));
        PropVariantClear(&m_propVariant);
        CoTaskMemFree(m_pszValue);
        m_pszValue = nullptr;
    }

    BOOL IsValid() { return m_fIsValid; }

    HRESULT GetValue(PROPVARIANT **ppPropVariant)
    {
        if (!ppPropVariant)
            return E_INVALIDARG;

        *ppPropVariant = nullptr;

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
        if (pszValue == nullptr)
            return E_INVALIDARG;

        HRESULT hr = SetChunk(pkey, chunkType, locale, cwcLenSource, cwcStartSource, chunkBreakType);
        if (FAILED(hr))
            return hr;

        size_t cch = wcslen(pszValue) + 1;
        PWSTR pszCoTaskValue = static_cast<PWSTR>(CoTaskMemAlloc(cch * sizeof(WCHAR)));
        if (!pszCoTaskValue)
            return E_OUTOFMEMORY;

        StringCchCopyW(pszCoTaskValue, cch, pszValue);
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

inline HRESULT ChunkValue::SetChunk(REFPROPERTYKEY pkey,
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

class FilterBase : public IFilter, public IInitializeWithStream, public IPersistStream, public IPersistFile
{
public:
    // OnInit() is called when the IFilter is initialized (at the end of IFilter::Init)
    virtual HRESULT OnInit() = 0;

    // When GetNextChunkValue() is called you should fill in the ChunkValue by calling SetXXXValue() with the property.
    // example:  chunkValue.SetTextValue(PKYE_ItemName, L"foo bar");
    // return FILTER_E_END_OF_CHUNKS when there are no more chunks
    virtual HRESULT GetNextChunkValue(ChunkValue &chunkValue) = 0;

protected:
    inline DWORD GetChunkId() const { return m_dwChunkId; }

public:
    FilterBase(long *plRefCount) : m_lRef(1), m_plModuleRef(plRefCount),
        m_dwChunkId(0), m_iText(0), m_pStream(nullptr) {
        InterlockedIncrement(m_plModuleRef);
    }

    virtual ~FilterBase() {
        if (m_pStream)
            m_pStream->Release();
        InterlockedDecrement(m_plModuleRef);
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) {
        static const QITAB qit[] = {
            QITABENT(FilterBase, IPersistStream),
            QITABENT(FilterBase, IPersistFile),
            QITABENTMULTI(FilterBase, IPersist, IPersistStream),
            QITABENT(FilterBase, IInitializeWithStream),
            QITABENT(FilterBase, IFilter),
            { 0 }
        };
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() {
        return InterlockedIncrement(&m_lRef);
    }
    IFACEMETHODIMP_(ULONG) Release() {
        long cRef = InterlockedDecrement(&m_lRef);
        if (cRef == 0)
            delete this;
        return cRef;
    }

    // IFilter
    IFACEMETHODIMP Init(__unused ULONG grfFlags, ULONG cAttributes, const FULLPROPSPEC *aAttributes, ULONG *pFlags) {
        if (cAttributes > 0 && !aAttributes)
            return E_INVALIDARG;

        m_dwChunkId = 0;
        m_iText = 0;
        m_currentChunk.Clear();
        if (pFlags)
            *pFlags = 0;

        return OnInit();
    }

    IFACEMETHODIMP GetChunk(STAT_CHUNK *pStat) {
        m_currentChunk.Clear();
        HRESULT hr = GetNextChunkValue(m_currentChunk);
        if (hr != S_OK)
            return hr;
        if (!m_currentChunk.IsValid())
             return E_INVALIDARG;

        m_iText = 0;
        m_currentChunk.CopyChunk(pStat);
        pStat->idChunk = ++m_dwChunkId;
        if (pStat->flags == CHUNK_TEXT)
            pStat->idChunkSource = pStat->idChunk;

        return hr;
    }

    IFACEMETHODIMP GetText(ULONG *pcwcBuffer, WCHAR *awcBuffer) {
        if (!pcwcBuffer || !*pcwcBuffer)
            return E_INVALIDARG;
        if (!m_currentChunk.IsValid())
            return FILTER_E_NO_MORE_TEXT;
        if (m_currentChunk.GetChunkType() != CHUNK_TEXT)
            return FILTER_E_NO_TEXT;

        ULONG cchTotal = static_cast<ULONG>(wcslen(m_currentChunk.GetString()));
        ULONG cchLeft = cchTotal - m_iText;
        ULONG cchToCopy = std::min(*pcwcBuffer - 1, cchLeft);

        if (!cchToCopy)
            return FILTER_E_NO_MORE_TEXT;

        PCWSTR psz = m_currentChunk.GetString() + m_iText;
        StringCchCopyNW(awcBuffer, *pcwcBuffer, psz, cchToCopy);
        awcBuffer[cchToCopy] = '\0';

        *pcwcBuffer = cchToCopy;
        m_iText += cchToCopy;
        cchLeft -= cchToCopy;

        if (!cchLeft)
            return FILTER_S_LAST_TEXT;

        return S_OK;
    }

    IFACEMETHODIMP GetValue(PROPVARIANT **ppPropValue) {
        if (!m_currentChunk.IsValid())
            return FILTER_E_NO_MORE_VALUES;
        if (m_currentChunk.GetChunkType() != CHUNK_VALUE)
            return FILTER_E_NO_VALUES;
        if (ppPropValue == nullptr)
            return E_INVALIDARG;

        HRESULT hr = m_currentChunk.GetValue(ppPropValue);
        m_currentChunk.Clear();

        return hr;
    }
    IFACEMETHODIMP BindRegion(FILTERREGION, REFIID, void **) { return E_NOTIMPL; }

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream *pStm, __unused DWORD grfMode) {
        if (m_pStream)
            m_pStream->Release();
        m_pStream = pStm;
        if (!m_pStream)
            return E_INVALIDARG;
        m_pStream->AddRef();
        return S_OK;
    };

    // IPersistStream
    IFACEMETHODIMP IsDirty() { return E_NOTIMPL; }
    IFACEMETHODIMP Load(IStream *pStm) { return Initialize(pStm, 0); }
    IFACEMETHODIMP Save(__unused IStream *pStm, __unused BOOL fClearDirty) { return E_NOTIMPL; }
    IFACEMETHODIMP GetSizeMax(__unused ULARGE_INTEGER *pcbSize) { return E_NOTIMPL; }

    // IPersistFile (for compatibility with older Windows Desktop Search versions and ifilttst.exe)
    IFACEMETHODIMP Load(LPCOLESTR pszFileName, __unused DWORD dwMode) {
        HANDLE hFile = CreateFileW(pszFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return E_INVALIDARG;
        DWORD size = GetFileSize(hFile, nullptr), read;
        HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!data) {
            CloseHandle(hFile);
            return E_OUTOFMEMORY;
        }
        BOOL ok = ReadFile(hFile, GlobalLock(data), size, &read, nullptr);
        GlobalUnlock(data);
        CloseHandle(hFile);

        IStream *pStm;
        if (!ok || FAILED(CreateStreamOnHGlobal(data, TRUE, &pStm))) {
            GlobalFree(data);
            return E_FAIL;
        }
        HRESULT res = Initialize(pStm, 0);
        pStm->Release();
        return res;
    }
    IFACEMETHODIMP Save(__unused LPCOLESTR pszFileName, __unused BOOL bRemember) { return E_NOTIMPL; }
    IFACEMETHODIMP SaveCompleted(__unused LPCOLESTR pszFileName) { return E_NOTIMPL; }
    IFACEMETHODIMP GetCurFile(__unused LPOLESTR *ppszFileName) { return E_NOTIMPL; }

protected:
    IStream*                    m_pStream;

private:
    long                        m_lRef, * m_plModuleRef;

    DWORD                       m_dwChunkId;
    DWORD                       m_iText;

    ChunkValue                 m_currentChunk;
};

#endif
