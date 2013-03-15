; 7zCrcOpt.asm -- CRC32 calculation : optimized version
; 2009-12-12 : Igor Pavlov : Public domain

include 7zAsm.asm

MY_ASM_START

rD   equ  r2
rN   equ  r7

ifdef x64
    num_VAR     equ r8
    table_VAR   equ r9
else
    data_size   equ (REG_SIZE * 5)
    crc_table   equ (REG_SIZE + data_size)
    num_VAR     equ [r4 + data_size]
    table_VAR   equ [r4 + crc_table]
endif

SRCDAT  equ  rN + rD + 4 *

CRC macro op:req, dest:req, src:req, t:req
    op      dest, DWORD PTR [r5 + src * 4 + 0400h * t]
endm

CRC_XOR macro dest:req, src:req, t:req
    CRC xor, dest, src, t
endm

CRC_MOV macro dest:req, src:req, t:req
    CRC mov, dest, src, t
endm

CRC1b macro
    movzx   x6, BYTE PTR [rD]
    inc     rD
    movzx   x3, x0_L
    xor     x6, x3
    shr     x0, 8
    CRC     xor, x0, r6, 0
    dec     rN
endm

MY_PROLOG macro crc_end:req
    MY_PUSH_4_REGS
    
    mov     x0, x1
    mov     rN, num_VAR
    mov     r5, table_VAR
    test    rN, rN
    jz      crc_end
  @@:
    test    rD, 7
    jz      @F
    CRC1b
    jnz     @B
  @@:
    cmp     rN, 16
    jb      crc_end
    add     rN, rD
    mov     num_VAR, rN
    sub     rN, 8
    and     rN, NOT 7
    sub     rD, rN
    xor     x0, [SRCDAT 0]
endm

MY_EPILOG macro crc_end:req
    xor     x0, [SRCDAT 0]
    mov     rD, rN
    mov     rN, num_VAR
    sub     rN, rD
  crc_end:
    test    rN, rN
    jz      @F
    CRC1b
    jmp     crc_end
  @@:
    MY_POP_4_REGS
endm

MY_PROC CrcUpdateT8, 4
    MY_PROLOG crc_end_8
    mov     x1, [SRCDAT 1]
    align 16
  main_loop_8:
    mov     x6, [SRCDAT 2]
    movzx   x3, x1_L
    CRC_XOR x6, r3, 3
    movzx   x3, x1_H
    CRC_XOR x6, r3, 2
    shr     x1, 16
    movzx   x3, x1_L
    movzx   x1, x1_H
    CRC_XOR x6, r3, 1
    movzx   x3, x0_L
    CRC_XOR x6, r1, 0

    mov     x1, [SRCDAT 3]
    CRC_XOR x6, r3, 7
    movzx   x3, x0_H
    shr     x0, 16
    CRC_XOR x6, r3, 6
    movzx   x3, x0_L
    CRC_XOR x6, r3, 5
    movzx   x3, x0_H
    CRC_MOV x0, r3, 4
    xor     x0, x6
    add     rD, 8
    jnz     main_loop_8

    MY_EPILOG crc_end_8
MY_ENDP

MY_PROC CrcUpdateT4, 4
    MY_PROLOG crc_end_4
    align 16
  main_loop_4:
    movzx   x1, x0_L
    movzx   x3, x0_H
    shr     x0, 16
    movzx   x6, x0_H
    and     x0, 0FFh
    CRC_MOV x1, r1, 3
    xor     x1, [SRCDAT 1]
    CRC_XOR x1, r3, 2
    CRC_XOR x1, r6, 0
    CRC_XOR x1, r0, 1
 
    movzx   x0, x1_L
    movzx   x3, x1_H
    shr     x1, 16
    movzx   x6, x1_H
    and     x1, 0FFh
    CRC_MOV x0, r0, 3
    xor     x0, [SRCDAT 2]
    CRC_XOR x0, r3, 2
    CRC_XOR x0, r6, 0
    CRC_XOR x0, r1, 1
    add     rD, 8
    jnz     main_loop_4

    MY_EPILOG crc_end_4
MY_ENDP

end
