/* Ppmd8.h -- PPMdI codec
2011-01-27 : Igor Pavlov : Public domain
This code is based on:
  PPMd var.I (2002): Dmitry Shkarin : Public domain
  Carryless rangecoder (1999): Dmitry Subbotin : Public domain */

#ifndef ARCHIVE_PPMD8_PRIVATE_H_INCLUDED
#define ARCHIVE_PPMD8_PRIVATE_H_INCLUDED

#include "archive_ppmd_private.h"

#define PPMD8_MIN_ORDER 2
#define PPMD8_MAX_ORDER 16

struct CPpmd8_Context_;

typedef
  #ifdef PPMD_32BIT
    struct CPpmd8_Context_ *
  #else
    UInt32
  #endif
  CPpmd8_Context_Ref;

#pragma pack(push, 1)

typedef struct CPpmd8_Context_
{
  Byte NumStats;
  Byte Flags;
  UInt16 SummFreq;
  CPpmd_State_Ref Stats;
  CPpmd8_Context_Ref Suffix;
} CPpmd8_Context;

#pragma pack(pop)

#define Ppmd8Context_OneState(p) ((CPpmd_State *)&(p)->SummFreq)

/* The BUG in Shkarin's code for FREEZE mode was fixed, but that fixed
   code is not compatible with original code for some files compressed
   in FREEZE mode. So we disable FREEZE mode support. */

enum
{
  PPMD8_RESTORE_METHOD_RESTART,
  PPMD8_RESTORE_METHOD_CUT_OFF
  #ifdef PPMD8_FREEZE_SUPPORT
  , PPMD8_RESTORE_METHOD_FREEZE
  #endif
};

typedef struct
{
  CPpmd8_Context *MinContext, *MaxContext;
  CPpmd_State *FoundState;
  unsigned OrderFall, InitEsc, PrevSuccess, MaxOrder;
  Int32 RunLength, InitRL; /* must be 32-bit at least */

  UInt32 Size;
  UInt32 GlueCount;
  Byte *Base, *LoUnit, *HiUnit, *Text, *UnitsStart;
  UInt32 AlignOffset;
  unsigned RestoreMethod;

  /* Range Coder */
  UInt32 Range;
  UInt32 Code;
  UInt32 Low;
  union
  {
    IByteIn *In;
    IByteOut *Out;
  } Stream;

  Byte Indx2Units[PPMD_NUM_INDEXES];
  Byte Units2Indx[128];
  CPpmd_Void_Ref FreeList[PPMD_NUM_INDEXES];
  UInt32 Stamps[PPMD_NUM_INDEXES];

  Byte NS2BSIndx[256], NS2Indx[260];
  CPpmd_See DummySee, See[24][32];
  UInt16 BinSumm[25][64];
} CPpmd8;


/* ---------- Internal Functions ---------- */

extern const Byte PPMD8_kExpEscape[16];

#ifdef PPMD_32BIT
  #define Ppmd8_GetPtr(p, ptr) (ptr)
  #define Ppmd8_GetContext(p, ptr) (ptr)
  #define Ppmd8_GetStats(p, ctx) ((ctx)->Stats)
#else
  #define Ppmd8_GetPtr(p, offs) ((void *)((p)->Base + (offs)))
  #define Ppmd8_GetContext(p, offs) ((CPpmd8_Context *)Ppmd8_GetPtr((p), (offs)))
  #define Ppmd8_GetStats(p, ctx) ((CPpmd_State *)Ppmd8_GetPtr((p), ((ctx)->Stats)))
#endif

#define Ppmd8_GetBinSumm(p) \
    &p->BinSumm[p->NS2Indx[Ppmd8Context_OneState(p->MinContext)->Freq - 1]][ \
    p->NS2BSIndx[Ppmd8_GetContext(p, p->MinContext->Suffix)->NumStats] + \
    p->PrevSuccess + p->MinContext->Flags + ((p->RunLength >> 26) & 0x20)]


typedef struct
{
  /* Base Functions */
  void (*Ppmd8_Construct)(CPpmd8 *p);
  Bool (*Ppmd8_Alloc)(CPpmd8 *p, UInt32 size);
  void (*Ppmd8_Free)(CPpmd8 *p);
  void (*Ppmd8_Init)(CPpmd8 *p, unsigned max_order, unsigned restore_method);
  #define Ppmd7_WasAllocated(p) ((p)->Base != NULL)

  /* Decode Functions */
  int (*Ppmd8_RangeDec_Init)(CPpmd8 *p);
  int (*Ppmd8_DecodeSymbol)(CPpmd8 *p);
} IPpmd8;

extern const IPpmd8 __archive_ppmd8_functions;

#endif
