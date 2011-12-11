; Copyright 2011-2012 the ucrt project authors (see AUTHORS file)
; License: Simplified BSD (see COPYING)

; many of the functions here are redirects to functions in msvcrt.dll (or some
; other dll). If we want to redirect function foo() to function bar() defined
; in some other dll, we have to:
; * define public symbol _foo (public _foo). Adding "_" is how C mangles names
; * decleare __imp__bar as dword external symbol (external means its defined
;   somewhere else)
; * create a function _foo that jumps to __imp__bar

   .386

; TODO: write redirects for those and test they work as expected
; by writing small code snippets that call them and step through
; asembly to verify we end up in msvcrt.dll
;close=_close ; TODO: verify
;open=_open ; TODO: verify
;read=_read ; TODO: verify
;write=_write ; TODO: verify

_TEXT segment use32 para public 'CODE'

    public  __ftol2_sse
    public  _stricmp
    public  ___iob_func
    public _strnicmp

    extrn __imp___ftol:dword
    extrn __imp___stricmp:dword
    extrn __imp____p__iob:dword
    extrn __imp___strnicmp:dword

; redirect _ftol2_sse => _ftol in msvcrt.dll
__ftol2_sse     proc near
                jmp  __imp___ftol
__ftol2_sse     endp

; redirect stricmp => _stricmp in msvcrt.dll
_stricmp        proc near
                jmp __imp___stricmp
_stricmp        endp

; redirect strnicmp => _strnicmp in msvcrt.dll
_strnicmp       proc near
                jmp __imp___strnicmp
_strnicmp       endp

; redirect __iob_func => __p_iob in msvcrt.dll
___iob_func     proc near
                jmp __imp____p__iob
___iob_func     endp

_TEXT           ends
                end

