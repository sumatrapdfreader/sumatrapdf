---
name: update-libwebp
description: Regenerate the amalgamated libwebp copy in ext/a-libwebp from upstream with cmd/amalgam.ts. Use when asked to update, upgrade or re-amalgamate libwebp, or when the user runs /update-libwebp.
---

# Update amalgamated libwebp

`ext/a-libwebp/` is generated — never hand-edit it. `bun cmd/amalgam.ts -libwebp` clones upstream into
`.work/deps/libwebp`, amalgamates it, and validates the result by compiling it with `cl.exe`.

## Steps

1. Regenerate. With no further arguments it re-runs the repo and revision recorded in
   `ext/versions.txt`:

   ```sh
   bun cmd/amalgam.ts -libwebp
   ```

   To move to a different upstream revision, pass it explicitly:

   ```sh
   bun cmd/amalgam.ts -libwebp <repo-url> <tag-or-commit>
   ```

   It writes `libwebp.c`, the public headers under `webp/` (they keep their directory so `#include <webp/decode.h>` works), `COPYING`, `PATENTS`, `AUTHORS` and `version.txt` into `ext/a-libwebp/`.
   Add `-keep` to reuse the existing checkout while iterating on the amalgamation rules.

2. Check `ext/a-libwebp/version.txt` — it records the project homepage, source repo URL, requested
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

- Decoder only: `src/dec`, `src/utils` and the `src/dsp` files those need. No encoder, no mux/demux, no sharpyuv. The dsp list is `libwebpDspSources` in `cmd/amalgam.ts`; a new upstream dsp file the decoder needs must be added there.
