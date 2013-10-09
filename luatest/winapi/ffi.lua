--ffi: load the ffi and bit libs and check the platform.
setfenv(1, require'winapi.namespace')

--make ffi and bit namespaces available within the winapi namespace
ffi = require'ffi'
bit = require'bit'
C = ffi.C

assert(ffi.abi'32bit', 'only known to work on 32bit platforms')
assert(ffi.abi'win', 'platform not Windows')
