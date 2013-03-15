; 7zAsm.asm -- ASM macros
; 2009-12-12 : Igor Pavlov : Public domain

MY_ASM_START macro
  ifdef x64
    .code
  else
    .386
    .model flat
    _TEXT$00 SEGMENT PARA PUBLIC 'CODE'
  endif
endm

MY_PROC macro name:req, numParams:req
  align 16
  proc_numParams equ numParams
  ifdef x64
    proc_name equ name
    name PROC
  else
    proc_fastcall_name equ @CatStr(@,name,@, %numParams * 4)
    public proc_fastcall_name
    proc_fastcall_name:
  endif
endm

MY_ENDP macro
  ifdef x64
    ret
    proc_name ENDP
  else
    ret (proc_numParams - 2) * 4
  endif
endm

ifdef x64
  REG_SIZE equ 8
else
  REG_SIZE equ 4
endif

  x0 equ EAX
  x1 equ ECX
  x2 equ EDX
  x3 equ EBX
  x4 equ ESP
  x5 equ EBP
  x6 equ ESI
  x7 equ EDI

  x0_L equ AL
  x1_L equ CL
  x2_L equ DL
  x3_L equ BL

  x0_H equ AH
  x1_H equ CH
  x2_H equ DH
  x3_H equ BH

ifdef x64
  r0 equ RAX
  r1 equ RCX
  r2 equ RDX
  r3 equ RBX
  r4 equ RSP
  r5 equ RBP
  r6 equ RSI
  r7 equ RDI
else
  r0 equ x0
  r1 equ x1
  r2 equ x2
  r3 equ x3
  r4 equ x4
  r5 equ x5
  r6 equ x6
  r7 equ x7
endif

MY_PUSH_4_REGS macro
    push    r3
    push    r5
    push    r6
    push    r7
endm

MY_POP_4_REGS macro
    pop     r7
    pop     r6
    pop     r5
    pop     r3
endm
