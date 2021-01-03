/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef NS_WINDOWS_DLL_INTERCEPTOR_H_
#define NS_WINDOWS_DLL_INTERCEPTOR_H_
#include <windows.h>
#include <winternl.h>

/*
 * Simple trampoline interception
 *
 * 1. Save first N bytes of OrigFunction to trampoline, where N is a
 *    number of bytes >= 5 that are instruction aligned.
 *
 * 2. Replace first 5 bytes of OrigFunction with a jump to the Hook
 *    function.
 *
 * 3. After N bytes of the trampoline, add a jump to OrigFunction+N to
 *    continue original program flow.
 *
 * 4. Hook function needs to call the trampoline during its execution,
 *    to invoke the original function (so address of trampoline is
 *    returned).
 *
 * When the WindowsDllInterceptor class is destructed, OrigFunction is
 * patched again to jump directly to the trampoline instead of going
 * through the hook function. As such, re-intercepting the same function
 * won't work, as jump instructions are not supported.
 */

class WindowsDllInterceptor {
    typedef u8* byteptr_t;

  public:
    WindowsDllInterceptor() : mModule(0) {
    }

    WindowsDllInterceptor(const char* modulename, int nhooks = 0) {
        Init(modulename, nhooks);
    }

    ~WindowsDllInterceptor() {
        int i;
        byteptr_t p;
        for (i = 0, p = mHookPage; i < mCurHooks; i++, p += kHookSize) {
#if defined(_M_IX86)
            size_t nBytes = 1 + sizeof(intptr_t);
#elif defined(_M_X64)
            size_t nBytes = 2 + sizeof(intptr_t);
#else
#error "Unknown processor type"
#endif
            byteptr_t origBytes = *((byteptr_t*)p);
            // ensure we can modify the original code
            DWORD op;
            if (!VirtualProtectEx(GetCurrentProcess(), origBytes, nBytes, PAGE_EXECUTE_READWRITE, &op)) {
                // printf ("VirtualProtectEx failed! %d\n", GetLastError());
                continue;
            }
            // Remove the hook by making the original function jump directly
            // in the trampoline.
            intptr_t dest = (intptr_t)(p + sizeof(void*));
#if defined(_M_IX86)
            *((intptr_t*)(origBytes + 1)) = dest - (intptr_t)(origBytes + 5); // target displacement
#elif defined(_M_X64)
            *((intptr_t*)(origBytes + 2)) = dest;
#else
#error "Unknown processor type"
#endif
            // restore protection; if this fails we can't really do anything about it
            VirtualProtectEx(GetCurrentProcess(), origBytes, nBytes, op, &op);
        }
    }

    void Init(const char* modulename, int nhooks = 0) {
        if (mModule)
            return;

        mModule = LoadLibraryExA(modulename, nullptr, 0);
        if (!mModule) {
            // printf("LoadLibraryEx for '%s' failed\n", modulename);
            return;
        }

        int hooksPerPage = 4096 / kHookSize;
        if (nhooks == 0)
            nhooks = hooksPerPage;

        mMaxHooks = nhooks + (hooksPerPage % nhooks);
        mCurHooks = 0;

        mHookPage = (byteptr_t)VirtualAllocEx(GetCurrentProcess(), nullptr, mMaxHooks * kHookSize,
                                              MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

        if (!mHookPage) {
            mModule = 0;
            return;
        }
    }

    void LockHooks() {
        if (!mModule)
            return;

        DWORD op;
        VirtualProtectEx(GetCurrentProcess(), mHookPage, mMaxHooks * kHookSize, PAGE_EXECUTE_READ, &op);

        mModule = 0;
    }

    bool AddHook(const char* pname, intptr_t hookDest, void** origFunc) {
        if (!mModule)
            return false;

        void* pAddr = (void*)GetProcAddress(mModule, pname);
        if (!pAddr) {
            // printf ("GetProcAddress failed\n");
            return false;
        }

        void* tramp = CreateTrampoline(pAddr, hookDest);
        if (!tramp) {
            // printf ("CreateTrampoline failed\n");
            return false;
        }

        *origFunc = tramp;

        return true;
    }

  protected:
    const static int kPageSize = 4096;
    const static int kHookSize = 128;

    HMODULE mModule = nullptr;
    byteptr_t mHookPage = 0;
    int mMaxHooks = 0;
    int mCurHooks = 0;

    byteptr_t CreateTrampoline(void* origFunction, intptr_t dest) {
        byteptr_t tramp = FindTrampolineSpace();
        if (!tramp)
            return 0;

        byteptr_t origBytes = (byteptr_t)origFunction;

        int nBytes = 0;
        int pJmp32 = -1;

#if defined(_M_IX86)
        while (nBytes < 5) {
            // Understand some simple instructions that might be found in a
            // prologue; we might need to extend this as necessary.
            //
            // Note!  If we ever need to understand jump instructions, we'll
            // need to rewrite the displacement argument.
            if (origBytes[nBytes] >= 0x88 && origBytes[nBytes] <= 0x8B) {
                // various MOVs
                u8 b = origBytes[nBytes + 1];
                if (((b & 0xc0) == 0xc0) || (((b & 0xc0) == 0x00) && ((b & 0x07) != 0x04) && ((b & 0x07) != 0x05))) {
                    // REG=r, R/M=r or REG=r, R/M=[r]
                    nBytes += 2;
                } else if (((b & 0xc0) == 0x40) && ((b & 0x38) != 0x20)) {
                    // REG=r, R/M=[r + disp8]
                    nBytes += 3;
                } else {
                    // complex MOV, bail
                    return 0;
                }
            } else if (origBytes[nBytes] == 0x83) {
                // ADD|ODR|ADC|SBB|AND|SUB|XOR|CMP r/m, imm8
                u8 b = origBytes[nBytes + 1];
                if ((b & 0xc0) == 0xc0) {
                    // ADD|ODR|ADC|SBB|AND|SUB|XOR|CMP r, imm8
                    nBytes += 3;
                } else {
                    // bail
                    return 0;
                }
            } else if (origBytes[nBytes] == 0x68) {
                // PUSH with 4-byte operand
                nBytes += 5;
            } else if ((origBytes[nBytes] & 0xf0) == 0x50) {
                // 1-byte PUSH/POP
                nBytes++;
            } else if (origBytes[nBytes] == 0x6A) {
                // PUSH imm8
                nBytes += 2;
            } else if (origBytes[nBytes] == 0xe9) {
                pJmp32 = nBytes;
                // jmp 32bit offset
                nBytes += 5;
            } else {
                // printf ("Unknown x86 instruction byte 0x%02x, aborting trampoline\n", origBytes[nBytes]);
                return 0;
            }
        }
#elif defined(_M_X64)
        while (nBytes < 13) {
            // if found JMP 32bit offset, next bytes must be NOP
            if (pJmp32 >= 0) {
                if (origBytes[nBytes++] != 0x90)
                    return 0;

                continue;
            }

            if (origBytes[nBytes] == 0x41) {
                // REX.B
                nBytes++;

                if ((origBytes[nBytes] & 0xf0) == 0x50) {
                    // push/pop with Rx register
                    nBytes++;
                } else if (origBytes[nBytes] >= 0xb8 && origBytes[nBytes] <= 0xbf) {
                    // mov r32, imm32
                    nBytes += 5;
                } else {
                    return 0;
                }
            } else if (origBytes[nBytes] == 0x45) {
                // REX.R & REX.B
                nBytes++;

                if (origBytes[nBytes] == 0x33) {
                    // xor r32, r32
                    nBytes += 2;
                } else {
                    return 0;
                }
            } else if ((origBytes[nBytes] & 0xfb) == 0x48) {
                // REX.W | REX.WR
                nBytes++;

                if (origBytes[nBytes] == 0x81 && (origBytes[nBytes + 1] & 0xf8) == 0xe8) {
                    // sub r, dword
                    nBytes += 6;
                } else if (origBytes[nBytes] == 0x83 && (origBytes[nBytes + 1] & 0xf8) == 0xe8) {
                    // sub r, byte
                    nBytes += 3;
                } else if (origBytes[nBytes] == 0x83 && (origBytes[nBytes + 1] & 0xf8) == 0x60) {
                    // and [r+d], imm8
                    nBytes += 5;
                } else if ((origBytes[nBytes] & 0xfd) == 0x89) {
                    // MOV r/m64, r64 | MOV r64, r/m64
                    if ((origBytes[nBytes + 1] & 0xc0) == 0x40) {
                        if ((origBytes[nBytes + 1] & 0x7) == 0x04) {
                            // R/M=[SIB+disp8], REG=r64
                            nBytes += 4;
                        } else {
                            // R/M=[r64+disp8], REG=r64
                            nBytes += 3;
                        }
                    } else if (((origBytes[nBytes + 1] & 0xc0) == 0xc0) ||
                               (((origBytes[nBytes + 1] & 0xc0) == 0x00) && ((origBytes[nBytes + 1] & 0x07) != 0x04) &&
                                ((origBytes[nBytes + 1] & 0x07) != 0x05))) {
                        // REG=r64, R/M=r64 or REG=r64, R/M=[r64]
                        nBytes += 2;
                    } else {
                        // complex MOV
                        return 0;
                    }
                } else {
                    // not support yet!
                    return 0;
                }
            } else if ((origBytes[nBytes] & 0xf0) == 0x50) {
                // 1-byte push/pop
                nBytes++;
            } else if (origBytes[nBytes] == 0x90) {
                // nop
                nBytes++;
            } else if (origBytes[nBytes] == 0xe9) {
                pJmp32 = nBytes;
                // jmp 32bit offset
                nBytes += 5;
            } else if (origBytes[nBytes] == 0xff) {
                nBytes++;
                if ((origBytes[nBytes] & 0xf8) == 0xf0) {
                    // push r64
                    nBytes++;
                } else {
                    return 0;
                }
            } else {
                return 0;
            }
        }
#else
#error "Unknown processor type"
#endif

        if (nBytes > 100) {
            // printf ("Too big!");
            return 0;
        }

        // We keep the address of the original function in the first bytes of
        // the trampoline buffer
        *((void**)tramp) = origFunction;
        tramp += sizeof(void*);

        memcpy(tramp, origFunction, nBytes);

        // OrigFunction+N, the target of the trampoline
        byteptr_t trampDest = origBytes + nBytes;

#if defined(_M_IX86)
        if (pJmp32 >= 0) {
            // Jump directly to the original target of the jump instead of jumping to the
            // original function.
            // Adjust jump target displacement to jump location in the trampoline.
            *((intptr_t*)(tramp + pJmp32 + 1)) += origBytes + pJmp32 - tramp;
        } else {
            tramp[nBytes] = 0xE9; // jmp
            *((intptr_t*)(tramp + nBytes + 1)) =
                (intptr_t)trampDest - (intptr_t)(tramp + nBytes + 5); // target displacement
        }
#elif defined(_M_X64)
        // If JMP32 opcode found, we don't insert to trampoline jump
        if (pJmp32 >= 0) {
            // convert JMP 32bit offset to JMP 64bit direct
            byteptr_t directJmpAddr = origBytes + pJmp32 + 5 + (*((LONG*)(origBytes + pJmp32 + 1)));
            // mov r11, address
            tramp[pJmp32] = 0x49;
            tramp[pJmp32 + 1] = 0xbb;
            *((intptr_t*)(tramp + pJmp32 + 2)) = (intptr_t)directJmpAddr;

            // jmp r11
            tramp[pJmp32 + 10] = 0x41;
            tramp[pJmp32 + 11] = 0xff;
            tramp[pJmp32 + 12] = 0xe3;
        } else {
            // mov r11, address
            tramp[nBytes] = 0x49;
            tramp[nBytes + 1] = 0xbb;
            *((intptr_t*)(tramp + nBytes + 2)) = (intptr_t)trampDest;

            // jmp r11
            tramp[nBytes + 10] = 0x41;
            tramp[nBytes + 11] = 0xff;
            tramp[nBytes + 12] = 0xe3;
        }
#endif

        // ensure we can modify the original code
        DWORD op;
        if (!VirtualProtectEx(GetCurrentProcess(), origFunction, nBytes, PAGE_EXECUTE_READWRITE, &op)) {
            // printf ("VirtualProtectEx failed! %d\n", GetLastError());
            return 0;
        }

#if defined(_M_IX86)
        // now modify the original bytes
        origBytes[0] = 0xE9;                                              // jmp
        *((intptr_t*)(origBytes + 1)) = dest - (intptr_t)(origBytes + 5); // target displacement
#elif defined(_M_X64)
        // mov r11, address
        origBytes[0] = 0x49;
        origBytes[1] = 0xbb;

        *((intptr_t*)(origBytes + 2)) = dest;

        // jmp r11
        origBytes[10] = 0x41;
        origBytes[11] = 0xff;
        origBytes[12] = 0xe3;
#endif

        // restore protection; if this fails we can't really do anything about it
        VirtualProtectEx(GetCurrentProcess(), origFunction, nBytes, op, &op);

        return tramp;
    }

    byteptr_t FindTrampolineSpace() {
        if (mCurHooks >= mMaxHooks)
            return 0;

        byteptr_t p = mHookPage + mCurHooks * kHookSize;

        mCurHooks++;

        return p;
    }
};

#endif /* NS_WINDOWS_DLL_INTERCEPTOR_H_ */
