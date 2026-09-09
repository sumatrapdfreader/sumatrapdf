# Updating Brotli

Use `cmd/amalgam.ts -brotli` to update the amalgamated Brotli copy used by the
build.

1. Pick the upstream repository URL and tag or commit hash. The current source
   is:

   ```sh
   bun cmd/amalgam.ts -brotli https://github.com/ArtifexSoftware/thirdparty-brotli 2523f3314501aa5c90e561922b2e668b3ccee26e
   ```

   Running `bun cmd/amalgam.ts -brotli` without further arguments uses those
   defaults. Keep the revision in sync with the brotli submodule of the
   vendored mupdf (see `ext/versions.txt`).

2. The script checks out the requested revision under `deps/brotli` and writes
   `ext/a-brotli/brotli.c`, the public headers under `ext/a-brotli/brotli/`,
   `ext/a-brotli/version.txt` and `ext/a-brotli/LICENSE`. The headers keep
   their directory so `#include <brotli/decode.h>` works with `ext/a-brotli` on
   the include path.
3. Review `ext/a-brotli/version.txt`; it records the project homepage, source
   repo URL, requested revision, resolved commit SHA-1, and GitHub commit URL.
4. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

5. Build:

   ```sh
   bun cmd/build.ts -debug
   ```

The `*_inc.h` files are X-macro fragments with no include guard; the inliner
expands them at every use, which is what they are for.

`compress_fragment_two_pass.c` is a near-copy of `compress_fragment.c`, so most
of `brotliRenames` in `cmd/amalgam.ts` is one half of that pair. A new
collision after an update shows up as a redefinition error from the script's
own `cl.exe` validation.
