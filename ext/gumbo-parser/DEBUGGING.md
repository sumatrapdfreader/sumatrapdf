These are a couple of debugging notes that may be helpful for anyone developing
Gumbo or trying to diagnose a tricky problem.  They will probably not be
necessary for normal clients of this library - Gumbo is relatively stable, and
bugs are often rare and obscure.  However, they're handy to have as a reference,
and may also provide useful Google fodder to people searching for these tools.

Standard disclaimer: I use all of these techniques on my Ubuntu 14.04 computer
with gcc 4.8.2, clang 3.4, and gtest 1.6.0, but make no warranty about them
working on other systems.  In particular, they're almost certain not to work on
Windows.

Debug output
============

Gumbo has a compile-time switch to dump lots of debug output onto stdout.
Compile with the GUMBO_DEBUG define enabled:

```bash
$ make CFLAGS='-DGUMBO_DEBUG'
```

Note that this spits *a lot* of debug information to the console and makes the
program run significantly slower, so it's usually helpful to isolate only the
specific HTML file or fragment that causes the bug.  It lets us trace the
operation of each of the tokenizer & parser's state machines in depth, though.

Unit tests
==========

As mentioned in the README, Gumbo relies on [googletest][] for unit tests.
Unzip the gtest ZIP distribution inside the Gumbo root and rename it 'gtest'.
'make check' runs the tests, as normal.

```bash
$ make check
$ cat test-suite.log
```

If you need to debug a core dump, you'll probably want to run the test binary
directly:

```bash
$ ulimit -c unlimited
$ make check
$ .libs/lt-gumbo_test
$ gdb .libs/lt-gumbo_test core
```

The same goes for core dumps in other example binaries.

To run only a single unit test, pass the --gtest_filter='TestName' flag to the
lt-gumbo_test binary.

Assertions
==========

Gumbo relies pretty heavily on assertions.  By default they're enabled at
run-time: to turn them off, define NDEBUG:

```bash
$ make CFLAGS='-DNDEBUG'
```

ASAN
====

Google's [address-sanitizer][] is a helpful tool that lets you find memory
errors with relatively low overhead: enough that you can often run it in
production.  Enabling it for C/C++ binaries is pretty standard and described on
the ASAN documentation pages.  It requires Clang >=3.1 or GCC >= 4.8.

```bash
$ make \
    CFLAGS='-fsanitize=address -fno-omit-frame-pointer -fno-inline' \
    LDFLAGS='-fsanitize=address'
```

ASAN can also be used when Gumbo is compiled as a shared library and linked into
a scripting language via FFI, but this use-case is unsupported by the ASAN
authors.  To do it, use LD_PRELOAD to ensure the ASAN runtime support is
included in the process:

```bash
$ LD_PRELOAD=libasan.so.0 python -c 'import gumbo; gumbo.parse(problem_text)'
```

Getting clean stack traces from this requires the use of the llvm-symbolizer
binary, included with clang:

```bash
$ export ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer-3.4
$ export ASAN_OPTIONS=symbolize=1
$ LD_PRELOAD=libasan.so.0 python -c \
  'import gumbo; gumbo.parse(problem_text)' 2>&1 | head -100
$ killall llvm-symbolizer-3.4
$ killall llvm-symbolizer-3.4
$ killall llvm-symbolizer-3.4
```

This use case is even less officially supported than using it with dynamic
shared objects; on my machine, it led to a recursive ASAN error about a
use-after-free in llvm-symbolizer, effectively fork-bombing the machine.  Have
the killalls ready, and avoid letting the process run for too long (eg. piping
it to 'less').

[googletest]: https://code.google.com/p/googletest/
[address-sanitizer]: https://code.google.com/p/address-sanitizer/
