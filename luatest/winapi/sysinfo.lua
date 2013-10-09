--core/sysinfo: system info API
setfenv(1, require'winapi')

ffi.cdef[[
typedef struct _SYSTEM_INFO {
    union {
        DWORD dwOemId;
        struct {
            WORD wProcessorArchitecture;
            WORD wReserved;
        };
    };
    DWORD dwPageSize;
    LPVOID lpMinimumApplicationAddress;
    LPVOID lpMaximumApplicationAddress;
    DWORD_PTR dwActiveProcessorMask;
    DWORD dwNumberOfProcessors;
    DWORD dwProcessorType;
    DWORD dwAllocationGranularity;
    WORD wProcessorLevel;
    WORD wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

void GetSystemInfo(LPSYSTEM_INFO lpSystemInfo);
]]

function GetSystemInfo(sysinfo)
	local sysinfo = sysinfo or ffi.new'SYSTEM_INFO'
	C.GetSystemInfo(sysinfo)
	return sysinfo
end
