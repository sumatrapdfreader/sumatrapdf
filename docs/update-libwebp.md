# Updating libwebp

Use `cmd/amalgam.ts -libwebp` to update the amalgamated libwebp copy used by
the build.

1. Pick the upstream repository URL and tag or commit hash. The current source
   is:

   ```sh
   bun cmd/amalgam.ts -libwebp https://github.com/webmproject/libwebp v1.6.0
   ```

   Running `bun cmd/amalgam.ts -libwebp` without further arguments uses those
   defaults.

2. The script checks out the requested revision under `.work/deps/libwebp` and writes
   `ext/a-libwebp/libwebp.c`, the public headers under `ext/a-libwebp/webp/`,
   `ext/a-libwebp/version.txt`, `COPYING`, `PATENTS` and `AUTHORS`. The headers
   keep their directory so `#include <webp/decode.h>` works with
   `ext/a-libwebp` on the include path.
3. Review `ext/a-libwebp/version.txt`; it records the project homepage, source
   repo URL, requested revision, resolved commit SHA-1, and GitHub commit URL.
4. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

5. Build:

   ```sh
   bun cmd/build.ts -dbg
   ```

Decoder only: `src/dec`, `src/utils` and the `src/dsp` files those need. No
encoder, no mux/demux, no sharpyuv. The dsp list is `libwebpDspSources` in
`cmd/amalgam.ts`; adding a dsp file upstream that the decoder needs means
adding it there.
