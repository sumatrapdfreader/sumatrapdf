# Updating FreeType

Use `cmd/amalgam.ts -freetype` to update the amalgamated FreeType copy used by
the build.

1. Pick the upstream FreeType repository URL and tag or commit hash. The current
   source is:

   ```sh
   bun cmd/amalgam.ts -freetype https://github.com/ArtifexSoftware/thirdparty-freetype2 0a0221a1347e2f1e07c395263540026e9a0aa7c7
   ```

   Running `bun cmd/amalgam.ts -freetype` without further arguments uses those
   defaults. Keep the revision in sync with the freetype submodule of the
   vendored mupdf (see `ext/versions.txt`).

2. The script checks out the requested revision under `.work/deps/freetype` and writes
   `ext/a-freetype/freetype.c`, the `ext/a-freetype/include` header tree,
   `ext/a-freetype/version.txt`, `ext/a-freetype/LICENSE.TXT` and the
   `ext/a-freetype/docs` license notices. The output directory is wiped first,
   so headers dropped upstream don't linger.
3. Review `ext/a-freetype/version.txt`; it records the project homepage, source
   repo URL, requested revision, resolved commit SHA-1, and GitHub commit URL.
4. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

5. Build:

   ```sh
   bun cmd/build.ts -dbg
   ```

Only the modules mupdf needs are amalgamated (no autofit, bdf, cache, pcf, pfr,
sdf, svg, type42 or winfonts). That list lives in `cmd/amalgam.ts`; adding a
module means adding its aggregate `.c` there.

The module and option set still comes from mupdf's `slimftmodules.h` /
`slimftoptions.h` in `ext/mupdf/scripts/freetype`, selected by the
`FT_CONFIG_MODULES_H` / `FT_CONFIG_OPTIONS_H` build defines, so consumers of the
FreeType headers must keep defining them.
