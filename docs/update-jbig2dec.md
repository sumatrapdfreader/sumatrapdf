# Updating jbig2dec

Use `cmd/amalgam.ts -jbig2dec` to update the amalgamated jbig2dec copy used by the
build.

1. Pick the upstream jbig2dec repository URL and tag or commit hash. The current
   source is:

   ```sh
   bun cmd/amalgam.ts -jbig2dec https://github.com/ArtifexSoftware/jbig2dec dc15c39bbbddc90f79c14563d2eb5a794106be8f
   ```

   Running `bun cmd/amalgam.ts -jbig2dec` without further arguments uses those defaults.

2. The script checks out the requested revision under `deps/jbig2dec` and writes
   `ext/a-jbig2dec/jbig2.h`, `ext/a-jbig2dec/jbig2dec.c`, and
   `ext/a-jbig2dec/version.txt`. It also re-copies the upstream `COPYING` and
   `LICENSE` files into `ext/a-jbig2dec`; they are the only copies in the tree
   and `AUTHORS` points at `ext/a-jbig2dec/COPYING`.
3. Review `ext/a-jbig2dec/version.txt`; it records the project homepage, source
   repo URL, requested revision, resolved commit SHA-1, and GitHub commit URL.
4. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

5. Build:

   ```sh
   bun cmd/build.ts -dbg
   ```
