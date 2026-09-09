---
name: update-jbig2dec
description: Regenerate the amalgamated jbig2dec copy in ext/a-jbig2dec from upstream with cmd/amalgam.ts. Use when asked to update, upgrade or re-amalgamate jbig2dec, or when the user runs /update-jbig2dec.
---

# Update amalgamated jbig2dec

`ext/a-jbig2dec/` is generated — never hand-edit it. `bun cmd/amalgam.ts -jbig2dec` clones upstream into
`.work/deps/jbig2dec`, amalgamates it, and validates the result by compiling it with `cl.exe`.

## Steps

1. Regenerate. With no further arguments it re-runs the repo and revision recorded in
   `ext/versions.txt`:

   ```sh
   bun cmd/amalgam.ts -jbig2dec
   ```

   To move to a different upstream revision, pass it explicitly:

   ```sh
   bun cmd/amalgam.ts -jbig2dec <repo-url> <tag-or-commit>
   ```

   It writes `jbig2.h`, `jbig2dec.c`, `COPYING`, `LICENSE` and `version.txt` into `ext/a-jbig2dec/`.
   Add `-keep` to reuse the existing checkout while iterating on the amalgamation rules.

2. Check `ext/a-jbig2dec/version.txt` — it records the project homepage, source repo URL, requested
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

- `COPYING` and `LICENSE` are the only copies in the tree — `AUTHORS` points at `ext/a-jbig2dec/COPYING`.
