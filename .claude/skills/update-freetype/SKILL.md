---
name: update-freetype
description: Regenerate the amalgamated FreeType copy in ext/a-freetype from upstream with cmd/amalgam.ts. Use when asked to update, upgrade or re-amalgamate FreeType, or when the user runs /update-freetype.
---

# Update amalgamated FreeType

`ext/a-freetype/` is generated — never hand-edit it. `bun cmd/amalgam.ts -freetype` clones upstream into
`.work/deps/freetype`, amalgamates it, and validates the result by compiling it with `cl.exe`.

## Steps

1. Regenerate. With no further arguments it re-runs the repo and revision recorded in
   `ext/versions.txt`:

   ```sh
   bun cmd/amalgam.ts -freetype
   ```

   To move to a different upstream revision, pass it explicitly:

   ```sh
   bun cmd/amalgam.ts -freetype <repo-url> <tag-or-commit>
   ```

   Keep the revision in sync with the freetype submodule of the vendored mupdf (`ext/versions.txt`).

   It writes `freetype.c`, the `include/` header tree, `LICENSE.TXT` and the `docs` license notices. The output directory is wiped first, so headers dropped upstream don't linger and `version.txt` into `ext/a-freetype/`.
   Add `-keep` to reuse the existing checkout while iterating on the amalgamation rules.

2. Check `ext/a-freetype/version.txt` — it records the project homepage, source repo URL, requested
   revision, resolved commit SHA-1 and GitHub commit URL. Confirm it is the revision you meant.

3. Regenerate the Visual Studio projects, since the file set may have changed:

   ```sh
   bun cmd/premake.ts
   ```

4. Build:

   ```sh
   bun cmd/build.ts -dbg
   ```

5. Review the diff. Don't commit without a passing build.

## Notes

- Only the modules mupdf needs are amalgamated (no autofit, bdf, cache, pcf, pfr, sdf, svg, type42 or winfonts). That list lives in `cmd/amalgam.ts`; adding a module means adding its aggregate `.c` there.
- The module and option set still comes from mupdf's `slimftmodules.h` / `slimftoptions.h` in `ext/mupdf/scripts/freetype`, selected by the `FT_CONFIG_MODULES_H` / `FT_CONFIG_OPTIONS_H` build defines, so consumers of the FreeType headers must keep defining them.
