# Updating zopfli

Use `cmd/a-zopfli.ts` to update the amalgamated zopfli copy used by the build.

1. Pick the upstream zopfli repository URL and tag or commit hash. The current
   source is:

   ```sh
   bun cmd/a-zopfli.ts https://github.com/google/zopfli ccf9f0588d4a4509cb1040310ec122243e670ee6
   ```

   Running `bun cmd/a-zopfli.ts` without arguments uses those defaults.

2. The script checks out the requested revision under `deps/zopfli` and writes
   `ext/a-zopfli/zopflipng/zopflipng_lib.h`,
   `ext/a-zopfli/zopflipng/lodepng/lodepng.h`,
   `ext/a-zopfli/zopfli.cpp`, `ext/a-zopfli/version.txt`, and
   `ext/a-zopfli/COPYING`.
3. Review `ext/a-zopfli/version.txt`; it records the project homepage, source
   repo URL, requested revision, resolved commit SHA-1, and GitHub commit URL.
4. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

5. Build:

   ```sh
   bun cmd/build.ts -debug
   ```

The active SumatraPDF build uses only the amalgamated `ext/a-zopfli` source.
