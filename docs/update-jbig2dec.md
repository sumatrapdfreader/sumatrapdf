# Updating jbig2dec

Use `cmd/a-jbig2dec.ts` to update the amalgamated jbig2dec copy used by the
build.

1. Pick the upstream jbig2dec repository URL and tag or commit hash. The current
   source is:

   ```sh
   bun cmd/a-jbig2dec.ts https://github.com/ArtifexSoftware/jbig2dec dc15c39bbbddc90f79c14563d2eb5a794106be8f
   ```

   Running `bun cmd/a-jbig2dec.ts` without arguments uses those defaults.

2. The script checks out the requested revision under `deps/jbig2dec` and writes
   `ext/a-jbig2dec/jbig2.h`, `ext/a-jbig2dec/jbig2dec.c`, and
   `ext/a-jbig2dec/version.txt`.
3. Review `ext/a-jbig2dec/version.txt`; it records the project homepage, source
   repo URL, requested revision, resolved commit SHA-1, and GitHub commit URL.
4. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

5. Build:

   ```sh
   bun ./cmd/build.ts
   ```

The old `ext/jbig2dec` checkout is intentionally left in the tree for now, but
the active MuPDF build uses the amalgamated `ext/a-jbig2dec` source.
