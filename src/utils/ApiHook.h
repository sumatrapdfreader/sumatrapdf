/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace PLH {

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

/* Used by detours class only. This doesn't live in instruction because it
 * only makes sense for specific jump instructions (perhaps re-factor instruction
 * to store inst. specific stuff when needed?). There are two classes of information for jumps
 * 1) how displacement is encoded, either relative to I.P. or Absolute
 * 2) where the jmp points, either absolutely to the destination or to a memory loc. that then points to the final dest.
 *
 * The first information is stored internal to the PLH::Instruction object. The second is this enum class that you
 * tack on via a pair or tuple when you need to tranfer that knowledge.*/
enum class JmpType { Absolute, Indirect };

enum class Mode { x86, x64 };

enum class ErrorLevel { INFO, WARN, SEV };

} // namespace PLH

PLH::ProtFlag operator|(PLH::ProtFlag lhs, PLH::ProtFlag rhs);
bool operator&(PLH::ProtFlag lhs, PLH::ProtFlag rhs);
std::ostream& operator<<(std::ostream& os, const PLH::ProtFlag v);

namespace PLH {
int TranslateProtection(const PLH::ProtFlag flags);
ProtFlag TranslateProtection(const int prot);

class MemoryProtector {
  public:
    MemoryProtector(const uint64_t address, const uint64_t length, const PLH::ProtFlag prot,
                    bool unsetOnDestroy = true) {
        m_address = address;
        m_length = length;
        unsetLater = unsetOnDestroy;

        m_origProtection = PLH::ProtFlag::UNSET;
        m_origProtection = protect(address, length, TranslateProtection(prot));
    }

    PLH::ProtFlag originalProt() { return m_origProtection; }

    bool isGood() { return status; }

    ~MemoryProtector() {
        if (m_origProtection == PLH::ProtFlag::UNSET || !unsetLater)
            return;

        protect(m_address, m_length, TranslateProtection(m_origProtection));
    }

  private:
    PLH::ProtFlag protect(const uint64_t address, const uint64_t length, int prot) {
        DWORD orig;
        DWORD dwProt = prot;
        status = VirtualProtect((char*)address, (SIZE_T)length, dwProt, &orig) != 0;
        return TranslateProtection(orig);
    }

    PLH::ProtFlag m_origProtection;

    uint64_t m_address;
    uint64_t m_length;
    bool status;
    bool unsetLater;
};

#if defined(__clang__)

#elif defined(__GNUC__) || defined(__GNUG__)
#define NOINLINE __attribute__((noinline))
#define OPTS_OFF _Pragma("GCC push_options") _Pragma("GCC optimize (\"O0\")")
#define OPTS_ON #pragma GCC pop_options
#elif defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#define OPTS_OFF __pragma(optimize("", off))
#define OPTS_ON __pragma(optimize("", on))
#endif

namespace PLH {
class IatHook {
  public:
    IatHook(const std::string& dllName, const std::string& apiName, const char* fnCallback, uint64_t* userOrigVar,
            const std::wstring& moduleName);
    IatHook(const std::string& dllName, const std::string& apiName, const uint64_t fnCallback, uint64_t* userOrigVar,
            const std::wstring& moduleName);
    bool hook();
    bool unHook();

  private:
    IMAGE_THUNK_DATA* FindIatThunk(const std::string& dllName, const std::string& apiName,
                                   const std::wstring moduleName = L"");
    IMAGE_THUNK_DATA* FindIatThunkInModule(void* moduleBase, const std::string& dllName, const std::string& apiName);

    std::string m_dllName;
    std::string m_apiName;
    std::wstring m_moduleName;

    uint64_t m_fnCallback;
    uint64_t m_origFunc;
    uint64_t* m_userOrigVar;

    bool m_hooked;
};
} // namespace PLH
