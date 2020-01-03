## Build & Run

Depending on what area you are working in change or add `HB_DEBUG_<whatever>`.
Values defined in `hb-debug.hh`.

```shell
# quick sanity check
time (make -j4 CPPFLAGS='-DHB_DEBUG_SUBSET=100' \
  && (make -j4 -C test/api check || cat test/api/test-suite.log))

# slower sanity check
time (make -j4 CPPFLAGS='-DHB_DEBUG_SUBSET=100' \
   && make -j4 -C src check \
   && make -j4 -C test/api check \
   && make -j4 -C test/subset check)

# confirm you didn't break anything else
time (make -j4 CPPFLAGS='-DHB_DEBUG_SUBSET=100' \
  && make -j4 check)

# often catches files you didn't add, e.g. test fonts to EXTRA_DIST
make distcheck
```

### Run tests with asan

**NOTE**: this sometimes yields harder to read results than the full fuzzer

```shell
# For nice symbols tell asan how to symoblize. Note that it doesn't like versioned copies like llvm-symbolizer-3.8
# export ASAN_SYMBOLIZER_PATH=path to version-less llvm-symbolizer
# ex
export ASAN_SYMBOLIZER_PATH=/usr/lib/llvm-3.8/bin/llvm-symbolizer

./configure CC=clang CXX=clang++ CPPFLAGS=-fsanitize=address LDFLAGS=-fsanitize=address
# make/run tests as usual
```

### Debug with GDB

```
cd ./util
../libtool --mode=execute gdb --args ./hb-subset ...
```

### Enable Debug Logging

```shell
# make clean if you previously build w/o debug logging
make CPPFLAGS=-DHB_DEBUG_SUBSET=100
```

## Build and Test via CMake

Note: You'll need to first install ninja-build via apt-get.

```shell
cd harfbuzz
mkdir buid
cmake -DHB_CHECK=ON -Bbuild -H. -GNinja && ninja -Cbuild && CTEST_OUTPUT_ON_FAILURE=1 ninja -Cbuild test
```
## Test with the Fuzzer

```shell
# push your changs to a branch on googlefonts/harfbuzz
# In a local copy of oss-fuzz, edit projects/harfbuzz/Dockerfile
# Change the git clone to pull your branch

# Do this periodically
sudo python infra/helper.py build_image harfbuzz

# Do these to update/run
sudo python infra/helper.py build_fuzzers --sanitizer address harfbuzz
sudo python infra/helper.py run_fuzzer harfbuzz hb-subset-fuzzer
```

## Profiling

```
make clean
./configure CXXFLAGS="-fno-omit-frame-pointer -g"
make
perf record -o <perf output file> -g <command to run>
perf report -i<perf output file>
```

