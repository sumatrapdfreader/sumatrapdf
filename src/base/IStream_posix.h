/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef IStreamPosix_h
#define IStreamPosix_h

using HRESULT = int32_t;
using ULONG = uint32_t;

struct LARGE_INTEGER {
    int64_t QuadPart = 0;
};

struct ULARGE_INTEGER {
    uint64_t QuadPart = 0;
};

struct STATSTG {
    ULARGE_INTEGER cbSize;
};

struct IStream {
    HRESULT QueryInterface(const void*, void**);
    ULONG AddRef();
    ULONG Release();
    HRESULT Read(void* pv, ULONG cb, ULONG* pcbRead);
    HRESULT Write(const void* pv, ULONG cb, ULONG* pcbWritten);
    HRESULT Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition);
    HRESULT SetSize(ULARGE_INTEGER libNewSize);
    HRESULT CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten);
    HRESULT Commit(DWORD grfCommitFlags);
    HRESULT Revert();
    HRESULT LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
    HRESULT UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
    HRESULT Stat(STATSTG* pstatstg, DWORD grfStatFlag);
    HRESULT Clone(IStream** ppstm);
};

constexpr HRESULT S_OK = 0;
constexpr HRESULT E_NOTIMPL = (HRESULT)0x80004001;
constexpr HRESULT E_FAIL = (HRESULT)0x80004005;
constexpr int STREAM_SEEK_SET = 0;
constexpr int STATFLAG_NONAME = 1;

#define FAILED(hr) ((HRESULT)(hr) < 0)
#define SUCCEEDED(hr) ((HRESULT)(hr) >= 0)

inline HRESULT IStream::QueryInterface(const void*, void**) {
    ReportIf(true);
    return E_NOTIMPL;
}

inline ULONG IStream::AddRef() {
    ReportIf(true);
    return 0;
}

inline ULONG IStream::Release() {
    ReportIf(true);
    return 0;
}

inline HRESULT IStream::Read(void*, ULONG, ULONG*) {
    ReportIf(true);
    return E_NOTIMPL;
}

inline HRESULT IStream::Write(const void*, ULONG, ULONG*) {
    ReportIf(true);
    return E_NOTIMPL;
}

inline HRESULT IStream::Seek(LARGE_INTEGER, DWORD, ULARGE_INTEGER*) {
    ReportIf(true);
    return E_NOTIMPL;
}

inline HRESULT IStream::SetSize(ULARGE_INTEGER) {
    ReportIf(true);
    return E_NOTIMPL;
}

inline HRESULT IStream::CopyTo(IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*) {
    ReportIf(true);
    return E_NOTIMPL;
}

inline HRESULT IStream::Commit(DWORD) {
    ReportIf(true);
    return E_NOTIMPL;
}

inline HRESULT IStream::Revert() {
    ReportIf(true);
    return E_NOTIMPL;
}

inline HRESULT IStream::LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) {
    ReportIf(true);
    return E_NOTIMPL;
}

inline HRESULT IStream::UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) {
    ReportIf(true);
    return E_NOTIMPL;
}

inline HRESULT IStream::Stat(STATSTG*, DWORD) {
    ReportIf(true);
    return E_NOTIMPL;
}

inline HRESULT IStream::Clone(IStream**) {
    ReportIf(true);
    return E_NOTIMPL;
}

#endif
