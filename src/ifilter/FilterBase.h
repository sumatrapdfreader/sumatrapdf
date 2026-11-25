// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#ifndef __FILTERBASE_H__
#define __FILTERBASE_H__

#include <propkey.h>
#include <propsys.h>
#include <filter.h>
#include <filterr.h>

class ChunkValue {
  public:
    ChunkValue() : m_fIsValid(false), m_pszValue(nullptr), m_cchValue(0) {
        PropVariantInit(&m_propVariant);
        Clear();
    }
    ChunkValue(const ChunkValue&) = delete;
    ChunkValue& operator=(const ChunkValue&) = delete;

    ~ChunkValue() { Clear(); };

    void Clear() {
        m_fIsValid = false;
        ZeroMemory(&m_chunk, sizeof(m_chunk));
        PropVariantClear(&m_propVariant);
        CoTaskMemFree(m_pszValue);
        m_pszValue = nullptr;
        m_cchValue = 0;
    }

    BOOL IsValid() { return m_fIsValid; }

    HRESULT GetValue(PROPVARIANT** ppPropVariant) {
        if (!ppPropVariant) return E_INVALIDARG;

        *ppPropVariant = nullptr;

        PROPVARIANT* pPropVariant = static_cast<PROPVARIANT*>(CoTaskMemAlloc(sizeof(PROPVARIANT)));
        if (!pPropVariant) return E_OUTOFMEMORY;

        HRESULT hr = PropVariantCopy(pPropVariant, &m_propVariant);
        if (SUCCEEDED(hr))
            *ppPropVariant = pPropVariant;
        else
            CoTaskMemFree(pPropVariant);

        return hr;
    }

    PWSTR GetString() { return m_pszValue; }
    size_t GetStringLen() const { return m_cchValue; }

    HRESULT CopyChunk(STAT_CHUNK* pStatChunk) {
        if (!pStatChunk) return E_INVALIDARG;

        *pStatChunk = m_chunk;
        return S_OK;
    }

    CHUNKSTATE GetChunkType() { return m_chunk.flags; }

    HRESULT SetTextValue(REFPROPERTYKEY pkey, PCWSTR pszValue, CHUNKSTATE chunkType = CHUNK_VALUE, LCID locale = 0,
                         DWORD cwcLenSource = 0, DWORD cwcStartSource = 0,
                         CHUNK_BREAKTYPE chunkBreakType = CHUNK_NO_BREAK) {
        if (pszValue == nullptr) return E_INVALIDARG;

        HRESULT hr = SetChunk(pkey, chunkType, locale, cwcLenSource, cwcStartSource, chunkBreakType);
        if (FAILED(hr)) return hr;

        size_t cch = wcslen(pszValue);
        if (cch >= SIZE_MAX / sizeof(WCHAR)) return E_OUTOFMEMORY;
        PWSTR pszCoTaskValue = static_cast<PWSTR>(CoTaskMemAlloc((cch + 1) * sizeof(WCHAR)));
        if (!pszCoTaskValue) return E_OUTOFMEMORY;

        memcpy(pszCoTaskValue, pszValue, (cch + 1) * sizeof(WCHAR));
        m_fIsValid = true;
        if (chunkType == CHUNK_VALUE) {
            m_propVariant.vt = VT_LPWSTR;
            m_propVariant.pwszVal = pszCoTaskValue;
        } else {
            m_pszValue = pszCoTaskValue;
            m_cchValue = cch;
        }

        return hr;
    };

    HRESULT SetFileTimeValue(REFPROPERTYKEY pkey, FILETIME dtVal, CHUNKSTATE chunkType = CHUNK_VALUE, LCID locale = 0,
                             DWORD cwcLenSource = 0, DWORD cwcStartSource = 0,
                             CHUNK_BREAKTYPE chunkBreakType = CHUNK_NO_BREAK) {
        HRESULT hr = SetChunk(pkey, chunkType, locale, cwcLenSource, cwcStartSource, chunkBreakType);
        if (FAILED(hr)) return hr;

        m_propVariant.vt = VT_FILETIME;
        m_propVariant.filetime = dtVal;
        m_fIsValid = true;

        return hr;
    };

  protected:
    HRESULT SetChunk(REFPROPERTYKEY pkey, CHUNKSTATE chunkType = CHUNK_VALUE, LCID locale = 0, DWORD cwcLenSource = 0,
                     DWORD cwcStartSource = 0, CHUNK_BREAKTYPE chunkBreakType = CHUNK_NO_BREAK);

  private:
    bool m_fIsValid;
    STAT_CHUNK m_chunk;
    PROPVARIANT m_propVariant;
    PWSTR m_pszValue;
    size_t m_cchValue;
};

inline HRESULT ChunkValue::SetChunk(REFPROPERTYKEY pkey, CHUNKSTATE chunkType /*=CHUNK_VALUE*/, LCID locale /*=0*/,
                                    DWORD cwcLenSource /*=0*/, DWORD cwcStartSource /*=0*/,
                                    CHUNK_BREAKTYPE chunkBreakType /*= CHUNK_NO_BREAK */) {
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

class FilterBase : public IFilter, public IInitializeWithStream, public IPersistStream, public IPersistFile {
  public:
    // OnInit() is called when the IFilter is initialized (at the end of IFilter::Init)
    virtual HRESULT OnInit() = 0;

    // When GetNextChunkValue() is called you should fill in the ChunkValue by calling SetXXXValue() with the property.
    // example:  chunkValue.SetTextValue(PKYE_ItemName, L"foo bar");
    // return FILTER_E_END_OF_CHUNKS when there are no more chunks
    virtual HRESULT GetNextChunkValue(ChunkValue& chunkValue) = 0;

    FilterBase(AtomicInt* plRefCount) : m_lRef(1), m_plModuleRef(plRefCount), m_dwChunkId(0), m_iText(0) {
        AtomicIntInc(m_plModuleRef);
    }

    virtual ~FilterBase() {
        str::Free(m_data);
        AtomicIntDec(m_plModuleRef);
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        static const QITAB qit[] = {QITABENT(FilterBase, IPersistStream),
                                    QITABENT(FilterBase, IPersistFile),
                                    QITABENTMULTI(FilterBase, IPersist, IPersistStream),
                                    QITABENT(FilterBase, IInitializeWithStream),
                                    QITABENT(FilterBase, IFilter),
                                    {}};
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() { return AtomicIntInc(&m_lRef); }
    IFACEMETHODIMP_(ULONG) Release() {
        int cRef = AtomicIntDec(&m_lRef);
        if (cRef == 0) delete this;
        return cRef;
    }

    // IFilter
    IFACEMETHODIMP Init(__unused ULONG grfFlags, ULONG cAttributes, const FULLPROPSPEC* aAttributes, ULONG* pFlags) {
        if (cAttributes > 0 && !aAttributes) return E_INVALIDARG;

        m_dwChunkId = 0;
        m_iText = 0;
        m_currentChunk.Clear();
        if (pFlags) *pFlags = 0;

        return OnInit();
    }

    IFACEMETHODIMP GetChunk(STAT_CHUNK* pStat) {
        if (!pStat) return E_INVALIDARG;
        // indexer hosts live long; temp data never outlives a call
        ResetTempArena();
        m_currentChunk.Clear();
        HRESULT hr = GetNextChunkValue(m_currentChunk);
        if (hr != S_OK) return hr;
        if (!m_currentChunk.IsValid()) return E_INVALIDARG;

        m_iText = 0;
        m_currentChunk.CopyChunk(pStat);
        pStat->idChunk = ++m_dwChunkId;
        if (pStat->flags == CHUNK_TEXT) pStat->idChunkSource = pStat->idChunk;

        return hr;
    }

    IFACEMETHODIMP GetText(ULONG* pcwcBuffer, WCHAR* awcBuffer) {
        if (!pcwcBuffer || !awcBuffer || *pcwcBuffer < 2) return E_INVALIDARG;
        if (!m_currentChunk.IsValid()) return FILTER_E_NO_MORE_TEXT;
        if (m_currentChunk.GetChunkType() != CHUNK_TEXT) return FILTER_E_NO_TEXT;

        size_t cchTotal = m_currentChunk.GetStringLen();
        if (m_iText >= cchTotal) return FILTER_E_NO_MORE_TEXT;
        size_t cchLeft = cchTotal - m_iText;
        ULONG cchToCopy = (ULONG)std::min<size_t>((size_t)(*pcwcBuffer - 1), cchLeft);

        PCWSTR psz = m_currentChunk.GetString() + m_iText;
        memcpy(awcBuffer, psz, (size_t)cchToCopy * sizeof(WCHAR));
        awcBuffer[cchToCopy] = 0;

        *pcwcBuffer = cchToCopy;
        m_iText += cchToCopy;
        cchLeft -= cchToCopy;

        if (!cchLeft) return FILTER_S_LAST_TEXT;

        return S_OK;
    }

    IFACEMETHODIMP GetValue(PROPVARIANT** ppPropValue) {
        if (!m_currentChunk.IsValid()) return FILTER_E_NO_MORE_VALUES;
        if (m_currentChunk.GetChunkType() != CHUNK_VALUE) return FILTER_E_NO_VALUES;
        if (ppPropValue == nullptr) return E_INVALIDARG;

        HRESULT hr = m_currentChunk.GetValue(ppPropValue);
        m_currentChunk.Clear();

        return hr;
    }
    IFACEMETHODIMP BindRegion(FILTERREGION, REFIID, void**) { return E_NOTIMPL; }

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream* pStm, __unused DWORD grfMode) {
        if (!pStm) return E_INVALIDARG;
        // The shell hands us a deny-write stream, so keeping it alive locks the
        // file for as long as this filter lives and an editor can't write over
        // it while it's being indexed (issue #1530). Every filter reads the
        // whole thing anyway, so read it here and let go of the file.
        str::FreePtr(&m_data);
        m_data = ReadIStream(pStm);
        return str::IsNull(m_data) ? E_FAIL : S_OK;
    };

    // IPersistStream
    IFACEMETHODIMP IsDirty() { return E_NOTIMPL; }
    IFACEMETHODIMP Load(IStream* pStm) { return Initialize(pStm, 0); }
    IFACEMETHODIMP Save(__unused IStream* pStm, __unused BOOL fClearDirty) { return E_NOTIMPL; }
    IFACEMETHODIMP GetSizeMax(__unused ULARGE_INTEGER* pcbSize) { return E_NOTIMPL; }

    // IPersistFile (for compatibility with older Windows Desktop Search versions and ifilttst.exe)
    IFACEMETHODIMP Load(LPCOLESTR pszFileName, __unused DWORD dwMode) {
        if (!pszFileName) return E_INVALIDARG;
        // share everything: we only read, and denying writers would lock the
        // file for the read the same way holding the shell's stream did
        DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
        HANDLE hFile =
            CreateFileW(pszFileName, GENERIC_READ, share, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return E_INVALIDARG;
        // Str holds an int length, so files of 2 GB and more can't be loaded
        LARGE_INTEGER fileSize{};
        if (!GetFileSizeEx(hFile, &fileSize)) {
            HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
            CloseHandle(hFile);
            return hr;
        }
        if (fileSize.QuadPart >= INT_MAX) {
            CloseHandle(hFile);
            return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
        }
        DWORD size = (DWORD)fileSize.QuadPart;
        char* data = AllocArray<char>((int)size + 1);
        if (!data) {
            CloseHandle(hFile);
            return E_OUTOFMEMORY;
        }
        DWORD totalRead = 0;
        BOOL ok = TRUE;
        while (totalRead < size) {
            DWORD read = 0;
            ok = ReadFile(hFile, data + totalRead, size - totalRead, &read, nullptr);
            if (!ok || read == 0) {
                break;
            }
            totalRead += read;
        }
        CloseHandle(hFile);
        if (!ok || totalRead != size) {
            free(data);
            return E_FAIL;
        }
        str::FreePtr(&m_data);
        m_data = Str(data, (int)size);
        return S_OK;
    }
    IFACEMETHODIMP Save(__unused LPCOLESTR pszFileName, __unused BOOL bRemember) { return E_NOTIMPL; }
    IFACEMETHODIMP SaveCompleted(__unused LPCOLESTR pszFileName) { return E_NOTIMPL; }
    IFACEMETHODIMP GetCurFile(__unused LPOLESTR* ppszFileName) { return E_NOTIMPL; }

  protected:
    // the file's bytes, owned; filters read them in OnInit(), which can run
    // again if IFilter::Init() is called for another pass
    Str m_data;

  private:
    AtomicInt m_lRef;
    AtomicInt* m_plModuleRef;

    DWORD m_dwChunkId;
    DWORD m_iText;

    ChunkValue m_currentChunk;
};

#endif
