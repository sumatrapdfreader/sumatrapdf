#ifndef _BZLIB_H
#define _BZLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#define BZ_RUN               0
#define BZ_FLUSH             1
#define BZ_FINISH            2

#define BZ_OK                0
#define BZ_RUN_OK            1
#define BZ_FLUSH_OK          2
#define BZ_FINISH_OK         3
#define BZ_STREAM_END        4
#define BZ_SEQUENCE_ERROR    (-1)
#define BZ_PARAM_ERROR       (-2)
#define BZ_MEM_ERROR         (-3)
#define BZ_DATA_ERROR        (-4)
#define BZ_DATA_ERROR_MAGIC  (-5)
#define BZ_IO_ERROR          (-6)
#define BZ_UNEXPECTED_EOF    (-7)
#define BZ_OUTBUFF_FULL      (-8)
#define BZ_CONFIG_ERROR      (-9)

typedef
   struct {
      char *next_in;
      unsigned int avail_in;
      unsigned int total_in_lo32;
      unsigned int total_in_hi32;

      char *next_out;
      unsigned int avail_out;
      unsigned int total_out_lo32;
      unsigned int total_out_hi32;

      void *state;

      void *(*bzalloc)(void *,int,int);
      void (*bzfree)(void *,void *);
      void *opaque;
   }
   bz_stream;

#ifndef BZ_IMPORT
#define BZ_EXPORT
#endif

#ifndef BZ_NO_STDIO

#include <stdio.h>
#endif

#ifdef _WIN32
#   include <windows.h>
#   ifdef small

#      undef small
#   endif
#   ifdef BZ_EXPORT
#   define BZ_API(func) WINAPI func
#   define BZ_EXTERN extern
#   else

#   define BZ_API(func) (WINAPI * func)
#   define BZ_EXTERN
#   endif
#else
#   define BZ_API(func) func
#   define BZ_EXTERN extern
#endif

BZ_EXTERN int BZ_API(BZ2_bzCompressInit) (
      bz_stream* strm,
      int        blockSize100k,
      int        verbosity,
      int        workFactor
   );

BZ_EXTERN int BZ_API(BZ2_bzCompress) (
      bz_stream* strm,
      int action
   );

BZ_EXTERN int BZ_API(BZ2_bzCompressEnd) (
      bz_stream* strm
   );

BZ_EXTERN int BZ_API(BZ2_bzDecompressInit) (
      bz_stream *strm,
      int       verbosity,
      int       small
   );

BZ_EXTERN int BZ_API(BZ2_bzDecompress) (
      bz_stream* strm
   );

BZ_EXTERN int BZ_API(BZ2_bzDecompressEnd) (
      bz_stream *strm
   );

#ifndef BZ_NO_STDIO
#define BZ_MAX_UNUSED 5000

typedef void BZFILE;

BZ_EXTERN BZFILE* BZ_API(BZ2_bzReadOpen) (
      int*  bzerror,
      FILE* f,
      int   verbosity,
      int   small,
      void* unused,
      int   nUnused
   );

BZ_EXTERN void BZ_API(BZ2_bzReadClose) (
      int*    bzerror,
      BZFILE* b
   );

BZ_EXTERN void BZ_API(BZ2_bzReadGetUnused) (
      int*    bzerror,
      BZFILE* b,
      void**  unused,
      int*    nUnused
   );

BZ_EXTERN int BZ_API(BZ2_bzRead) (
      int*    bzerror,
      BZFILE* b,
      void*   buf,
      int     len
   );

BZ_EXTERN BZFILE* BZ_API(BZ2_bzWriteOpen) (
      int*  bzerror,
      FILE* f,
      int   blockSize100k,
      int   verbosity,
      int   workFactor
   );

BZ_EXTERN void BZ_API(BZ2_bzWrite) (
      int*    bzerror,
      BZFILE* b,
      void*   buf,
      int     len
   );

BZ_EXTERN void BZ_API(BZ2_bzWriteClose) (
      int*          bzerror,
      BZFILE*       b,
      int           abandon,
      unsigned int* nbytes_in,
      unsigned int* nbytes_out
   );

BZ_EXTERN void BZ_API(BZ2_bzWriteClose64) (
      int*          bzerror,
      BZFILE*       b,
      int           abandon,
      unsigned int* nbytes_in_lo32,
      unsigned int* nbytes_in_hi32,
      unsigned int* nbytes_out_lo32,
      unsigned int* nbytes_out_hi32
   );
#endif

BZ_EXTERN int BZ_API(BZ2_bzBuffToBuffCompress) (
      char*         dest,
      unsigned int* destLen,
      char*         source,
      unsigned int  sourceLen,
      int           blockSize100k,
      int           verbosity,
      int           workFactor
   );

BZ_EXTERN int BZ_API(BZ2_bzBuffToBuffDecompress) (
      char*         dest,
      unsigned int* destLen,
      char*         source,
      unsigned int  sourceLen,
      int           small,
      int           verbosity
   );

BZ_EXTERN const char * BZ_API(BZ2_bzlibVersion) (
      void
   );

#ifndef BZ_NO_STDIO
BZ_EXTERN BZFILE * BZ_API(BZ2_bzopen) (
      const char *path,
      const char *mode
   );

BZ_EXTERN BZFILE * BZ_API(BZ2_bzdopen) (
      int        fd,
      const char *mode
   );

BZ_EXTERN int BZ_API(BZ2_bzread) (
      BZFILE* b,
      void* buf,
      int len
   );

BZ_EXTERN int BZ_API(BZ2_bzwrite) (
      BZFILE* b,
      void*   buf,
      int     len
   );

BZ_EXTERN int BZ_API(BZ2_bzflush) (
      BZFILE* b
   );

BZ_EXTERN void BZ_API(BZ2_bzclose) (
      BZFILE* b
   );

BZ_EXTERN const char * BZ_API(BZ2_bzerror) (
      BZFILE *b,
      int    *errnum
   );
#endif

#ifdef __cplusplus
}
#endif

#endif
