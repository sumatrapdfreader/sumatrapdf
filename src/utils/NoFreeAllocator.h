/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef NoFreeAllocator_h
#define NoFreeAllocator_h

#include "BaseUtil.h"

namespace nf {

// Per-thread stats for no-free allocator. Knowing them allows
// tweaking the sizes of memory blocks and frequency of
// AllocatorMark placement.
struct AllocStats {
    size_t  allocsCount;
    size_t  maxMemUse;
    uint64  totalMemAlloced;
};

class AllocatorMark {
    // NULL is valid and has a special meaning: it means the first
    // MemBlock. It allows us to delay allocating first MemBlock to
    // the moment of first nf::alloc() calls (useful for threads that
    // might but not necessarily will allocate)
    struct MemBlock *   block;
    // position within block
    size_t              pos;
    // block can be NULL in more than one object but we need to
    // know which one was the very first (it has to free all memory)
    // We keep track of it with isFirst and we know that because only
    // the first allocates gStats
    bool                isFirst;
public:
    AllocatorMark();
    ~AllocatorMark();
};

void *alloc(size_t size);
void *memdup(const void *ptr, size_t size);

namespace str {

inline char *Dup(const char *s) { return (char *)nf::memdup(s, strlen(s) + 1); }
inline WCHAR *Dup(const WCHAR *s) { return (WCHAR *)nf::memdup(s, (wcslen(s) + 1) * sizeof(WCHAR)); }

char *  ToMultiByte(const WCHAR *txt, UINT CodePage);
char *  ToMultiByte(const char *src, UINT CodePageSrc, UINT CodePageDest);
WCHAR * ToWideChar(const char *src, UINT CodePage);

namespace conv {

#ifdef UNICODE
inline TCHAR *  FromWStr(const WCHAR *src) { return Dup(src); }
inline WCHAR *  ToWStr(const TCHAR *src) { return Dup(src); }
inline TCHAR *  FromCodePage(const char *src, UINT cp) { return ToWideChar(src, cp); }
inline char *   ToCodePage(const TCHAR *src, UINT cp) { return ToMultiByte(src, cp); }
#else
inline TCHAR *  FromWStr(const WCHAR *src) { return ToMultiByte(src, CP_ACP); }
inline WCHAR *  ToWStr(const TCHAR *src) { return ToWideChar(src, CP_ACP); }
inline TCHAR *  FromCodePage(const char *src, UINT cp) { return ToMultiByte(src, cp, CP_ACP); }
inline char *   ToCodePage(const TCHAR *src, UINT cp) { return ToMultiByte(src, CP_ACP, cp); }
#endif
inline TCHAR *  FromUtf8(const char *src) { return FromCodePage(src, CP_UTF8); }
inline char *   ToUtf8(const TCHAR *src) { return ToCodePage(src, CP_UTF8); }
inline TCHAR *  FromAnsi(const char *src) { return FromCodePage(src, CP_ACP); }
inline char *   ToAnsi(const TCHAR *src) { return ToCodePage(src, CP_ACP); }

} // namespace conv
} // namespace str

} // namespace nf

#endif
