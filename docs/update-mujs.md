# Updating MuJS

Use `cmd/a-mujs.ts` to update the amalgamated MuJS copy used by the build.

1. Check `ext/versions.txt` for the intended upstream repository and revision.
   The current source is:

   ```sh
   bun cmd/a-mujs.ts https://github.com/ArtifexSoftware/mujs e892c9fdbbddba94e52f656ccb378ed4885e30cc
   ```

   Running `bun cmd/a-mujs.ts` without arguments uses those defaults.

2. The script checks out the requested revision under `deps/mujs` and writes
   `ext/a-mujs/mujs.h`, `ext/a-mujs/mujs.c`, and
   `ext/a-mujs/version.txt`.
3. Review `ext/a-mujs/version.txt`; it records the project homepage, source
   repo URL, requested revision, resolved commit SHA-1, and GitHub URLs.
4. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

5. Build:

   ```sh
   bun ./cmd/build.ts
   ```

The old `ext/mujs` checkout is intentionally left in the tree for now, but
the active MuJS build uses the `a-mujs` Premake project.
