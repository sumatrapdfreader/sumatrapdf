--proc/rpc: RPC types
setfenv(1, require'winapi')

rpc = ffi.load'Rpcrt4'

ffi.cdef[[
typedef struct _GUID {
	uint8_t data[16];
} GUID;
typedef GUID IID;
typedef IID *LPIID;
typedef GUID CLSID;
typedef CLSID *LPCLSID;
typedef GUID FMTID;
typedef FMTID *LPFMTID;
typedef IID* REFIID;

typedef GUID UUID;
typedef long RPC_STATUS;
typedef unsigned short* RPC_WSTR;

RPC_STATUS RpcStringFreeW(RPC_WSTR* String);
]]
