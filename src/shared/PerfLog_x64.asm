; /callcap: enter passes function address in rax; exit in rcx (return in rax).
default rel
bits 64

section .text

global _CAP_Enter_Function
global _CAP_Exit_Function
extern PerfEnterImpl
extern PerfExitImpl

; 7 pushes (56) => rsp 16-aligned; 32 bytes shadow + 96 bytes for xmm0..xmm5.
%define SHADOW 32
%define XMM_SAVE 96
%define FRAME (SHADOW + XMM_SAVE)
%define SAVED_RAX (FRAME + 48)
%define SAVED_RCX (FRAME + 40)

_CAP_Enter_Function:
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    sub rsp, FRAME
    movdqu [rsp+SHADOW+0], xmm0
    movdqu [rsp+SHADOW+16], xmm1
    movdqu [rsp+SHADOW+32], xmm2
    movdqu [rsp+SHADOW+48], xmm3
    movdqu [rsp+SHADOW+64], xmm4
    movdqu [rsp+SHADOW+80], xmm5
    mov rcx, [rsp+SAVED_RAX]
    call PerfEnterImpl
    movdqu xmm5, [rsp+SHADOW+80]
    movdqu xmm4, [rsp+SHADOW+64]
    movdqu xmm3, [rsp+SHADOW+48]
    movdqu xmm2, [rsp+SHADOW+32]
    movdqu xmm1, [rsp+SHADOW+16]
    movdqu xmm0, [rsp+SHADOW+0]
    add rsp, FRAME
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    ret

_CAP_Exit_Function:
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    sub rsp, FRAME
    movdqu [rsp+SHADOW+0], xmm0
    movdqu [rsp+SHADOW+16], xmm1
    movdqu [rsp+SHADOW+32], xmm2
    movdqu [rsp+SHADOW+48], xmm3
    movdqu [rsp+SHADOW+64], xmm4
    movdqu [rsp+SHADOW+80], xmm5
    mov rcx, [rsp+SAVED_RCX]
    call PerfExitImpl
    movdqu xmm5, [rsp+SHADOW+80]
    movdqu xmm4, [rsp+SHADOW+64]
    movdqu xmm3, [rsp+SHADOW+48]
    movdqu xmm2, [rsp+SHADOW+32]
    movdqu xmm1, [rsp+SHADOW+16]
    movdqu xmm0, [rsp+SHADOW+0]
    add rsp, FRAME
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    ret
