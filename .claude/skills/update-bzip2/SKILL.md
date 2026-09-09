---
name: update-bzip2
description: Regenerate the amalgamated bzip2 copy in ext/a-bzip2 from upstream with cmd/amalgam.ts. Use when asked to update, upgrade or re-amalgamate bzip2, or when the user runs /update-bzip2.
---

# Update amalgamated bzip2

`ext/a-bzip2/` is generated — never hand-edit it. `bun cmd/amalgam.ts -bzip2` clones upstream into
`.work/deps/bzip2`, amalgamates it, and validates the result by compiling it with `cl.exe`.

## Steps

1. Regenerate. With no further arguments it re-runs the repo and revision recorded in
   `ext/versions.txt`:

   ```sh
   bun cmd/amalgam.ts -bzip2
   ```

   To move to a different upstream revision, pass it explicitly:

   ```sh
   bun cmd/amalgam.ts -bzip2 <repo-url> <tag-or-commit>
   ```

   It writes `bzlib.h`, `bzip2.c`, `LICENSE` and `version.txt` into `ext/a-bzip2/`.
   Add `-keep` to reuse the existing checkout while iterating on the amalgamation rules.

2. Check `ext/a-bzip2/version.txt` — it records the project homepage, source repo URL, requested
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

- The libarchive build uses only the amalgamated `ext/a-bzip2` source.
