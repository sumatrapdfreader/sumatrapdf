# Updating Little CMS

Use `cmd/amalgam.ts -lcms2` to update the amalgamated Little CMS copy used by
the build. It is Artifex's `lcms2mt` fork, the thread-safe variant mupdf needs.

1. Pick the upstream repository URL and tag or commit hash. The current source
   is:

   ```sh
   bun cmd/amalgam.ts -lcms2 https://github.com/ArtifexSoftware/thirdparty-lcms2 d69c64417c4a33a4629957fc0e08f4f4c5abcc3d
   ```

   Running `bun cmd/amalgam.ts -lcms2` without further arguments uses those
   defaults. Keep the revision in sync with the lcms2 submodule of the vendored
   mupdf (see `ext/versions.txt`).

2. The script checks out the requested revision under `deps/lcms2` and writes
   `ext/a-lcms2/lcms2.c`, the public `lcms2mt.h` / `lcms2mt_plugin.h`,
   `extra_xform.h`, `ext/a-lcms2/version.txt`, `LICENSE` and `AUTHORS`.
3. Review `ext/a-lcms2/version.txt`; it records the project homepage, source
   repo URL, requested revision, resolved commit SHA-1, and GitHub commit URL.
4. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

5. Build:

   ```sh
   bun cmd/build.ts -debug
   ```

`extra_xform.h` is a chameleonic header: `cmsxform.c` includes it dozens of
times, redefining `FUNCTION_NAME` and friends each time, so it ships as a file
rather than being inlined.

A handful of file-local statics collide once every `.c` is one translation
unit. `lcms2Renames` in `cmd/amalgam.ts` gives them a per-file prefix; a new
collision after an update shows up as a redefinition error from the script's
own `cl.exe` validation.
