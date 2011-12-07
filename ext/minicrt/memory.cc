
#include "libctiny.h"
#include <windows.h>
#include <errno.h>
#include "memory.h"

// memmove is defined first so it will use the intrinsic memcpy
void * __cdecl memmove(void * dst, const void * src, size_t count) {
  void * ret = dst;

  if (dst <= src || (char *)dst >= ((char *)src + count)) {
    // Non-Overlapping Buffers - copy from lower addresses to higher addresses
    // saves 500 bytes of 1.4MB in uncompressed setup.exe, worth it?
    memcpy(dst, src, count);
    // while (count--) {
    //     *(char *)dst = *(char *)src;
    //     dst = (char *)dst + 1;
    //     src = (char *)src + 1;
    // }
  }
  else {
    // Overlapping Buffers - copy from higher addresses to lower addresses
    dst = (char *)dst + count - 1;
    src = (char *)src + count - 1;

    while (count--) {
      *(char *)dst = *(char *)src;
      dst = (char *)dst - 1;
      src = (char *)src - 1;
    }
  }

  return(ret);
}

// Turn off compiler intrinsics so that we can define these functions
#pragma function(memcmp, memcpy, memset)

int __cdecl memcmp(const void * buf1, const void * buf2, size_t count) {
  if (!count)
    return(0);
  while (--count && *(char *)buf1 == *(char *)buf2) {
    buf1 = (char *)buf1 + 1;
    buf2 = (char *)buf2 + 1;
  }
  return( *((unsigned char *)buf1) - *((unsigned char *)buf2) );
}

void * __cdecl memcpy(void * dst, const void * src, size_t count) {
  void * ret = dst;
  // copy from lower addresses to higher addresses
  while (count--) {
    *(char *)dst = *(char *)src;
    dst = (char *)dst + 1;
    src = (char *)src + 1;
  }
  return(ret);
}

void * __cdecl memset(void *dst, int val, size_t count) {
  void *start = dst;
  while (count--) {
    *(char *)dst = (char)val;
    dst = (char *)dst + 1;
  }
  return(start);
}

errno_t __cdecl memmove_s(void* dst,
                          size_t size_in_bytes,
                          const void* src,
                          size_t count) {
  if (count == 0) {
      return 0;
  }

  if (!dst) return EINVAL;
  if (!src) return EINVAL;
  if (size_in_bytes < count) return ERANGE;

  memmove(dst, src, count);
  return 0;
}

errno_t __cdecl memcpy_s(void *dst,
                         size_t size_in_bytes,
                         const void *src,
                         size_t count) {
  if (count == 0) {
        return 0;
  }

  if (!dst) return EINVAL;
  if (!src) return EINVAL;
  if (size_in_bytes < count) return ERANGE;

  memcpy(dst, src, count);
  return 0;
}

