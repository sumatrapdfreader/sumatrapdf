#ifndef _RAR_DEFS_
#define _RAR_DEFS_

#define  Min(x,y) (((x)<(y)) ? (x):(y))
#define  Max(x,y) (((x)>(y)) ? (x):(y))

#define  ASIZE(x) (sizeof(x)/sizeof(x[0]))

// MAXPASSWORD is expected to be multiple of CRYPTPROTECTMEMORY_BLOCK_SIZE (16)
// for CryptProtectMemory in SecPassword.
#define  MAXPASSWORD       128

#define  MAXSFXSIZE        0x100000

#define  DefSFXName        L"default.sfx"
#define  DefSortListName   L"rarfiles.lst"


#ifndef SFX_MODULE
#define USE_QOPEN
#endif

// Suppress GCC warn_unused_result warning in -O2 mode
// for those function calls where we do not need it.
#define ignore_result(x) if (x)

// Produce the value, which is larger than 'v' and aligned to 'a'.
#define ALIGN_VALUE(v,a) (size_t(v) + ( (~size_t(v) + 1) & (a - 1) ) )

#endif
