/*
This is a single-file implementation of Visual Studio crt
made in order to reduce the size of executables.

This is preliminary, I might change my mind or never
finish this code, but this is the plan:
* implementation is in a single file, to make it easy to integrate
  in Sumatra and other projects
* I'll start from scratch i.e. start with no code at all and
  add the necessary functions one by one, guided by what
  linker tells us is missing. This is to learn what is the absolutely
  minimum to include and review all the code that is included
* I'll reuse as much as possible from msvcrt.dll
* I'll use the code already written in omaha's minicrt project
  (but only after reviewing each function)
* the code that comes in *.obj files will have to be written
* other places that I might steal the code from:
 - http://llvm.org/svn/llvm-project/compiler-rt/trunk/
 - http://www.jbox.dk/sanos/source/
 - http://f4b24.googlecode.com/svn/trunk/extra/smartvc9/

More info:
* http://kobyk.wordpress.com/2007/07/20/dynamically-linking-with-msvcrtdll-using-visual-c-2005/
* http://adrianhenke.wordpress.com/2008/12/05/create-lib-file-from-dll/ - info on how to create
  .lib file from .def file
* http://www.ibsensoftware.com/download.html wcrt is another small C runtime library, not
   open source
* http://drdobbs.com/windows/184416623

TODO:
* get _alldiv() from ntdll.dll ?

* msvcrt.dll (c:\windows\system32\msvcrt.dll) might contain
  more functionality in later versions of windows. We have
  to make sure we don't use function that are not present in XP
  (by extracting list of symbols from msvcrt.dll on XP with
  dumpbin and not using anything that is not there)
*/


#if 0 // TODO: not sure what the right thing to do is: undefine, leave it alone?
#ifdef UNICODE
#undef UNICODE
#endif

#ifdef _UNICODE
#undef _UNICODE
#endif
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
//#include <crtdbg.h>

extern "C" {

/* definitions of functions implemented in msvcrt.dll */
typedef int (__cdecl *_PIFV)(void);
//void *  __cdecl malloc(size_t size);
//void    __cdecl free(void * p);
//void *  __cdecl calloc(size_t nitems, size_t size);
FILE *  __cdecl __p__iob(void);
//int __cdecl _initterm_e(_PIFV *, _PIFV *);
int _cdecl _initterm(_PIFV *, _PIFV *);

#pragma section(".CRT$XCA",long,read)
#pragma section(".CRT$XCZ",long,read)
#pragma section(".CRT$XIA",long,read)
#pragma section(".CRT$XIZ",long,read)
#pragma section(".CRT$XPA",long,read)
#pragma section(".CRT$XPZ",long,read)
#pragma section(".CRT$XTA",long,read)
#pragma section(".CRT$XTZ",long,read)

#pragma comment(linker, "/merge:.CRT=.rdata")

__declspec(allocate(".CRT$XIA")) _PIFV __xi_a[] = { 0 };
__declspec(allocate(".CRT$XIZ")) _PIFV __xi_z[] = { 0 }; /* C initializers */
__declspec(allocate(".CRT$XCA")) _PIFV __xc_a[] = { 0 };
__declspec(allocate(".CRT$XCZ")) _PIFV __xc_z[] = { 0 }; /* C++ initializers */
__declspec(allocate(".CRT$XPA")) _PIFV __xp_a[] = { 0 };
__declspec(allocate(".CRT$XPZ")) _PIFV __xp_z[] = { 0 }; /* C pre-terminators */
__declspec(allocate(".CRT$XTA")) _PIFV __xt_a[] = { 0 };
__declspec(allocate(".CRT$XTZ")) _PIFV __xt_z[] = { 0 }; /* C terminators */

void * __cdecl _malloc_dbg(size_t size, int nBlockUse, const char *file,int line)
{
	return malloc(size);
}

void __cdecl _free_dbg(void *data, int nBlockUse)
{
	free(data);
}

void * __cdecl _calloc_dbg(size_t nNum, size_t nSize, int nBlockUse, const char *file, int line)
{
	return calloc(nNum, nSize);
}

char * __cdecl _strdup_dbg(const char *s, int blockType, const char *file, int line)
{
	return _strdup(s);
}

wchar_t * __cdecl _wcsdup_dbg(const wchar_t *s, int blockType, const char *file, int line)
{
	return _wcsdup(s);
}

FILE * __cdecl __iob_func(void) {
	return __p__iob();
}

__declspec(naked) void __cdecl _alldiv(void)
{
    __asm {
        push    edi
        push    esi
        push    ebx

; Set up the local stack and save the index registers.  When this is done
; the stack frame will look as follows (assuming that the expression a/b will
; generate a call to lldiv(a, b)):
;
;               -----------------
;               |               |
;               |---------------|
;               |               |
;               |--divisor (b)--|
;               |               |
;               |---------------|
;               |               |
;               |--dividend (a)-|
;               |               |
;               |---------------|
;               | return addr** |
;               |---------------|
;               |      EDI      |
;               |---------------|
;               |      ESI      |
;               |---------------|
;       ESP---->|      EBX      |
;               -----------------
;

#define DVNDLO  [esp + 16]      /* stack address of dividend (a) loword */
#define DVNDHI  [esp + 20]      /* stack address of dividend (a) hiword */
#define DVSRLO  [esp + 24]      /* stack address of divisor (b) loword */
#define DVSRHI  [esp + 28]      /* stack address of divisor (b) hiword */


; Determine sign of the result (edi = 0 if result is positive, non-zero
; otherwise) and make operands positive.

        xor     edi,edi         ; result sign assumed positive

        mov     eax,DVNDHI      ; hi word of a
        or      eax,eax         ; test to see if signed
        jge     short L1        ; skip rest if a is already positive
        inc     edi             ; complement result sign flag
        mov     edx,DVNDLO      ; lo word of a
        neg     eax             ; make a positive
        neg     edx
        sbb     eax,0
        mov     DVNDHI,eax      ; save positive value
        mov     DVNDLO,edx
L1:
        mov     eax,DVSRHI      ; hi word of b
        or      eax,eax         ; test to see if signed
        jge     short L2        ; skip rest if b is already positive
        inc     edi             ; complement the result sign flag
        mov     edx,DVSRLO      ; lo word of a
        neg     eax             ; make b positive
        neg     edx
        sbb     eax,0
        mov     DVSRHI,eax      ; save positive value
        mov     DVSRLO,edx
L2:

;
; Now do the divide.  First look to see if the divisor is less than 4194304K.
; If so, then we can use a simple algorithm with word divides, otherwise
; things get a little more complex.
;
; NOTE - eax currently contains the high order word of DVSR
;

        or      eax,eax         ; check to see if divisor < 4194304K
        jnz     short L3        ; nope, gotta do this the hard way
        mov     ecx,DVSRLO      ; load divisor
        mov     eax,DVNDHI      ; load high word of dividend
        xor     edx,edx
        div     ecx             ; eax <- high order bits of quotient
        mov     ebx,eax         ; save high bits of quotient
        mov     eax,DVNDLO      ; edx:eax <- remainder:lo word of dividend
        div     ecx             ; eax <- low order bits of quotient
        mov     edx,ebx         ; edx:eax <- quotient
        jmp     short L4        ; set sign, restore stack and return

;
; Here we do it the hard way.  Remember, eax contains the high word of DVSR
;

L3:
        mov     ebx,eax         ; ebx:ecx <- divisor
        mov     ecx,DVSRLO
        mov     edx,DVNDHI      ; edx:eax <- dividend
        mov     eax,DVNDLO
L5:
        shr     ebx,1           ; shift divisor right one bit
        rcr     ecx,1
        shr     edx,1           ; shift dividend right one bit
        rcr     eax,1
        or      ebx,ebx
        jnz     short L5        ; loop until divisor < 4194304K
        div     ecx             ; now divide, ignore remainder
        mov     esi,eax         ; save quotient

;
; We may be off by one, so to check, we will multiply the quotient
; by the divisor and check the result against the orignal dividend
; Note that we must also check for overflow, which can occur if the
; dividend is close to 2**64 and the quotient is off by 1.
;

        mul     dword ptr DVSRHI ; QUOT * DVSRHI
        mov     ecx,eax
        mov     eax,DVSRLO
        mul     esi             ; QUOT * DVSRLO
        add     edx,ecx         ; EDX:EAX = QUOT * DVSR
        jc      short L6        ; carry means Quotient is off by 1

;
; do long compare here between original dividend and the result of the
; multiply in edx:eax.  If original is larger or equal, we are ok, otherwise
; subtract one (1) from the quotient.
;

        cmp     edx,DVNDHI      ; compare hi words of result and original
        ja      short L6        ; if result > original, do subtract
        jb      short L7        ; if result < original, we are ok
        cmp     eax,DVNDLO      ; hi words are equal, compare lo words
        jbe     short L7        ; if less or equal we are ok, else subtract
L6:
        dec     esi             ; subtract 1 from quotient
L7:
        xor     edx,edx         ; edx:eax <- quotient
        mov     eax,esi

;
; Just the cleanup left to do.  edx:eax contains the quotient.  Set the sign
; according to the save value, cleanup the stack, and return.
;

L4:
        dec     edi             ; check to see if result is negative
        jnz     short L8        ; if EDI == 0, result should be negative
        neg     edx             ; otherwise, negate the result
        neg     eax
        sbb     edx,0

;
; Restore the saved registers and return.
;

L8:
        pop     ebx
        pop     esi
        pop     edi

        ret     16
    }
}

// TODO: should it be 1? should it be defined in asm file? does it matter?
int _fltused = 0x9875;

}

extern "C" void __cdecl WinMainCRTStartup() {
    int mainret;
    STARTUPINFO StartupInfo = {0};
    GetStartupInfo(&StartupInfo);

    //_atexit_init();

    // call C initializers and C++ constructors
    _initterm(__xi_a, __xi_z);
    _initterm(__xc_a, __xc_z);

    mainret = WinMain(GetModuleHandle(NULL), NULL, NULL,
                      StartupInfo.dwFlags & STARTF_USESHOWWINDOW
                            ? StartupInfo.wShowWindow : SW_SHOWDEFAULT );

    _initterm(__xp_a, __xp_z);
    _initterm(__xt_a, __xt_z);

    ExitProcess(mainret);
}
