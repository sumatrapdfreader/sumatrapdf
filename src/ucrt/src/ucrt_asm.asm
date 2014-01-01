; Copyright 2014 the ucrt project authors (see AUTHORS file)
; License: Simplified BSD (see COPYING.BSD)

; many of the functions here are redirects to functions in msvcrt.dll (or some
; other dll). If we want to redirect function foo() to function bar() defined
; in some other dll, we have to:
; * define public symbol _foo (public _foo). Adding "_" is how C mangles names
; * decleare __imp__bar as dword external symbol (external means its defined
;   somewhere else)
; * create a function _foo that jumps to __imp__bar
;
; Exception handling support:
; ___CxxFrameHandler[n] is called for try/catch blocks (C++)
; See "How a C++ compiler implements exception handling"
; http://www.codeproject.com/cpp/exceptionhandler.asp
   
   .386

_TEXT segment use32 para public 'CODE'

    public  __ftol2_sse
    public  _stricmp
    public  _strnicmp
    public  _vsnprintf
    public  __chkstk
    public  __alloca_probe
    public  ___iob_func
    public  ___CxxFrameHandler3
    public  __CxxThrowException@8
    public  ___CxxLongjmpUnwind@4
    public  _msvcrt_vswprintf
    public  __except_handler4

    extrn __imp___ftol:dword
    extrn __imp___stricmp:dword
    extrn __imp____p__iob:dword
    extrn __imp___strnicmp:dword
    extrn __imp___vsnprintf:dword
    extrn __imp____CxxFrameHandler:dword
    extrn __imp___CxxThrowException:dword
    extrn __imp____CxxLongjmpUnwind:dword
    extrn __imp__vswprintf:dword
    extrn __imp___except_handler3:dword

; _ftol2_sse => msvcrt._ftol
__ftol2_sse     proc near
                jmp  __imp___ftol
__ftol2_sse     endp

; stricmp => msvcrt._stricmp
_stricmp        proc near
                jmp __imp___stricmp
_stricmp        endp

; strnicmp => msvcrt._strnicmp
_strnicmp       proc near
                jmp __imp___strnicmp
_strnicmp       endp

; msvcrt_vswprintf => msvcrt.vswprintf
_msvcrt_vswprintf proc near
                jmp __imp__vswprintf
_msvcrt_vswprintf endp

; vsnprintf => ntdll._vsnprintf
_vsnprintf      proc near
                jmp __imp___vsnprintf
_vsnprintf      endp

; __iob_func => msvcrt.__p_iob
___iob_func     proc near
                jmp __imp____p__iob
___iob_func     endp

; __CxxFrameHandler3 => msvcrt._CxxFrameHandler
___CxxFrameHandler3 proc near
                jmp  __imp____CxxFrameHandler
___CxxFrameHandler3 endp

; _CxxThrowException@8 => msvcrt._CxxThrowException
__CxxThrowException@8 proc near
                jmp  __imp___CxxThrowException
__CxxThrowException@8 endp

; TODO: just guessing
; __CxxLongjmpUnwind@4 => msvcrt.__CxxLongjmpUnwind
___CxxLongjmpUnwind@4 proc near
                jmp __imp____CxxLongjmpUnwind
___CxxLongjmpUnwind@4 endp

__except_handler4     proc    near
                jmp __imp___except_handler3
__except_handler4     endp

; _alloca_probe and _chkstk come from sanos project
; http://www.jbox.dk/sanos/source/lib/chkstk.asm.html
PAGESIZE        equ     4096

__chkstk        proc    near
                assume  cs:_TEXT

__alloca_probe  =  __chkstk

                cmp     eax, PAGESIZE           ; more than one page?
                jae     short probesetup        ;   yes, go setup probe loop
                                                ;   no
                neg     eax                     ; compute new stack pointer in eax
                add     eax,esp
                add     eax,4
                test    dword ptr [eax],eax     ; probe it
                xchg    eax,esp
                mov     eax,dword ptr [eax]
                push    eax
                ret

probesetup:
                push    ecx                     ; save ecx
                lea     ecx,[esp] + 8           ; compute new stack pointer in ecx
                                                ; correct for return address and
                                                ; saved ecx

probepages:
                sub     ecx,PAGESIZE            ; yes, move down a page
                sub     eax,PAGESIZE            ; adjust request and...

                test    dword ptr [ecx],eax     ; ...probe it

                cmp     eax,PAGESIZE            ; more than one page requested?
                jae     short probepages        ; no

lastpage:
                sub     ecx,eax                 ; move stack down by eax
                mov     eax,esp                 ; save current tos and do a...

                test    dword ptr [ecx],eax     ; ...probe in case a page was crossed

                mov     esp,ecx                 ; set the new stack pointer

                mov     ecx,dword ptr [eax]     ; recover ecx
                mov     eax,dword ptr [eax + 4] ; recover return address

                push    eax                     ; prepare return address
                                                ; ...probe in case a page was crossed
                ret

__chkstk        endp

_TEXT           ends
                end

