/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "ApiHook.h"
#include "PEB.h"

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

// unsafe enum by design to allow binary OR
enum ProtFlag : std::uint8_t {
    UNSET = 0, // Value means this give no information about protection state (un-read)
    X = 1 << 1,
    R = 1 << 2,
    W = 1 << 3,
    S = 1 << 4,
    P = 1 << 5,
    NONE = 1 << 6 // The value equaling the linux flag PROT_UNSET (read the prot, and the prot is unset)
};


static ProtFlag operator|(ProtFlag lhs, ProtFlag rhs) {
    return static_cast<ProtFlag>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

static bool operator&(ProtFlag lhs, ProtFlag rhs) {
    return static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs);
}


int TranslateProtection(const ProtFlag flags) {
    int NativeFlag = 0;
    if (flags == ProtFlag::X)
        NativeFlag = PAGE_EXECUTE;

    if (flags == ProtFlag::R)
        NativeFlag = PAGE_READONLY;

    if (flags == ProtFlag::W || (flags == (ProtFlag::R | ProtFlag::W)))
        NativeFlag = PAGE_READWRITE;

    if ((flags & ProtFlag::X) && (flags & ProtFlag::R))
        NativeFlag = PAGE_EXECUTE_READ;

    if ((flags & ProtFlag::X) && (flags & ProtFlag::W))
        NativeFlag = PAGE_EXECUTE_READWRITE;

    if ((flags & ProtFlag::X) && (flags & ProtFlag::R) && (flags & ProtFlag::W))
        NativeFlag = PAGE_EXECUTE_READWRITE;

    if (flags & ProtFlag::NONE)
        NativeFlag = PAGE_NOACCESS;
    return NativeFlag;
}

ProtFlag TranslateProtection(const int prot) {
    ProtFlag flags = ProtFlag::UNSET;
    switch (prot) {
        case PAGE_EXECUTE:
            flags = flags | ProtFlag::X;
            break;
        case PAGE_READONLY:
            flags = flags | ProtFlag::R;
            break;
        case PAGE_READWRITE:
            flags = flags | ProtFlag::W;
            flags = flags | ProtFlag::R;
            break;
        case PAGE_EXECUTE_READWRITE:
            flags = flags | ProtFlag::X;
            flags = flags | ProtFlag::R;
            flags = flags | ProtFlag::W;
            break;
        case PAGE_EXECUTE_READ:
            flags = flags | ProtFlag::X;
            flags = flags | ProtFlag::R;
            break;
        case PAGE_NOACCESS:
            flags = flags | ProtFlag::NONE;
            break;
    }
    return flags;
}

class MemoryProtector {
  public:
    MemoryProtector(const uint64_t address, const uint64_t length, const ProtFlag prot,
                    bool unsetOnDestroy = true) {
        m_address = address;
        m_length = length;
        unsetLater = unsetOnDestroy;

        m_origProtection = ProtFlag::UNSET;
        m_origProtection = protect(address, length, TranslateProtection(prot));
    }

    ProtFlag originalProt() { return m_origProtection; }

    bool isGood() { return status; }

    ~MemoryProtector() {
        if (m_origProtection == ProtFlag::UNSET || !unsetLater)
            return;

        protect(m_address, m_length, TranslateProtection(m_origProtection));
    }

  private:
    ProtFlag protect(const uint64_t address, const uint64_t length, int prot) {
        DWORD orig;
        DWORD dwProt = prot;
        status = VirtualProtect((char*)address, (SIZE_T)length, dwProt, &orig) != 0;
        return TranslateProtection(orig);
    }

    ProtFlag m_origProtection;

    uint64_t m_address;
    uint64_t m_length;
    bool status;
    bool unsetLater;
};

static IMAGE_THUNK_DATA* FindIatThunkInModule(void* moduleBase, const std::string& dllName,
                                                     const std::string& apiName) {
    assert(moduleBase != nullptr);
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
        if (my_narrow_stricmp(RVA2VA(PCHAR, moduleBase, pImports[i].Name), dllName.c_str()) != 0)
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

            PIMAGE_IMPORT_BY_NAME pImport =
                (PIMAGE_IMPORT_BY_NAME)RVA2VA(uintptr_t, moduleBase, pOriginalThunk->u1.AddressOfData);

            if (my_narrow_stricmp(pImport->Name, apiName.c_str()) != 0)
                continue;

            return pThunk;
        }
    }

    // ErrorLog::singleton().push("Thunk not found before end of IAT", ErrorLevel::SEV);
    return nullptr;
}

static IMAGE_THUNK_DATA* FindIatThunk(const std::string& dllName, const std::string& apiName,
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


IatHook::IatHook(const std::string& dllName, const std::string& apiName, const char* fnCallback, uint64_t* userOrigVar, const std::wstring& moduleName)
	: IatHook(dllName, apiName, (uint64_t)fnCallback, userOrigVar, moduleName)
{}

IatHook::IatHook(const std::string& dllName, const std::string& apiName, const uint64_t fnCallback, uint64_t* userOrigVar, const std::wstring& moduleName)
	: m_moduleName(moduleName)
    , m_dllName(dllName)
    , m_apiName(apiName)
    , m_userOrigVar(userOrigVar)
    , m_fnCallback(fnCallback)
{}

bool IatHook::hook() {
	assert(m_userOrigVar != nullptr);
	IMAGE_THUNK_DATA* pThunk = FindIatThunk(m_dllName, m_apiName);
	if (pThunk == nullptr)
		return false;

	// IAT is by default a writeable section
	MemoryProtector prot((uint64_t)&pThunk->u1.Function, sizeof(uintptr_t), ProtFlag::R | ProtFlag::W);
	m_origFunc = (uint64_t)pThunk->u1.Function;
	pThunk->u1.Function = (uintptr_t)m_fnCallback;
	m_hooked = true;
	*m_userOrigVar = m_origFunc;
	return true;
}

bool IatHook::unHook() {
	assert(m_userOrigVar != nullptr);
	assert(m_hooked);
	if (!m_hooked)
		return false;

	IMAGE_THUNK_DATA* pThunk = FindIatThunk(m_dllName, m_apiName);
	if (pThunk == nullptr)
		return false;

	MemoryProtector prot((uint64_t)&pThunk->u1.Function, sizeof(uintptr_t), ProtFlag::R | ProtFlag::W);
	pThunk->u1.Function = (uintptr_t)m_origFunc;
	m_hooked = false;
	*m_userOrigVar = NULL;
	return true;
}

