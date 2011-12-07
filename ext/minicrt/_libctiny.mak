#==================================================
# LIBCTINY - Matt Pietrek 1996
# Microsoft Systems Journal, October 1996
# FILE: LIBCTINY.MAK - Makefile for Microsoft version
#==================================================
CC = CL
CC_OPTIONS = /c /W3 /DWIN32_LEAN_AND_MEAN /Gy /GR- /GX- /GF

S=.
!ifdef DEBUG
CC_OPTIONS = $(CC_OPTIONS) /Zi
O=Debug
!else
CC_OPTIONS = $(CC_OPTIONS) /Zi /Ogisyb2
O=Release
!endif

PROJ = LIBCTINY

OBJS =  $O\CRT0TCON.OBJ $O\CRT0TWIN.OBJ $O\DLLCRT0.OBJ $O\ARGCARGV.OBJ $O\PRINTF.OBJ \
        $O\SPRINTF.OBJ $O\PUTS.OBJ $O\ALLOC.OBJ $O\ALLOC2.OBJ $O\ALLOCSUP.OBJ $O\STRUPLWR.OBJ \
        $O\ISCTYPE.OBJ $O\ATOL.OBJ $O\STRICMP.OBJ $O\NEWDEL.OBJ $O\INITTERM.OBJ

all: $O $O\$(PROJ).LIB

$O: ; mkdir $O

$O\$(PROJ).LIB: {$O}$(OBJS)
    LIB /OUT:$O\$(PROJ).LIB $(OBJS)

{$S}.CPP{$O}.OBJ::
    $(CC) $(CC_OPTIONS) -Fo$O\ -Fd$O\ $<
    
{$O}$(OBJS):
