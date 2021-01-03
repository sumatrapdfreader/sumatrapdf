/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// based on https://github.com/TsudaKageyu/minhook
// only supports 64 bit and all code in a single file

// MinHook Error Codes.
typedef enum MH_STATUS {
    // Unknown error. Should not be returned.
    MH_UNKNOWN = -1,

    // Successful.
    MH_OK = 0,

    // The hook for the specified target function is already created.
    MH_ERROR_ALREADY_CREATED,

    // The hook for the specified target function is already enabled.
    MH_ERROR_ENABLED,

    // The specified target function cannot be hooked.
    MH_ERROR_UNSUPPORTED_FUNCTION,

    // Failed to allocate memory.
    MH_ERROR_MEMORY_ALLOC,

    // Failed to change the memory protection.
    MH_ERROR_MEMORY_PROTECT,
} MH_STATUS;

struct HOOK_ENTRY {
    // fill those before calling MH_CreateHooks
    LPVOID pTarget; // Address of the target function.
    LPVOID pDetour; // Address of the detour or relay function.

    // filled by MH_CreateHooks
    LPVOID pTrampoline; // Address of the trampoline function i.e. original.

    LPVOID* ppOrig;

    // private data
    u8 backup[8]; // Original prologue of the target function.

    u8 patchAbove : 1;  // Uses the hot patch area.
    u8 isEnabled : 1;   // Enabled.
    u8 queueEnable : 1; // Queued for enabling/disabling when != isEnabled.

    UINT nIP : 4; // Count of the instruction boundaries.
    u8 oldIPs[8]; // Instruction boundaries of the target function.
    u8 newIPs[8]; // Instruction boundaries of the trampoline function.
};

LPVOID GetProcInDll(const char* dllName, const char* procName);

MH_STATUS WINAPI MH_Initialize();
MH_STATUS WINAPI MH_Uninitialize(HOOK_ENTRY* pHooks, int nHooks);

MH_STATUS WINAPI MH_CreateHooks(HOOK_ENTRY* pHooks, int nHooks);
MH_STATUS WINAPI MH_EnableOrDisableHooks(HOOK_ENTRY* pHooks, int nHooks, BOOL enable);

MH_STATUS WINAPI MH_ApplyQueued(HOOK_ENTRY* pHooks, int nHooks);

const char* WINAPI MH_StatusToString(MH_STATUS status);
