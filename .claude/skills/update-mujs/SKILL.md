---
name: update-mujs
description: Regenerate the amalgamated MuJS copy in ext/a-mujs from upstream with cmd/amalgam.ts. Use when asked to update, upgrade or re-amalgamate MuJS, or when the user runs /update-mujs.
---

# Update amalgamated MuJS

`ext/a-mujs/` is generated — never hand-edit it. `bun cmd/amalgam.ts -mujs` clones upstream into
`.work/deps/mujs`, amalgamates it, and validates the result by compiling it with `cl.exe`.

## Steps

1. Regenerate. With no further arguments it re-runs the repo and revision recorded in
   `ext/versions.txt`:

   ```sh
   bun cmd/amalgam.ts -mujs
   ```

   To move to a different upstream revision, pass it explicitly:

   ```sh
   bun cmd/amalgam.ts -mujs <repo-url> <tag-or-commit>
   ```

   It writes `mujs.h`, `mujs.c`, `regexp.h` (used by MuPDF text search), `COPYING` and `version.txt` into `ext/a-mujs/`.
   Add `-keep` to reuse the existing checkout while iterating on the amalgamation rules.

2. Check `ext/a-mujs/version.txt` — it records the project homepage, source repo URL, requested
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

- The MuJS build uses only the amalgamated `ext/a-mujs` source.
