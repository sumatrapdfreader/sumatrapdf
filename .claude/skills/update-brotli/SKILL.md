---
name: update-brotli
description: Regenerate the amalgamated Brotli copy in ext/a-brotli from upstream with cmd/amalgam.ts. Use when asked to update, upgrade or re-amalgamate Brotli, or when the user runs /update-brotli.
---

# Update amalgamated Brotli

`ext/a-brotli/` is generated — never hand-edit it. `bun cmd/amalgam.ts -brotli` clones upstream into
`.work/deps/brotli`, amalgamates it, and validates the result by compiling it with `cl.exe`.

## Steps

1. Regenerate. With no further arguments it re-runs the repo and revision recorded in
   `ext/versions.txt`:

   ```sh
   bun cmd/amalgam.ts -brotli
   ```

   To move to a different upstream revision, pass it explicitly:

   ```sh
   bun cmd/amalgam.ts -brotli <repo-url> <tag-or-commit>
   ```

   Keep the revision in sync with the brotli submodule of the vendored mupdf (`ext/versions.txt`).

   It writes `brotli/*.h` (the headers keep their directory so `#include <brotli/decode.h>` works), `brotli.c`, `LICENSE` and `version.txt` into `ext/a-brotli/`.
   Add `-keep` to reuse the existing checkout while iterating on the amalgamation rules.

2. Check `ext/a-brotli/version.txt` — it records the project homepage, source repo URL, requested
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

- The `*_inc.h` files are X-macro fragments with no include guard; the inliner expands them at every use, which is what they are for.
- `compress_fragment_two_pass.c` is a near-copy of `compress_fragment.c`, so most of `brotliRenames` in `cmd/amalgam.ts` is one half of that pair. A new collision after an update shows up as a redefinition error from the script's own `cl.exe` validation — add the offending name to `brotliRenames`.
