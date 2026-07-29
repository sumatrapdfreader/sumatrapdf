# Updating extract

Use `cmd/a-extract.ts` to update the amalgamated extract copy used by the
build.

1. Pick the upstream extract repository URL and tag or commit hash. The current
   source is:

   ```sh
   bun cmd/a-extract.ts https://github.com/ArtifexSoftware/extract 8750ac39c30a0d65119b426b5a491c5b8e8bf674
   ```

   Running `bun cmd/a-extract.ts` without arguments uses those defaults.

2. The script checks out the requested revision under `deps/extract` and writes
   `ext/a-extract/extract/*.h`, `ext/a-extract/memento.h`,
   `ext/a-extract/extract.c`, and `ext/a-extract/version.txt`.
3. Review `ext/a-extract/version.txt`; it records the project homepage, source
   repo URL, requested revision, resolved commit SHA-1, and GitHub commit URL.
4. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

5. Build:

   ```sh
   bun ./cmd/build.ts
   ```

The old `ext/extract` checkout is intentionally left in the tree for now, but
the active MuPDF build uses the amalgamated `ext/a-extract` source.
