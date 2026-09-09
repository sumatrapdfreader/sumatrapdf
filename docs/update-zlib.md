# Updating zlib

Use `cmd/amalgam.ts -zlib` to update the amalgamated zlib copy used by the build.

1. Pick the upstream zlib repository URL and tag or commit hash. The current
   source is:

   ```sh
   bun cmd/amalgam.ts -zlib https://github.com/madler/zlib v1.3.2
   ```

   Running `bun cmd/amalgam.ts -zlib` without further arguments uses those defaults.

2. The script checks out the requested revision under `.work/deps/zlib` and writes
   `ext/a-zlib/zlib.h`, `ext/a-zlib/zlib.c`, `ext/a-zlib/version.txt`, and
   `ext/a-zlib/LICENSE`.
3. Review `ext/a-zlib/version.txt`; it records the project homepage, source
   repo URL, requested revision, resolved commit SHA-1, and GitHub URLs.
4. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

5. Build:

   ```sh
   bun cmd/build.ts -dbg
   ```

The active zlib build uses only the amalgamated `ext/a-zlib` source. A small
sample document is kept at `ext/a-zlib/zlib.3.pdf`.
