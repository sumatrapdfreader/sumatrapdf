--standard C types from various headers from mingw.
--the following types are ffi built-ins: int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t,
--uint64_t, intptr_t, uintptr_t, ptrdiff_t, size_t and wchar_t.
local ffi = require'ffi'

--from stddef.h

--wint_t is defined as short per mingw for compatibility with MS.
ffi.cdef[[
typedef short unsigned int wint_t;
]]

--result of `cpp stdint.h` from mingw

ffi.cdef[[
typedef signed char int_least8_t;
typedef unsigned char uint_least8_t;
typedef short int_least16_t;
typedef unsigned short uint_least16_t;
typedef int int_least32_t;
typedef unsigned uint_least32_t;
typedef long long int_least64_t;
typedef unsigned long long uint_least64_t;
typedef signed char int_fast8_t;
typedef unsigned char uint_fast8_t;
typedef short int_fast16_t;
typedef unsigned short uint_fast16_t;
typedef int int_fast32_t;
typedef unsigned int uint_fast32_t;
typedef long long int_fast64_t;
typedef unsigned long long uint_fast64_t;
typedef long long intmax_t;
typedef unsigned long long uintmax_t;
]]

--result of `cpp sys/types.h` from mingw

ffi.cdef[[
typedef long __time32_t;
typedef long long __time64_t;
typedef long _off_t;
typedef _off_t off_t;
typedef unsigned int _dev_t;
typedef _dev_t dev_t;
typedef short _ino_t;
typedef _ino_t ino_t;
typedef int _pid_t;
typedef _pid_t pid_t;
typedef unsigned short _mode_t;
typedef _mode_t mode_t;
typedef int _sigset_t;
typedef _sigset_t sigset_t;
typedef int _ssize_t;
typedef _ssize_t ssize_t;
typedef long long fpos64_t;
typedef long long off64_t;
typedef unsigned int useconds_t;
]]

