; /callcap: enter passes function address in rax; exit in rcx (return in rax).
default rel
bits 64

section .text

global _CAP_Enter_Function
global _CAP_Exit_Function
extern PerfEnterImpl
extern PerfExitImpl

; 7 pushes (56) => rsp 16-aligned; 32 bytes of shadow space for the call.
%define SHADOW 32
%define SAVED_RAX (SHADOW + 48)
%define SAVED_RCX (SHADOW + 40)

_CAP_Enter_Function:
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    sub rsp, SHADOW
    mov rcx, [rsp+SAVED_RAX]
    call PerfEnterImpl
    add rsp, SHADOW
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
    sub rsp, SHADOW
    mov rcx, [rsp+SAVED_RCX]
    call PerfExitImpl
    add rsp, SHADOW
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    ret
