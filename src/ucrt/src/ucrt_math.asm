; code taken from http://www.jbox.dk/sanos

   .386

_TEXT segment use32 para public 'CODE'
    public __alldiv
    public __alldvrm
    public __allmul
    public __allrem
    public __allshl
    public __allshr
    public __aulldiv
    public __aulldvrm
    public __aullrem
    public __aullshr
    

LOWORD  equ     [0]
HIWORD  equ     [4]

;
; lldiv - signed long divide
;
; Purpose:
;       Does a signed long divide of the arguments.  Arguments are
;       not changed.
;
; Entry:
;       Arguments are passed on the stack:
;               1st pushed: divisor (QWORD)
;               2nd pushed: dividend (QWORD)
;
; Exit:
;       EDX:EAX contains the quotient (dividend/divisor)
;       NOTE: this routine removes the parameters from the stack.
;
; Uses:
;       ECX
;

__alldiv        proc    near
                assume  cs:_TEXT

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

DVND    equ     [esp + 16]      ; stack address of dividend (a)
DVSR    equ     [esp + 24]      ; stack address of divisor (b)


; Determine sign of the result (edi = 0 if result is positive, non-zero
; otherwise) and make operands positive.

        xor     edi,edi         ; result sign assumed positive

        mov     eax,HIWORD(DVND) ; hi word of a
        or      eax,eax         ; test to see if signed
        jge     short L1        ; skip rest if a is already positive
        inc     edi             ; complement result sign flag
        mov     edx,LOWORD(DVND) ; lo word of a
        neg     eax             ; make a positive
        neg     edx
        sbb     eax,0
        mov     HIWORD(DVND),eax ; save positive value
        mov     LOWORD(DVND),edx
L1:
        mov     eax,HIWORD(DVSR) ; hi word of b
        or      eax,eax         ; test to see if signed
        jge     short L2        ; skip rest if b is already positive
        inc     edi             ; complement the result sign flag
        mov     edx,LOWORD(DVSR) ; lo word of a
        neg     eax             ; make b positive
        neg     edx
        sbb     eax,0
        mov     HIWORD(DVSR),eax ; save positive value
        mov     LOWORD(DVSR),edx
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
        mov     ecx,LOWORD(DVSR) ; load divisor
        mov     eax,HIWORD(DVND) ; load high word of dividend
        xor     edx,edx
        div     ecx             ; eax <- high order bits of quotient
        mov     ebx,eax         ; save high bits of quotient
        mov     eax,LOWORD(DVND) ; edx:eax <- remainder:lo word of dividend
        div     ecx             ; eax <- low order bits of quotient
        mov     edx,ebx         ; edx:eax <- quotient
        jmp     short L4        ; set sign, restore stack and return

;
; Here we do it the hard way.  Remember, eax contains the high word of DVSR
;

L3:
        mov     ebx,eax         ; ebx:ecx <- divisor
        mov     ecx,LOWORD(DVSR)
        mov     edx,HIWORD(DVND) ; edx:eax <- dividend
        mov     eax,LOWORD(DVND)
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

        mul     dword ptr HIWORD(DVSR) ; QUOT * HIWORD(DVSR)
        mov     ecx,eax
        mov     eax,LOWORD(DVSR)
        mul     esi             ; QUOT * LOWORD(DVSR)
        add     edx,ecx         ; EDX:EAX = QUOT * DVSR
        jc      short L6        ; carry means Quotient is off by 1

;
; do long compare here between original dividend and the result of the
; multiply in edx:eax.  If original is larger or equal, we are ok, otherwise
; subtract one (1) from the quotient.
;

        cmp     edx,HIWORD(DVND) ; compare hi words of result and original
        ja      short L6        ; if result > original, do subtract
        jb      short L7        ; if result < original, we are ok
        cmp     eax,LOWORD(DVND) ; hi words are equal, compare lo words
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

__alldiv        endp

;
; lldvrm - signed long divide and remainder
;
; Purpose:
;       Does a signed long divide and remainder of the arguments.  Arguments are
;       not changed.
;
; Entry:
;       Arguments are passed on the stack:
;               1st pushed: divisor (QWORD)
;               2nd pushed: dividend (QWORD)
;
; Exit:
;       EDX:EAX contains the quotient (dividend/divisor)
;       EBX:ECX contains the remainder (divided % divisor)
;       NOTE: this routine removes the parameters from the stack.
;
; Uses:
;       ECX
;

__alldvrm       proc    near
                assume  cs:_TEXT

        push    edi
        push    esi
        push    ebp

; Set up the local stack and save the index registers.  When this is done
; the stack frame will look as follows (assuming that the expression a/b will
; generate a call to alldvrm(a, b)):
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
;       ESP---->|      EBP      |
;               -----------------
;

DVND    equ     [esp + 16]      ; stack address of dividend (a)
DVSR    equ     [esp + 24]      ; stack address of divisor (b)


; Determine sign of the quotient (edi = 0 if result is positive, non-zero
; otherwise) and make operands positive.
; Sign of the remainder is kept in ebp.

        xor     edi,edi         ; result sign assumed positive
        xor     ebp,ebp         ; result sign assumed positive

        mov     eax,HIWORD(DVND) ; hi word of a
        or      eax,eax         ; test to see if signed
        jge     short L1        ; skip rest if a is already positive
        inc     edi             ; complement result sign flag
        inc     ebp             ; complement result sign flag
        mov     edx,LOWORD(DVND) ; lo word of a
        neg     eax             ; make a positive
        neg     edx
        sbb     eax,0
        mov     HIWORD(DVND),eax ; save positive value
        mov     LOWORD(DVND),edx
L1:
        mov     eax,HIWORD(DVSR) ; hi word of b
        or      eax,eax         ; test to see if signed
        jge     short L2        ; skip rest if b is already positive
        inc     edi             ; complement the result sign flag
        mov     edx,LOWORD(DVSR) ; lo word of a
        neg     eax             ; make b positive
        neg     edx
        sbb     eax,0
        mov     HIWORD(DVSR),eax ; save positive value
        mov     LOWORD(DVSR),edx
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
        mov     ecx,LOWORD(DVSR) ; load divisor
        mov     eax,HIWORD(DVND) ; load high word of dividend
        xor     edx,edx
        div     ecx             ; eax <- high order bits of quotient
        mov     ebx,eax         ; save high bits of quotient
        mov     eax,LOWORD(DVND) ; edx:eax <- remainder:lo word of dividend
        div     ecx             ; eax <- low order bits of quotient
        mov     esi,eax         ; ebx:esi <- quotient
;
; Now we need to do a multiply so that we can compute the remainder.
;
        mov     eax,ebx         ; set up high word of quotient
        mul     dword ptr LOWORD(DVSR) ; HIWORD(QUOT) * DVSR
        mov     ecx,eax         ; save the result in ecx
        mov     eax,esi         ; set up low word of quotient
        mul     dword ptr LOWORD(DVSR) ; LOWORD(QUOT) * DVSR
        add     edx,ecx         ; EDX:EAX = QUOT * DVSR
        jmp     short L4        ; complete remainder calculation

;
; Here we do it the hard way.  Remember, eax contains the high word of DVSR
;

L3:
        mov     ebx,eax         ; ebx:ecx <- divisor
        mov     ecx,LOWORD(DVSR)
        mov     edx,HIWORD(DVND) ; edx:eax <- dividend
        mov     eax,LOWORD(DVND)
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

        mul     dword ptr HIWORD(DVSR) ; QUOT * HIWORD(DVSR)
        mov     ecx,eax
        mov     eax,LOWORD(DVSR)
        mul     esi             ; QUOT * LOWORD(DVSR)
        add     edx,ecx         ; EDX:EAX = QUOT * DVSR
        jc      short L6        ; carry means Quotient is off by 1

;
; do long compare here between original dividend and the result of the
; multiply in edx:eax.  If original is larger or equal, we are ok, otherwise
; subtract one (1) from the quotient.
;

        cmp     edx,HIWORD(DVND) ; compare hi words of result and original
        ja      short L6        ; if result > original, do subtract
        jb      short L7        ; if result < original, we are ok
        cmp     eax,LOWORD(DVND) ; hi words are equal, compare lo words
        jbe     short L7        ; if less or equal we are ok, else subtract
L6:
        dec     esi             ; subtract 1 from quotient
        sub     eax,LOWORD(DVSR) ; subtract divisor from result
        sbb     edx,HIWORD(DVSR)
L7:
        xor     ebx,ebx         ; ebx:esi <- quotient

L4:
;
; Calculate remainder by subtracting the result from the original dividend.
; Since the result is already in a register, we will do the subtract in the
; opposite direction and negate the result if necessary.
;

        sub     eax,LOWORD(DVND) ; subtract dividend from result
        sbb     edx,HIWORD(DVND)

;
; Now check the result sign flag to see if the result is supposed to be positive
; or negative.  It is currently negated (because we subtracted in the 'wrong'
; direction), so if the sign flag is set we are done, otherwise we must negate
; the result to make it positive again.
;

        dec     ebp             ; check result sign flag
        jns     short L9        ; result is ok, set up the quotient
        neg     edx             ; otherwise, negate the result
        neg     eax
        sbb     edx,0

;
; Now we need to get the quotient into edx:eax and the remainder into ebx:ecx.
;
L9:
        mov     ecx,edx
        mov     edx,ebx
        mov     ebx,ecx
        mov     ecx,eax
        mov     eax,esi

;
; Just the cleanup left to do.  edx:eax contains the quotient.  Set the sign
; according to the save value, cleanup the stack, and return.
;

        dec     edi             ; check to see if result is negative
        jnz     short L8        ; if EDI == 0, result should be negative
        neg     edx             ; otherwise, negate the result
        neg     eax
        sbb     edx,0

;
; Restore the saved registers and return.
;

L8:
        pop     ebp
        pop     esi
        pop     edi

        ret     16

__alldvrm       endp

;
; llmul - long multiply routine
;
; Purpose:
;       Does a long multiply (same for signed/unsigned)
;       Parameters are not changed.
;
; Entry:
;       Parameters are passed on the stack:
;               1st pushed: multiplier (QWORD)
;               2nd pushed: multiplicand (QWORD)
;
; Exit:
;       EDX:EAX - product of multiplier and multiplicand
;       NOTE: parameters are removed from the stack
;
; Uses:
;       ECX
;

__allmul        proc    near
                assume  cs:_TEXT

A       EQU     [esp + 4]       ; stack address of a
B       EQU     [esp + 12]      ; stack address of b

;
;       AHI, BHI : upper 32 bits of A and B
;       ALO, BLO : lower 32 bits of A and B
;
;             ALO * BLO
;       ALO * BHI
; +     BLO * AHI
; ---------------------
;

        mov     eax,HIWORD(A)
        mov     ecx,HIWORD(B)
        or      ecx,eax         ;test for both hiwords zero.
        mov     ecx,LOWORD(B)
        jnz     short hard      ;both are zero, just mult ALO and BLO

        mov     eax,LOWORD(A)
        mul     ecx

        ret     16              ; callee restores the stack

hard:
        push    ebx

; must redefine A and B since esp has been altered

A2      EQU     [esp + 8]       ; stack address of a
B2      EQU     [esp + 16]      ; stack address of b

        mul     ecx             ;eax has AHI, ecx has BLO, so AHI * BLO
        mov     ebx,eax         ;save result

        mov     eax,LOWORD(A2)
        mul     dword ptr HIWORD(B2) ;ALO * BHI
        add     ebx,eax         ;ebx = ((ALO * BHI) + (AHI * BLO))

        mov     eax,LOWORD(A2)  ;ecx = BLO
        mul     ecx             ;so edx:eax = ALO*BLO
        add     edx,ebx         ;now edx has all the LO*HI stuff

        pop     ebx

        ret     16              ; callee restores the stack

__allmul        endp

;
; llrem - signed long remainder
;
; Purpose:
;       Does a signed long remainder of the arguments.  Arguments are
;       not changed.
;
; Entry:
;       Arguments are passed on the stack:
;               1st pushed: divisor (QWORD)
;               2nd pushed: dividend (QWORD)
;
; Exit:
;       EDX:EAX contains the remainder (dividend%divisor)
;       NOTE: this routine removes the parameters from the stack.
;
; Uses:
;       ECX
;

__allrem        proc    near
                assume  cs:_TEXT

        push    ebx
        push    edi

; Set up the local stack and save the index registers.  When this is done
; the stack frame will look as follows (assuming that the expression a%b will
; generate a call to lrem(a, b)):
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
;               |       EBX     |
;               |---------------|
;       ESP---->|       EDI     |
;               -----------------
;

DVND    equ     [esp + 12]      ; stack address of dividend (a)
DVSR    equ     [esp + 20]      ; stack address of divisor (b)


; Determine sign of the result (edi = 0 if result is positive, non-zero
; otherwise) and make operands positive.

        xor     edi,edi         ; result sign assumed positive

        mov     eax,HIWORD(DVND) ; hi word of a
        or      eax,eax         ; test to see if signed
        jge     short L1        ; skip rest if a is already positive
        inc     edi             ; complement result sign flag bit
        mov     edx,LOWORD(DVND) ; lo word of a
        neg     eax             ; make a positive
        neg     edx
        sbb     eax,0
        mov     HIWORD(DVND),eax ; save positive value
        mov     LOWORD(DVND),edx
L1:
        mov     eax,HIWORD(DVSR) ; hi word of b
        or      eax,eax         ; test to see if signed
        jge     short L2        ; skip rest if b is already positive
        mov     edx,LOWORD(DVSR) ; lo word of b
        neg     eax             ; make b positive
        neg     edx
        sbb     eax,0
        mov     HIWORD(DVSR),eax ; save positive value
        mov     LOWORD(DVSR),edx
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
        mov     ecx,LOWORD(DVSR) ; load divisor
        mov     eax,HIWORD(DVND) ; load high word of dividend
        xor     edx,edx
        div     ecx             ; edx <- remainder
        mov     eax,LOWORD(DVND) ; edx:eax <- remainder:lo word of dividend
        div     ecx             ; edx <- final remainder
        mov     eax,edx         ; edx:eax <- remainder
        xor     edx,edx
        dec     edi             ; check result sign flag
        jns     short L4        ; negate result, restore stack and return
        jmp     short L8        ; result sign ok, restore stack and return

;
; Here we do it the hard way.  Remember, eax contains the high word of DVSR
;

L3:
        mov     ebx,eax         ; ebx:ecx <- divisor
        mov     ecx,LOWORD(DVSR)
        mov     edx,HIWORD(DVND) ; edx:eax <- dividend
        mov     eax,LOWORD(DVND)
L5:
        shr     ebx,1           ; shift divisor right one bit
        rcr     ecx,1
        shr     edx,1           ; shift dividend right one bit
        rcr     eax,1
        or      ebx,ebx
        jnz     short L5        ; loop until divisor < 4194304K
        div     ecx             ; now divide, ignore remainder

;
; We may be off by one, so to check, we will multiply the quotient
; by the divisor and check the result against the orignal dividend
; Note that we must also check for overflow, which can occur if the
; dividend is close to 2**64 and the quotient is off by 1.
;

        mov     ecx,eax         ; save a copy of quotient in ECX
        mul     dword ptr HIWORD(DVSR)
        xchg    ecx,eax         ; save product, get quotient in EAX
        mul     dword ptr LOWORD(DVSR)
        add     edx,ecx         ; EDX:EAX = QUOT * DVSR
        jc      short L6        ; carry means Quotient is off by 1

;
; do long compare here between original dividend and the result of the
; multiply in edx:eax.  If original is larger or equal, we are ok, otherwise
; subtract the original divisor from the result.
;

        cmp     edx,HIWORD(DVND) ; compare hi words of result and original
        ja      short L6        ; if result > original, do subtract
        jb      short L7        ; if result < original, we are ok
        cmp     eax,LOWORD(DVND) ; hi words are equal, compare lo words
        jbe     short L7        ; if less or equal we are ok, else subtract
L6:
        sub     eax,LOWORD(DVSR) ; subtract divisor from result
        sbb     edx,HIWORD(DVSR)
L7:

;
; Calculate remainder by subtracting the result from the original dividend.
; Since the result is already in a register, we will do the subtract in the
; opposite direction and negate the result if necessary.
;

        sub     eax,LOWORD(DVND) ; subtract dividend from result
        sbb     edx,HIWORD(DVND)

;
; Now check the result sign flag to see if the result is supposed to be positive
; or negative.  It is currently negated (because we subtracted in the 'wrong'
; direction), so if the sign flag is set we are done, otherwise we must negate
; the result to make it positive again.
;

        dec     edi             ; check result sign flag
        jns     short L8        ; result is ok, restore stack and return
L4:
        neg     edx             ; otherwise, negate the result
        neg     eax
        sbb     edx,0

;
; Just the cleanup left to do.  edx:eax contains the quotient.
; Restore the saved registers and return.
;

L8:
        pop     edi
        pop     ebx

        ret     16

__allrem        endp

;
; llshl - long shift left
;
; Purpose:
;       Does a Long Shift Left (signed and unsigned are identical)
;       Shifts a long left any number of bits.
;
; Entry:
;       EDX:EAX - long value to be shifted
;       CL      - number of bits to shift by
;
; Exit:
;       EDX:EAX - shifted value
;
; Uses:
;       CL is destroyed.
;

__allshl        proc    near
                assume  cs:_TEXT

;
; Handle shifts of 64 or more bits (all get 0)
;
        cmp     cl, 64
        jae     short RETZERO

;
; Handle shifts of between 0 and 31 bits
;
        cmp     cl, 32
        jae     short MORE32
        shld    edx,eax,cl
        shl     eax,cl
        ret

;
; Handle shifts of between 32 and 63 bits
;
MORE32:
        mov     edx,eax
        xor     eax,eax
        and     cl,31
        shl     edx,cl
        ret

;
; return 0 in edx:eax
;
RETZERO:
        xor     eax,eax
        xor     edx,edx
        ret

__allshl        endp

;
; llshr - long shift right
;
; Purpose:
;       Does a signed Long Shift Right
;       Shifts a long right any number of bits.
;
; Entry:
;       EDX:EAX - long value to be shifted
;       CL      - number of bits to shift by
;
; Exit:
;       EDX:EAX - shifted value
;
; Uses:
;       CL is destroyed.
;

__allshr        proc    near
                assume  cs:_TEXT

;
; Handle shifts of 64 bits or more (if shifting 64 bits or more, the result
; depends only on the high order bit of edx).
;
        cmp     cl,64
        jae     short RETSIGN

;
; Handle shifts of between 0 and 31 bits
;
        cmp     cl, 32
        jae     short MORE32
        shrd    eax,edx,cl
        sar     edx,cl
        ret

;
; Handle shifts of between 32 and 63 bits
;
MORE32:
        mov     eax,edx
        sar     edx,31
        and     cl,31
        sar     eax,cl
        ret

;
; Return double precision 0 or -1, depending on the sign of edx
;
RETSIGN:
        sar     edx,31
        mov     eax,edx
        ret

__allshr        endp

;
; ulldiv - unsigned long divide
;
; Purpose:
;       Does a unsigned long divide of the arguments.  Arguments are
;       not changed.
;
; Entry:
;       Arguments are passed on the stack:
;               1st pushed: divisor (QWORD)
;               2nd pushed: dividend (QWORD)
;
; Exit:
;       EDX:EAX contains the quotient (dividend/divisor)
;       NOTE: this routine removes the parameters from the stack.
;
; Uses:
;       ECX
;

__aulldiv       proc    near
                assume  cs:_TEXT

        push    ebx
        push    esi

; Set up the local stack and save the index registers.  When this is done
; the stack frame will look as follows (assuming that the expression a/b will
; generate a call to uldiv(a, b)):
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
;               |      EBX      |
;               |---------------|
;       ESP---->|      ESI      |
;               -----------------
;

DVND    equ     [esp + 12]      ; stack address of dividend (a)
DVSR    equ     [esp + 20]      ; stack address of divisor (b)

;
; Now do the divide.  First look to see if the divisor is less than 4194304K.
; If so, then we can use a simple algorithm with word divides, otherwise
; things get a little more complex.
;

        mov     eax,HIWORD(DVSR) ; check to see if divisor < 4194304K
        or      eax,eax
        jnz     short L1        ; nope, gotta do this the hard way
        mov     ecx,LOWORD(DVSR) ; load divisor
        mov     eax,HIWORD(DVND) ; load high word of dividend
        xor     edx,edx
        div     ecx             ; get high order bits of quotient
        mov     ebx,eax         ; save high bits of quotient
        mov     eax,LOWORD(DVND) ; edx:eax <- remainder:lo word of dividend
        div     ecx             ; get low order bits of quotient
        mov     edx,ebx         ; edx:eax <- quotient hi:quotient lo
        jmp     short L2        ; restore stack and return

;
; Here we do it the hard way.  Remember, eax contains DVSRHI
;

L1:
        mov     ecx,eax         ; ecx:ebx <- divisor
        mov     ebx,LOWORD(DVSR)
        mov     edx,HIWORD(DVND) ; edx:eax <- dividend
        mov     eax,LOWORD(DVND)
L3:
        shr     ecx,1           ; shift divisor right one bit; hi bit <- 0
        rcr     ebx,1
        shr     edx,1           ; shift dividend right one bit; hi bit <- 0
        rcr     eax,1
        or      ecx,ecx
        jnz     short L3        ; loop until divisor < 4194304K
        div     ebx             ; now divide, ignore remainder
        mov     esi,eax         ; save quotient

;
; We may be off by one, so to check, we will multiply the quotient
; by the divisor and check the result against the orignal dividend
; Note that we must also check for overflow, which can occur if the
; dividend is close to 2**64 and the quotient is off by 1.
;

        mul     dword ptr HIWORD(DVSR) ; QUOT * HIWORD(DVSR)
        mov     ecx,eax
        mov     eax,LOWORD(DVSR)
        mul     esi             ; QUOT * LOWORD(DVSR)
        add     edx,ecx         ; EDX:EAX = QUOT * DVSR
        jc      short L4        ; carry means Quotient is off by 1

;
; do long compare here between original dividend and the result of the
; multiply in edx:eax.  If original is larger or equal, we are ok, otherwise
; subtract one (1) from the quotient.
;

        cmp     edx,HIWORD(DVND) ; compare hi words of result and original
        ja      short L4        ; if result > original, do subtract
        jb      short L5        ; if result < original, we are ok
        cmp     eax,LOWORD(DVND) ; hi words are equal, compare lo words
        jbe     short L5        ; if less or equal we are ok, else subtract
L4:
        dec     esi             ; subtract 1 from quotient
L5:
        xor     edx,edx         ; edx:eax <- quotient
        mov     eax,esi

;
; Just the cleanup left to do.  edx:eax contains the quotient.
; Restore the saved registers and return.
;

L2:

        pop     esi
        pop     ebx

        ret     16

__aulldiv       endp

;
; ulldvrm - unsigned long divide and remainder
;
; Purpose:
;       Does a unsigned long divide and remainder of the arguments.  Arguments
;       are not changed.
;
; Entry:
;       Arguments are passed on the stack:
;               1st pushed: divisor (QWORD)
;               2nd pushed: dividend (QWORD)
;
; Exit:
;       EDX:EAX contains the quotient (dividend/divisor)
;       EBX:ECX contains the remainder (divided % divisor)
;       NOTE: this routine removes the parameters from the stack.
;
; Uses:
;       ECX
;

__aulldvrm      proc    near
                assume  cs:_TEXT

        push    esi

; Set up the local stack and save the index registers.  When this is done
; the stack frame will look as follows (assuming that the expression a/b will
; generate a call to aulldvrm(a, b)):
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
;       ESP---->|      ESI      |
;               -----------------
;

DVND    equ     [esp + 8]       ; stack address of dividend (a)
DVSR    equ     [esp + 16]      ; stack address of divisor (b)

;
; Now do the divide.  First look to see if the divisor is less than 4194304K.
; If so, then we can use a simple algorithm with word divides, otherwise
; things get a little more complex.
;

        mov     eax,HIWORD(DVSR) ; check to see if divisor < 4194304K
        or      eax,eax
        jnz     short L1        ; nope, gotta do this the hard way
        mov     ecx,LOWORD(DVSR) ; load divisor
        mov     eax,HIWORD(DVND) ; load high word of dividend
        xor     edx,edx
        div     ecx             ; get high order bits of quotient
        mov     ebx,eax         ; save high bits of quotient
        mov     eax,LOWORD(DVND) ; edx:eax <- remainder:lo word of dividend
        div     ecx             ; get low order bits of quotient
        mov     esi,eax         ; ebx:esi <- quotient

;
; Now we need to do a multiply so that we can compute the remainder.
;
        mov     eax,ebx         ; set up high word of quotient
        mul     dword ptr LOWORD(DVSR) ; HIWORD(QUOT) * DVSR
        mov     ecx,eax         ; save the result in ecx
        mov     eax,esi         ; set up low word of quotient
        mul     dword ptr LOWORD(DVSR) ; LOWORD(QUOT) * DVSR
        add     edx,ecx         ; EDX:EAX = QUOT * DVSR
        jmp     short L2        ; complete remainder calculation

;
; Here we do it the hard way.  Remember, eax contains DVSRHI
;

L1:
        mov     ecx,eax         ; ecx:ebx <- divisor
        mov     ebx,LOWORD(DVSR)
        mov     edx,HIWORD(DVND) ; edx:eax <- dividend
        mov     eax,LOWORD(DVND)
L3:
        shr     ecx,1           ; shift divisor right one bit; hi bit <- 0
        rcr     ebx,1
        shr     edx,1           ; shift dividend right one bit; hi bit <- 0
        rcr     eax,1
        or      ecx,ecx
        jnz     short L3        ; loop until divisor < 4194304K
        div     ebx             ; now divide, ignore remainder
        mov     esi,eax         ; save quotient

;
; We may be off by one, so to check, we will multiply the quotient
; by the divisor and check the result against the orignal dividend
; Note that we must also check for overflow, which can occur if the
; dividend is close to 2**64 and the quotient is off by 1.
;

        mul     dword ptr HIWORD(DVSR) ; QUOT * HIWORD(DVSR)
        mov     ecx,eax
        mov     eax,LOWORD(DVSR)
        mul     esi             ; QUOT * LOWORD(DVSR)
        add     edx,ecx         ; EDX:EAX = QUOT * DVSR
        jc      short L4        ; carry means Quotient is off by 1

;
; do long compare here between original dividend and the result of the
; multiply in edx:eax.  If original is larger or equal, we are ok, otherwise
; subtract one (1) from the quotient.
;

        cmp     edx,HIWORD(DVND) ; compare hi words of result and original
        ja      short L4        ; if result > original, do subtract
        jb      short L5        ; if result < original, we are ok
        cmp     eax,LOWORD(DVND) ; hi words are equal, compare lo words
        jbe     short L5        ; if less or equal we are ok, else subtract
L4:
        dec     esi             ; subtract 1 from quotient
        sub     eax,LOWORD(DVSR) ; subtract divisor from result
        sbb     edx,HIWORD(DVSR)
L5:
        xor     ebx,ebx         ; ebx:esi <- quotient

L2:
;
; Calculate remainder by subtracting the result from the original dividend.
; Since the result is already in a register, we will do the subtract in the
; opposite direction and negate the result.
;

        sub     eax,LOWORD(DVND) ; subtract dividend from result
        sbb     edx,HIWORD(DVND)
        neg     edx             ; otherwise, negate the result
        neg     eax
        sbb     edx,0

;
; Now we need to get the quotient into edx:eax and the remainder into ebx:ecx.
;
        mov     ecx,edx
        mov     edx,ebx
        mov     ebx,ecx
        mov     ecx,eax
        mov     eax,esi
;
; Just the cleanup left to do.  edx:eax contains the quotient.
; Restore the saved registers and return.
;

        pop     esi

        ret     16

__aulldvrm      endp

;
; ullrem - unsigned long remainder
;
; Purpose:
;       Does a unsigned long remainder of the arguments.  Arguments are
;       not changed.
;
; Entry:
;       Arguments are passed on the stack:
;               1st pushed: divisor (QWORD)
;               2nd pushed: dividend (QWORD)
;
; Exit:
;       EDX:EAX contains the remainder (dividend%divisor)
;       NOTE: this routine removes the parameters from the stack.
;
; Uses:
;       ECX
;

__aullrem       proc    near
                assume  cs:_TEXT

        push    ebx

; Set up the local stack and save the index registers.  When this is done
; the stack frame will look as follows (assuming that the expression a%b will
; generate a call to ullrem(a, b)):
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
;       ESP---->|      EBX      |
;               -----------------
;

DVND    equ     [esp + 8]       ; stack address of dividend (a)
DVSR    equ     [esp + 16]      ; stack address of divisor (b)

; Now do the divide.  First look to see if the divisor is less than 4194304K.
; If so, then we can use a simple algorithm with word divides, otherwise
; things get a little more complex.
;

        mov     eax,HIWORD(DVSR) ; check to see if divisor < 4194304K
        or      eax,eax
        jnz     short L1        ; nope, gotta do this the hard way
        mov     ecx,LOWORD(DVSR) ; load divisor
        mov     eax,HIWORD(DVND) ; load high word of dividend
        xor     edx,edx
        div     ecx             ; edx <- remainder, eax <- quotient
        mov     eax,LOWORD(DVND) ; edx:eax <- remainder:lo word of dividend
        div     ecx             ; edx <- final remainder
        mov     eax,edx         ; edx:eax <- remainder
        xor     edx,edx
        jmp     short L2        ; restore stack and return

;
; Here we do it the hard way.  Remember, eax contains DVSRHI
;

L1:
        mov     ecx,eax         ; ecx:ebx <- divisor
        mov     ebx,LOWORD(DVSR)
        mov     edx,HIWORD(DVND) ; edx:eax <- dividend
        mov     eax,LOWORD(DVND)
L3:
        shr     ecx,1           ; shift divisor right one bit; hi bit <- 0
        rcr     ebx,1
        shr     edx,1           ; shift dividend right one bit; hi bit <- 0
        rcr     eax,1
        or      ecx,ecx
        jnz     short L3        ; loop until divisor < 4194304K
        div     ebx             ; now divide, ignore remainder

;
; We may be off by one, so to check, we will multiply the quotient
; by the divisor and check the result against the orignal dividend
; Note that we must also check for overflow, which can occur if the
; dividend is close to 2**64 and the quotient is off by 1.
;

        mov     ecx,eax         ; save a copy of quotient in ECX
        mul     dword ptr HIWORD(DVSR)
        xchg    ecx,eax         ; put partial product in ECX, get quotient in EAX
        mul     dword ptr LOWORD(DVSR)
        add     edx,ecx         ; EDX:EAX = QUOT * DVSR
        jc      short L4        ; carry means Quotient is off by 1

;
; do long compare here between original dividend and the result of the
; multiply in edx:eax.  If original is larger or equal, we're ok, otherwise
; subtract the original divisor from the result.
;

        cmp     edx,HIWORD(DVND) ; compare hi words of result and original
        ja      short L4        ; if result > original, do subtract
        jb      short L5        ; if result < original, we're ok
        cmp     eax,LOWORD(DVND) ; hi words are equal, compare lo words
        jbe     short L5        ; if less or equal we're ok, else subtract
L4:
        sub     eax,LOWORD(DVSR) ; subtract divisor from result
        sbb     edx,HIWORD(DVSR)
L5:

;
; Calculate remainder by subtracting the result from the original dividend.
; Since the result is already in a register, we will perform the subtract in
; the opposite direction and negate the result to make it positive.
;

        sub     eax,LOWORD(DVND) ; subtract original dividend from result
        sbb     edx,HIWORD(DVND)
        neg     edx             ; and negate it
        neg     eax
        sbb     edx,0

;
; Just the cleanup left to do.  dx:ax contains the remainder.
; Restore the saved registers and return.
;

L2:

        pop     ebx

        ret     16

__aullrem       endp

;
; ullshr - long shift right
;
; Purpose:
;       Does a unsigned Long Shift Right
;       Shifts a long right any number of bits.
;
; Entry:
;       EDX:EAX - long value to be shifted
;       CL      - number of bits to shift by
;
; Exit:
;       EDX:EAX - shifted value
;
; Uses:
;       CL is destroyed.
;

__aullshr       proc    near
                assume  cs:_TEXT

;
; Handle shifts of 64 bits or more (if shifting 64 bits or more, the result
; depends only on the high order bit of edx).
;
        cmp     cl,64
        jae     short RETZERO

;
; Handle shifts of between 0 and 31 bits
;
        cmp     cl, 32
        jae     short MORE32
        shrd    eax,edx,cl
        shr     edx,cl
        ret

;
; Handle shifts of between 32 and 63 bits
;
MORE32:
        mov     eax,edx
        xor     edx,edx
        and     cl,31
        shr     eax,cl
        ret

;
; return 0 in edx:eax
;
RETZERO:
        xor     eax,eax
        xor     edx,edx
        ret

__aullshr       endp


_TEXT           ends
                end

