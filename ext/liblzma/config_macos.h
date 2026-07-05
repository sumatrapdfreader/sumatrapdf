/* config.h for building bundled liblzma on macOS (decoder subset). */
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDBOOL_H 1

#define SIZEOF_SIZE_T 8

#define MYTHREAD_POSIX 1

#define HAVE_DECODERS 1
#define HAVE_DECODER_LZMA1 1
#define HAVE_DECODER_LZMA2 1
#define HAVE_DECODER_DELTA 1
#define HAVE_DECODER_X86 1

#define HAVE_CHECK_CRC32 1
#define HAVE_CHECK_CRC64 1

#define TUKLIB_FAST_UNALIGNED_ACCESS 1