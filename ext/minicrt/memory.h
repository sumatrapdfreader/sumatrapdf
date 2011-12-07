#ifndef TR_COMMON_MINICRT_MEMORY_H_
#define TR_COMMON_MINICRT_MEMORY_H_

int __cdecl memcmp(const void * buf1, const void * buf2, size_t count);
void * __cdecl memcpy(void * dst, const void * src, size_t count);
void * __cdecl memset(void *dst, int val, size_t count);

#endif  // TR_COMMON_MINICRT_MEMORY_H_
