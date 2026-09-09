# Updating HarfBuzz

Use `cmd/amalgam.ts -harfbuzz` to update the amalgamated HarfBuzz copy used by
the build.

1. Pick the upstream HarfBuzz repository URL and tag or commit hash. The current
   source is:

   ```sh
   bun cmd/amalgam.ts -harfbuzz https://github.com/ArtifexSoftware/thirdparty-harfbuzz c28ba35a1e6dc4b6b32e2b1d11fd2a408f6c86bb
   ```

   Running `bun cmd/amalgam.ts -harfbuzz` without further arguments uses those
   defaults. Keep the revision in sync with the harfbuzz submodule of the
   vendored mupdf (see `ext/versions.txt`).

2. The script checks out the requested revision under `deps/harfbuzz` and writes
   `ext/a-harfbuzz/harfbuzz.cc`, the public `hb*.h` headers, three `.hh`
   X-macro fragments the inliner can't expand, `ext/a-harfbuzz/version.txt` and
   `ext/a-harfbuzz/COPYING`. The output directory is wiped first, so headers
   dropped upstream don't linger.
3. Review `ext/a-harfbuzz/version.txt`; it records the project homepage, source
   repo URL, requested revision, resolved commit SHA-1, and GitHub commit URL.
4. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

5. Build:

   ```sh
   bun cmd/build.ts -debug
   ```

Only the sources mupdf needs are amalgamated: `hb_base_sources`,
`hb_subset_sources` and `hb-ft.cc` from upstream's `src/meson.build`. The
cairo, CoreText, DirectWrite, graphite2, ICU, raster, Uniscribe and wasm
backends are left out. That list lives in `cmd/amalgam.ts`.

The single translation unit needs `/bigobj`; both build systems pass it.
