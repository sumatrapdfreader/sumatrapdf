--proc/guid: UUID API from rpcdce.h
setfenv(1, require'winapi')
require'winapi.rpc_types'

ffi.cdef[[
RPC_STATUS UuidCreate (UUID* Uuid);
RPC_STATUS UuidCreateSequential (UUID* Uuid);
RPC_STATUS UuidToStringA (const UUID* Uuid, RPC_WSTR* StringUuid);
RPC_STATUS UuidFromStringA (RPC_WSTR StringUuid, UUID* Uuid);
RPC_STATUS UuidToStringW (const UUID* Uuid, RPC_WSTR* StringUuid);
RPC_STATUS UuidFromStringW (RPC_WSTR StringUuid, UUID* Uuid);
signed int UuidCompare (UUID* Uuid1, UUID* Uuid2, RPC_STATUS* Status);
RPC_STATUS UuidCreateNil (UUID* NilUuid);
int UuidEqual (UUID* Uuid1, UUID* Uuid2, RPC_STATUS* Status);
unsigned short UuidHash (UUID* Uuid, RPC_STATUS* Status);
int UuidIsNil (UUID* Uuid, RPC_STATUS* Status);
]]

function UuidCreate()
	local uuid = types.UUID()
	checkz(rpc.UuidCreate(uuid))
	return uuid
end

function UuidCreateSequential()
	local uuid = types.UUID()
	checkz(rpc.UuidCreateSequential(uuid))
	return uuid
end

function UuidCreateNil()
	local uuid = types.UUID()
	checkz(rpc.UuidCreateNil(uuid))
	return uuid
end

function UuidFromString(s, uuid)
	uuid = types.UUID(uuid)
	checkz(rpc.UuidFromStringW(wcs(s), uuid))
	return uuid
end

function UuidToString(uuid, pws)
	pws = pws or ffi.new('RPC_WSTR[1]')
	checkz(rpc.UuidToStringW(uuid, pws))
	local s = mbs(pws[0])
	rpc.RpcStringFreeW(pws)
	return s
end

function UuidCompare(uuid1, uuid2, status) --returns -1, 0, 1 for <, ==, >
	status = status or ffi.new'RPC_STATUS[1]'
	local ret = rpc.UuidCompare(uuid1, uuid2, status)
	checkz(status[0])
	return ret
end

function UuidEqual(uuid1, uuid2, status)
	status = status or ffi.new'RPC_STATUS[1]'
	local ret = rpc.UuidEqual(uuid1, uuid2, status) == 1
	checkz(status[0])
	return ret
end

function UuidHash(uuid, status)
	status = status or ffi.new'RPC_STATUS[1]'
	local ret = rpc.UuidHash(uuid, status)
	checkz(status[0])
	return ret
end

function UuidIsNil(uuid, status)
	status = status or ffi.new'RPC_STATUS[1]'
	local ret = rpc.UuidIsNil(uuid, status) == 1
	checkz(status[0])
	return ret
end

ffi.metatype('GUID', {
	__tostring = UuidToString,
	__eq = UuidEqual,
	__index = {
		compare = UuidCompare,
		hash = UuidHash,
		is_nil = UuidIsNil,
	},
})

if not ... then
	print(UuidCreate())
	print(UuidCreateSequential())
	print(UuidCreate():hash())

	assert(#tostring(UuidCreate()) == 36)
	assert(#tostring(UuidCreateSequential()) == 36)
	assert(tostring(UuidCreateNil()) == '00000000-0000-0000-0000-000000000000')

	assert(UuidCreate():is_nil() == false)
	assert(UuidCreateSequential():is_nil() == false)
	assert(UuidCreateNil():is_nil() == true)
	assert(UuidCreateSequential():compare(UuidCreateSequential()) == -1)
	assert(UuidCreate() ~= UuidCreate())
end

