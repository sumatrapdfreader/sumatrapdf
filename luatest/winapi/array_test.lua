setfenv(1, require'winapi')

local a = arrays.int32_t(2) --n
assert(ffi.sizeof(a) == 2 * 4)
assert(a[0] == 0)
assert(a[1] == 0)

local a = arrays.int32_t(3, 2) --n, a1 repeated
assert(ffi.sizeof(a) == 3 * 4)
assert(a[0] == 2)
assert(a[1] == 2)
assert(a[2] == 2)

local a = arrays.int32_t(3, 2, 3) --n, a1, ...; rest is 0 padded
assert(ffi.sizeof(a) == 3 * 4)
assert(a[0] == 2)
assert(a[1] == 3)
assert(a[2] == 0) --zeroed

local a = arrays.int32_t(4, {2,nil,3}) --n, a1, ...; from first nil we get garbage
assert(ffi.sizeof(a) == 4 * 4)
assert(a[0] == 2)
--assert(a[1] == 0) --garbage
--assert(a[2] == 0) --garbage
--assert(a[3] == 0) --garbage

local a = arrays.int32_t{2,3} --t, size is #
assert(ffi.sizeof(a) == 2 * 4)
assert(a[0] == 2)
assert(a[1] == 3)

ffi.cdef'typedef struct point_ { int x, y; } point;'

local a = arrays.point{{2,3},{3,2}}
assert(ffi.sizeof(a) == 2 * ffi.sizeof'point')
assert(a[0].x == 2)
assert(a[0].y == 3)
assert(a[1].x == 3)
assert(a[1].y == 2)

local a = arrays.point(3, {{2,3},{3,2}})
assert(a[0].x == 2)
assert(a[0].y == 3)
assert(a[1].x == 3)
assert(a[1].y == 2)
--assert(a[2].x == 0) --garbage
--assert(a[2].y == 0) --garbage

local a = arrays.point(2, {{2,3}})
assert(a[0].x == 2)
assert(a[0].y == 3)
--assert(a[1].x == 0) --garbage
--assert(a[1].y == 0) --garbage

