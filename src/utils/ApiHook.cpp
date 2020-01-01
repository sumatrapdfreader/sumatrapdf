/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ApiHook.h"
#include "utils/PEB.h"

#define RVA2VA(type, base, rva) (type)((ULONG_PTR)base + rva)

// wtf this should be standard (stole from glibc & stackoverflow)
inline int my_narrow_stricmp(const char* a, const char* b) {
    int ca, cb;
    do {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        ca = tolower(toupper(ca));
        cb = tolower(toupper(cb));
    } while (ca == cb && ca != '\0');
    return ca - cb;
}

inline int my_wide_stricmp(const wchar_t* a, const wchar_t* b) {
    wint_t ca, cb;
    do {
        ca = (wint_t)*a++;
        cb = (wint_t)*b++;
        ca = towlower(towupper(ca));
        cb = towlower(towupper(cb));
    } while (ca == cb && ca != L'\0');
    return ca - cb;
}

class MemoryProtector {
  public:
    MemoryProtector(const uint64_t address, const uint64_t length, const DWORD prot) {
        m_address = address;
        m_length = length;
        isGood = VirtualProtect((char*)address, (SIZE_T)length, prot, &m_origProtection) != 0;
    }

    ~MemoryProtector() {
        if (m_origProtection != 0) {
            DWORD tmp;
            VirtualProtect((char*)m_address, (SIZE_T)m_length, m_origProtection, &tmp);
        }
    }

    DWORD m_origProtection = 0;

    uint64_t m_address = 0;
    uint64_t m_length = 0;
    bool isGood = false;
};

static IMAGE_THUNK_DATA* FindIatThunkInModule(void* moduleBase, const std::string_view& dllName,
                                              const std::string_view& apiName) {
    AssertCrash(moduleBase != nullptr);
    if (moduleBase == nullptr)
        return nullptr;

    IMAGE_DOS_HEADER* pDos = (IMAGE_DOS_HEADER*)moduleBase;
    IMAGE_NT_HEADERS* pNT = RVA2VA(IMAGE_NT_HEADERS*, moduleBase, pDos->e_lfanew);
    IMAGE_DATA_DIRECTORY* pDataDir = (IMAGE_DATA_DIRECTORY*)pNT->OptionalHeader.DataDirectory;

    if (pDataDir[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress == NULL) {
        // ErrorLog::singleton().push("PEs without import tables are unsupported", ErrorLevel::SEV);
        return nullptr;
    }

    IMAGE_IMPORT_DESCRIPTOR* pImports =
        (IMAGE_IMPORT_DESCRIPTOR*)RVA2VA(uintptr_t, moduleBase, pDataDir[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

    // import entry with null fields marks end
    for (uint_fast16_t i = 0; pImports[i].Name != NULL; i++) {
        const char* s = RVA2VA(PCHAR, moduleBase, pImports[i].Name);
        if (my_narrow_stricmp(s, dllName.data()) != 0)
            continue;

        // Original holds the API Names
        PIMAGE_THUNK_DATA pOriginalThunk =
            (PIMAGE_THUNK_DATA)RVA2VA(uintptr_t, moduleBase, pImports[i].OriginalFirstThunk);

        // FirstThunk is overwritten by loader with API addresses, we change this
        PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)RVA2VA(uintptr_t, moduleBase, pImports[i].FirstThunk);

        if (!pOriginalThunk) {
            // ErrorLog::singleton().push("IAT's without valid original thunk are un-supported", ErrorLevel::SEV);
            return nullptr;
        }

        // Table is null terminated, increment both tables
        for (; pOriginalThunk->u1.Ordinal != NULL; pOriginalThunk++, pThunk++) {
            if (IMAGE_SNAP_BY_ORDINAL(pOriginalThunk->u1.Ordinal)) {
                // printf("Import By Ordinal:[Ordinal:%d]\n", IMAGE_ORDINAL(pOriginalThunk->u1.Ordinal));
                continue;
            }

            IMAGE_IMPORT_BY_NAME* pImport =
                (IMAGE_IMPORT_BY_NAME*)RVA2VA(uintptr_t, moduleBase, pOriginalThunk->u1.AddressOfData);

            auto name = (const char*)(&pImport->Name[0]);
            if (my_narrow_stricmp(name, apiName.data()) != 0)
                continue;

            return pThunk;
        }
    }

    // ErrorLog::singleton().push("Thunk not found before end of IAT", ErrorLevel::SEV);
    return nullptr;
}

static IMAGE_THUNK_DATA* FindIatThunk(const std::string_view& dllName, const std::string_view& apiName,
                                      const std::wstring moduleName = L"") {
#if defined(_WIN64)
    PEB* peb = (PPEB)__readgsqword(0x60);
#else
    PEB* peb = (PPEB)__readfsdword(0x30);
#endif

    IMAGE_THUNK_DATA* pThunk = nullptr;
    PEB_LDR_DATA* ldr = (PPEB_LDR_DATA)peb->Ldr;

    // find loaded module from peb
    for (LDR_DATA_TABLE_ENTRY* dte = (LDR_DATA_TABLE_ENTRY*)ldr->InLoadOrderModuleList.Flink; dte->DllBase != NULL;
         dte = (LDR_DATA_TABLE_ENTRY*)dte->InLoadOrderLinks.Flink) {
        // TODO: create stricmp for UNICODE_STRING because this is really bad for performance
        std::wstring baseModuleName(dte->BaseDllName.Buffer, dte->BaseDllName.Length / sizeof(wchar_t));

        // try all modules if none given, otherwise only try specified
        if (!moduleName.empty() && (my_wide_stricmp(baseModuleName.c_str(), moduleName.c_str()) != 0))
            continue;

        pThunk = FindIatThunkInModule(dte->DllBase, dllName, apiName);
        if (pThunk != nullptr)
            return pThunk;
    }

    if (pThunk == nullptr) {
        // ErrorLog::singleton().push("Failed to find thunk for api from requested dll", ErrorLevel::SEV);
    }
    return pThunk;
}

IatHook::IatHook(const std::string_view& dllName, const std::string_view& apiName, const char* fnCallback,
                 uint64_t* userOrigVar, const std::wstring& moduleName)
    : IatHook(dllName, apiName, (uint64_t)fnCallback, userOrigVar, moduleName) {
}

IatHook::IatHook(const std::string_view& dllName, const std::string_view& apiName, const uint64_t fnCallback,
                 uint64_t* userOrigVar, const std::wstring& moduleName)
    : m_moduleName(moduleName),
      m_dllName(dllName),
      m_apiName(apiName),
      m_userOrigVar(userOrigVar),
      m_fnCallback(fnCallback) {
}

bool IatHook::hook() {
    AssertCrash(m_userOrigVar != nullptr);
    IMAGE_THUNK_DATA* pThunk = FindIatThunk(m_dllName, m_apiName);
    if (pThunk == nullptr)
        return false;

    // IAT is by default a writeable section
    MemoryProtector prot((uint64_t)&pThunk->u1.Function, sizeof(uintptr_t), PAGE_READWRITE);
    m_origFunc = (uint64_t)pThunk->u1.Function;
    pThunk->u1.Function = (uintptr_t)m_fnCallback;
    m_hooked = true;
    *m_userOrigVar = m_origFunc;
    return true;
}

bool IatHook::unHook() {
    AssertCrash(m_userOrigVar != nullptr);
    AssertCrash(m_hooked);
    if (!m_hooked)
        return false;

    IMAGE_THUNK_DATA* pThunk = FindIatThunk(m_dllName, m_apiName);
    if (pThunk == nullptr)
        return false;

    MemoryProtector prot((uint64_t)&pThunk->u1.Function, sizeof(uintptr_t), PAGE_READWRITE);
    pThunk->u1.Function = (uintptr_t)m_origFunc;
    m_hooked = false;
    *m_userOrigVar = NULL;
    return true;
}
