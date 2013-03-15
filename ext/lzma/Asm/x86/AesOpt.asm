; AesOpt.asm -- Intel's AES.
; 2009-12-12 : Igor Pavlov : Public domain

include 7zAsm.asm

MY_ASM_START

ifndef x64
    .xmm
endif

ifdef x64
    num     equ r8
else
    num     equ [r4 + REG_SIZE * 4]
endif

rD equ r2
rN equ r0

MY_PROLOG macro reg:req
    ifdef x64
    movdqa  [r4 + 8], xmm6
    movdqa  [r4 + 8 + 16], xmm7
    endif

    push    r3
    push    r5
    push    r6

    mov     rN, num
    mov     x6, [r1 + 16]
    shl     x6, 5

    movdqa  reg, [r1]
    add     r1, 32
endm

MY_EPILOG macro
    pop     r6
    pop     r5
    pop     r3

    ifdef x64
    movdqa  xmm6, [r4 + 8]
    movdqa  xmm7, [r4 + 8 + 16]
    endif

    MY_ENDP
endm

ways equ 4
ways16 equ (ways * 16)

OP_W macro op, op2
    i = 0
    rept ways
    op @CatStr(xmm,%i), op2
    i = i + 1
    endm
endm

LOAD_OP macro op:req, offs:req
    op      xmm0, [r1 + r3 offs]
endm
  
LOAD_OP_W macro op:req, offs:req
    movdqa  xmm7, [r1 + r3 offs]
    OP_W    op, xmm7
endm


; ---------- AES-CBC Decode ----------

CBC_DEC_UPDATE macro reg, offs
    pxor    reg, xmm6
    movdqa  xmm6, [rD + offs]
    movdqa  [rD + offs], reg
endm

DECODE macro op:req
    op      aesdec, +16
  @@:
    op      aesdec, +0
    op      aesdec, -16
    sub     x3, 32
    jnz     @B
    op      aesdeclast, +0
endm

MY_PROC AesCbc_Decode_Intel, 3
    MY_PROLOG xmm6

    sub     x6, 32

    jmp     check2

  align 16
  nextBlocks2:
    mov     x3, x6
    OP_W    movdqa, [rD + i * 16]
    LOAD_OP_W  pxor, +32
    DECODE  LOAD_OP_W
    OP_W    CBC_DEC_UPDATE, i * 16
    add     rD, ways16
  check2:
    sub     rN, ways
    jnc     nextBlocks2

    add     rN, ways
    jmp     check

  nextBlock:
    mov     x3, x6
    movdqa  xmm1, [rD]
    LOAD_OP movdqa, +32
    pxor    xmm0, xmm1
    DECODE  LOAD_OP
    pxor    xmm0, xmm6
    movdqa  [rD], xmm0
    movdqa  xmm6, xmm1
    add     rD, 16
  check:
    sub     rN, 1
    jnc     nextBlock

    movdqa  [r1 - 32], xmm6
    MY_EPILOG


; ---------- AES-CBC Encode ----------

ENCODE macro op:req
    op      aesenc, -16
  @@:
    op      aesenc, +0
    op      aesenc, +16
    add     r3, 32
    jnz     @B
    op      aesenclast, +0
endm

MY_PROC AesCbc_Encode_Intel, 3
    MY_PROLOG xmm0

    add     r1, r6
    neg     r6
    add     r6, 32

    jmp     check_e

  align 16
  nextBlock_e:
    mov     r3, r6
    pxor    xmm0, [rD]
    pxor    xmm0, [r1 + r3 - 32]
    ENCODE  LOAD_OP
    movdqa  [rD], xmm0
    add     rD, 16
  check_e:
    sub     rN, 1
    jnc     nextBlock_e

    movdqa  [r1 + r6 - 64], xmm0
    MY_EPILOG


; ---------- AES-CTR ----------

XOR_UPD_1 macro reg, offs
    pxor    reg, [rD + offs]
endm

XOR_UPD_2 macro reg, offs
    movdqa  [rD + offs], reg
endm

MY_PROC AesCtr_Code_Intel, 3
    MY_PROLOG xmm6

    mov     r5, r4
    shr     r5, 4
    dec     r5
    shl     r5, 4

    mov     DWORD PTR [r5], 1
    mov     DWORD PTR [r5 + 4], 0
    mov     DWORD PTR [r5 + 8], 0
    mov     DWORD PTR [r5 + 12], 0
    
    add     r1, r6
    neg     r6
    add     r6, 32

    jmp     check2_c

  align 16
  nextBlocks2_c:
    movdqa  xmm7, [r5]

    i = 0
    rept ways
    paddq   xmm6, xmm7
    movdqa  @CatStr(xmm,%i), xmm6
    i = i + 1
    endm

    mov     r3, r6
    LOAD_OP_W  pxor, -32
    ENCODE  LOAD_OP_W
    OP_W    XOR_UPD_1, i * 16
    OP_W    XOR_UPD_2, i * 16
    add     rD, ways16
  check2_c:
    sub     rN, ways
    jnc     nextBlocks2_c

    add     rN, ways
    jmp     check_c

  nextBlock_c:
    paddq   xmm6, [r5]
    mov     r3, r6
    movdqa  xmm0, [r1 + r3 - 32]
    pxor    xmm0, xmm6
    ENCODE  LOAD_OP
    XOR_UPD_1 xmm0, 0
    XOR_UPD_2 xmm0, 0
    add     rD, 16
  check_c:
    sub     rN, 1
    jnc     nextBlock_c

    movdqa  [r1 + r6 - 64], xmm6
    MY_EPILOG

end
