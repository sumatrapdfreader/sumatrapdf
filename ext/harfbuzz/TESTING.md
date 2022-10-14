## Build and Test

```shell
meson build
ninja -Cbuild
meson test -Cbuild
```

### Debug with GDB

```shell
meson test -Cbuild --gdb testname
```

## Build and Run

Depending on what area you are working in change or add `HB_DEBUG_<whatever>`.
Values defined in `hb-debug.hh`.

```shell
CPPFLAGS='-DHB_DEBUG_SUBSET=100' meson setup build --reconfigure
meson test -C build
```

### Run tests with asan

```shell
meson setup build -Db_sanitize=address --reconfigure
meson compile -C build
meson test -C build
```

### Enable Debug Logging

```shell
CPPFLAGS=-DHB_DEBUG_SUBSET=100 meson build --reconfigure
ninja -C build
```

## Test with the Fuzzer

FOr fuzzing, see `test/fuzzing/README.md`.

## Profiling

For profiling, see `perf/README.md`.

