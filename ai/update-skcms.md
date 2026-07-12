# Updating skcms

Use `cmd/a-skcms.ts` to update the amalgamated skcms copy used by the build.

1. Pick the upstream skcms repository URL and tag or commit hash. The current
   source is:

   ```sh
   bun cmd/a-skcms.ts https://skia.googlesource.com/skcms b2e692629c1fb19342517d7fb61f1cf83d075492
   ```

   Running `bun cmd/a-skcms.ts` without arguments uses those defaults.

2. The script checks out the requested revision under `deps/skcms` and writes
   `ext/a-skcms/skcms.h`, `ext/a-skcms/skcms.cpp`, and
   `ext/a-skcms/version.txt`.
3. Review `ext/a-skcms/version.txt`; it records the project homepage, source
   repo URL, requested revision, resolved commit SHA-1, and googlesource URL.
4. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

5. Build:

   ```sh
   bun ./cmd/build.ts
   ```

The old `ext/skcms` checkout is intentionally left in the tree for now, but the
active SumatraPDF build uses the amalgamated `ext/a-skcms` source.
