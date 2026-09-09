# Updating UnRAR

Use `cmd/amalgam.ts -unrar` to update the amalgamated UnRAR copy used by the
build.

1. Pick the upstream revision. rarlab ships tarballs rather than git, so the
   default repo is a mirror that commits each release tarball verbatim; its tag
   numbers do not match the UnRAR version, so pick the commit by the tarball it
   names. The current source is UnRAR 7.1.6:

   ```sh
   bun cmd/amalgam.ts -unrar https://github.com/aawc/unrar b82477a7d45b6998fbcfc504a0fa59aca6345e4c
   ```

   Running `bun cmd/amalgam.ts -unrar` without further arguments uses those
   defaults.

2. The script checks out the requested revision under `deps/unrar` and writes
   `ext/a-unrar/unrar.cpp`, `dll.hpp` (the C API `src/base/Archive.cpp` uses),
   `ext/a-unrar/version.txt`, `license.txt` and `acknow.txt`.
3. Review `ext/a-unrar/version.txt`; it records the project homepage, source
   repo URL, requested revision, resolved commit SHA-1, and GitHub commit URL.
4. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

5. Build:

   ```sh
   bun cmd/build.ts -dbg
   ```

`global.cpp` is first in `unrarSources` on purpose: it defines `INCLUDEGLOBAL`
before pulling in `rar.hpp`, which is what makes `global.hpp` define
`ErrHandler` rather than declare it. `rar.hpp` is inlined once, so whichever
chunk pulls it in first decides that.

The vendored tree carried one local edit — lowercase `<powrprof.h>`,
`<sddl.h>`, `<wbemidl.h>` so the mingw cross build works on a case-sensitive
filesystem. `lowercaseWinIncludes` in `cmd/amalgam.ts` applies it now.
