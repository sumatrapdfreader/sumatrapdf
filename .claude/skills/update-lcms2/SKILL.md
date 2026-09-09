---
name: update-lcms2
description: Regenerate the amalgamated Little CMS copy in ext/a-lcms2 from upstream with cmd/amalgam.ts. Use when asked to update, upgrade or re-amalgamate Little CMS (lcms2), or when the user runs /update-lcms2.
---

# Update amalgamated Little CMS

`ext/a-lcms2/` is generated — never hand-edit it. `bun cmd/amalgam.ts -lcms2` clones upstream into
`.work/deps/lcms2`, amalgamates it, and validates the result by compiling it with `cl.exe`. It is Artifex's `lcms2mt` fork, the thread-safe variant mupdf needs.

## Steps

1. Regenerate. With no further arguments it re-runs the repo and revision recorded in
   `ext/versions.txt`:

   ```sh
   bun cmd/amalgam.ts -lcms2
   ```

   To move to a different upstream revision, pass it explicitly:

   ```sh
   bun cmd/amalgam.ts -lcms2 <repo-url> <tag-or-commit>
   ```

   Keep the revision in sync with the lcms2 submodule of the vendored mupdf (`ext/versions.txt`).

   It writes `lcms2.c`, the public `lcms2mt.h` / `lcms2mt_plugin.h`, `extra_xform.h`, `LICENSE`, `AUTHORS` and `version.txt` into `ext/a-lcms2/`.
   Add `-keep` to reuse the existing checkout while iterating on the amalgamation rules.

2. Check `ext/a-lcms2/version.txt` — it records the project homepage, source repo URL, requested
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

- `extra_xform.h` is a chameleonic header: `cmsxform.c` includes it dozens of times, redefining `FUNCTION_NAME` and friends each time, so it ships as a file rather than being inlined.
- A handful of file-local statics collide once every `.c` is one translation unit. `lcms2Renames` in `cmd/amalgam.ts` gives them a per-file prefix. A new collision after an update shows up as a redefinition error from the script's own `cl.exe` validation — add the offending name there.
