---
name: update-gumbo
description: Regenerate the amalgamated Gumbo copy in ext/a-gumbo from upstream with cmd/amalgam.ts. Use when asked to update, upgrade or re-amalgamate Gumbo, or when the user runs /update-gumbo.
---

# Update amalgamated Gumbo

`ext/a-gumbo/` is generated — never hand-edit it. `bun cmd/amalgam.ts -gumbo` clones upstream into
`.work/deps/gumbo`, amalgamates it, and validates the result by compiling it with `cl.exe`.

## Steps

1. Regenerate. With no further arguments it re-runs the repo and revision recorded in
   `ext/versions.txt`:

   ```sh
   bun cmd/amalgam.ts -gumbo
   ```

   To move to a different upstream revision, pass it explicitly:

   ```sh
   bun cmd/amalgam.ts -gumbo <repo-url> <tag-or-commit>
   ```

   It writes `gumbo.h`, `gumbo.c` and `version.txt` into `ext/a-gumbo/`.
   Add `-keep` to reuse the existing checkout while iterating on the amalgamation rules.

2. Check `ext/a-gumbo/version.txt` — it records the project homepage, source repo URL, requested
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

- The output is stripped of comments and local `#include "..."` directives are expanded in place, because Gumbo has include fragments such as `tag_enum.h` and `tag_gperf.h` that are only valid in their original contexts. Expect a large, unreadable diff.
