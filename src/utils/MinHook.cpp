/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// based on https://github.com/TsudaKageyu/minhook
// only supports 64 bit and all code in a single file

#include "BaseUtil.h"
#include "MinHook.h"

// we only support x86 64-bit. for everyone else we do no-op stubs
#if !IS_INTEL_64
MH_STATUS WINAPI MH_Initialize() {
    return MH_UNKNOWN;
}

MH_STATUS WINAPI MH_Uninitialize() {
    return MH_UNKNOWN;
}

MH_STATUS WINAPI MH_CreateHook(void*, void*, void**) {
    return MH_UNKNOWN;
}

MH_STATUS WINAPI MH_CreateHookApi(const WCHAR*, const char*, void*, void**) {
    return MH_UNKNOWN;
}

MH_STATUS WINAPI MH_CreateHookApiEx(const WCHAR*, const char*, void*, void**,
                                    void**) {
    return MH_UNKNOWN;
}

MH_STATUS WINAPI MH_RemoveHook(void*) {
    return MH_UNKNOWN;
}

MH_STATUS WINAPI MH_EnableHook(void*) {
    return MH_UNKNOWN;
}

MH_STATUS WINAPI MH_DisableHook(void*) {
    return MH_UNKNOWN;
}

MH_STATUS WINAPI MH_QueueEnableHook(void*) {
    return MH_UNKNOWN;
}

MH_STATUS WINAPI MH_QueueDisableHook(void*) {
    return MH_UNKNOWN;
}

MH_STATUS WINAPI MH_ApplyQueued() {
    return MH_UNKNOWN;
}

const char* WINAPI MH_StatusToString(MH_STATUS) {
    return "";
}
#endif

#if IS_INTEL_64

// Initialize the MinHook library. You must call this function EXACTLY ONCE
// at the beginning of your program.
MH_STATUS WINAPI MH_Initialize() {
    return MH_UNKNOWN;
}

// Uninitialize the MinHook library. You must call this function EXACTLY
// ONCE at the end of your program.
MH_STATUS WINAPI MH_Uninitialize() {
    return MH_UNKNOWN;
}

// Creates a hook for the specified target function, in disabled state.
// Parameters:
//   pTarget     [in]  A pointer to the target function, which will be
//                     overridden by the detour function.
//   pDetour     [in]  A pointer to the detour function, which will override
//                     the target function.
//   ppOriginal  [out] A pointer to the trampoline function, which will be
//                     used to call the original target function.
//                     This parameter can be NULL.
MH_STATUS WINAPI MH_CreateHook(void* pTarget, void* pDetour, void** ppOriginal) {
    return MH_UNKNOWN;
}

// Creates a hook for the specified API function, in disabled state.
// Parameters:
//   pszModule   [in]  A pointer to the loaded module name which contains the
//                     target function.
//   pszProcName [in]  A pointer to the target function name, which will be
//                     overridden by the detour function.
//   pDetour     [in]  A pointer to the detour function, which will override
//                     the target function.
//   ppOriginal  [out] A pointer to the trampoline function, which will be
//                     used to call the original target function.
//                     This parameter can be NULL.
MH_STATUS WINAPI MH_CreateHookApi(const WCHAR* pszModule, const char* pszProcName, void* pDetour, void** ppOriginal) {
    return MH_UNKNOWN;
}

// Creates a hook for the specified API function, in disabled state.
// Parameters:
//   pszModule   [in]  A pointer to the loaded module name which contains the
//                     target function.
//   pszProcName [in]  A pointer to the target function name, which will be
//                     overridden by the detour function.
//   pDetour     [in]  A pointer to the detour function, which will override
//                     the target function.
//   ppOriginal  [out] A pointer to the trampoline function, which will be
//                     used to call the original target function.
//                     This parameter can be NULL.
//   ppTarget    [out] A pointer to the target function, which will be used
//                     with other functions.
//                     This parameter can be NULL.
MH_STATUS WINAPI MH_CreateHookApiEx(const WCHAR* pszModule, const char* pszProcName, void* pDetour, void** ppOriginal,
                                    void** ppTarget) {
    return MH_UNKNOWN;
}

// Removes an already created hook.
// Parameters:
//   pTarget [in] A pointer to the target function.
MH_STATUS WINAPI MH_RemoveHook(void* pTarget) {
    return MH_UNKNOWN;
}

// Enables an already created hook.
// Parameters:
//   pTarget [in] A pointer to the target function.
//                If this parameter is MH_ALL_HOOKS, all created hooks are
//                enabled in one go.
MH_STATUS WINAPI MH_EnableHook(void* pTarget) {
    return MH_UNKNOWN;
}

// Disables an already created hook.
// Parameters:
//   pTarget [in] A pointer to the target function.
//                If this parameter is MH_ALL_HOOKS, all created hooks are
//                disabled in one go.
MH_STATUS WINAPI MH_DisableHook(void* pTarget) {
    return MH_UNKNOWN;
}

// Queues to enable an already created hook.
// Parameters:
//   pTarget [in] A pointer to the target function.
//                If this parameter is MH_ALL_HOOKS, all created hooks are
//                queued to be enabled.
MH_STATUS WINAPI MH_QueueEnableHook(void* pTarget) {
    return MH_UNKNOWN;
}

// Queues to disable an already created hook.
// Parameters:
//   pTarget [in] A pointer to the target function.
//                If this parameter is MH_ALL_HOOKS, all created hooks are
//                queued to be disabled.
MH_STATUS WINAPI MH_QueueDisableHook(void* pTarget) {
    return MH_UNKNOWN;
}

// Applies all queued changes in one go.
MH_STATUS WINAPI MH_ApplyQueued() {
    return MH_UNKNOWN;
}

// Translates the MH_STATUS to its name as a string.
const char* WINAPI MH_StatusToString(MH_STATUS status) {
    return "";
}

#endif
