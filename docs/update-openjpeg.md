# Updating OpenJPEG

Use `cmd/a-openjpeg.ts` to update the amalgamated OpenJPEG copy used by the
build.

1. Pick the OpenJPEG repository URL and tag or commit hash. The current source is:

   ```sh
   bun cmd/a-openjpeg.ts https://github.com/ArtifexSoftware/thirdparty-openjpeg 957029eb875eee1118743f200cb86da9d8289de2
   ```

   Running `bun cmd/a-openjpeg.ts` without arguments uses those defaults.

2. The script checks out the requested revision under `deps/openjpeg` and writes
   `ext/a-openjpeg/*.h`, `ext/a-openjpeg/openjpeg.c`,
   `ext/a-openjpeg/version.txt`, and `ext/a-openjpeg/LICENSE`.
3. Review `ext/a-openjpeg/version.txt`; it records the project homepage, source
   repo URL, requested revision, resolved commit SHA-1, and GitHub commit URL.
4. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

5. Build:

   ```sh
   bun cmd/build.ts -debug
   ```

The active MuPDF build uses only the amalgamated `ext/a-openjpeg` source.
