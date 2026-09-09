# Updating bzip2

Use `cmd/amalgam.ts -bzip2` to update the amalgamated bzip2 copy used by the build.

1. Pick the upstream bzip2 repository URL and tag or commit hash. The current
   source is:

   ```sh
   bun cmd/amalgam.ts -bzip2 git://sourceware.org/git/bzip2.git bzip2-1.0.8
   ```

   Running `bun cmd/amalgam.ts -bzip2` without further arguments uses those defaults.

2. The script checks out the requested revision under `.work/deps/bzip2` and writes
   `ext/a-bzip2/bzlib.h`, `ext/a-bzip2/bzip2.c`, `ext/a-bzip2/version.txt`,
   and `ext/a-bzip2/LICENSE`.
3. Review `ext/a-bzip2/version.txt`; it records the project homepage, source
   repo URL, requested revision, and resolved commit SHA-1.
4. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

5. Build:

   ```sh
   bun cmd/build.ts -dbg
   ```

The active libarchive build uses only the amalgamated `ext/a-bzip2` source.
