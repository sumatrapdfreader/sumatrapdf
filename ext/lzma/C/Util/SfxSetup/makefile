PROG = 7zS2.sfx
LIBS = $(LIBS) 
CFLAGS = $(CFLAGS) -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS

C_OBJS = \
  $O\7zAlloc.obj \
  $O\7zBuf.obj \
  $O\7zBuf2.obj \
  $O\7zCrc.obj \
  $O\7zCrcOpt.obj \
  $O\7zFile.obj \
  $O\7zDec.obj \
  $O\7zIn.obj \
  $O\7zStream.obj \
  $O\Bcj2.obj \
  $O\Bra.obj \
  $O\Bra86.obj \
  $O\CpuArch.obj \
  $O\Lzma2Dec.obj \
  $O\LzmaDec.obj \

7Z_OBJS = \
  $O\SfxSetup.obj \

OBJS = \
  $(7Z_OBJS) \
  $(C_OBJS) \
  $O\resource.res

!include "../../../CPP/Build.mak"

$(7Z_OBJS): $(*B).c
	$(COMPL_O1)
$(C_OBJS): ../../$(*B).c
	$(COMPL_O1)
