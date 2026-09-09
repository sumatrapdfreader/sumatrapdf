# Updating libarchive

Use `cmd/amalgam.ts -libarchive` to update the amalgamated libarchive copy used
by the build.

1. Pick the upstream repository URL and tag or commit hash. The current source
   is:

   ```sh
   bun cmd/amalgam.ts -libarchive https://github.com/libarchive/libarchive v3.8.8
   ```

   Running `bun cmd/amalgam.ts -libarchive` without further arguments uses those
   defaults.

2. The script checks out the requested revision under `deps/libarchive` and
   writes `ext/a-libarchive/libarchive.c`, `archive.h`, `archive_entry.h`,
   `ext/a-libarchive/version.txt` and `COPYING`.
3. Review `ext/a-libarchive/version.txt`; it records the project homepage,
   source repo URL, requested revision, resolved commit SHA-1, and GitHub
   commit URL.
4. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

5. Build:

   ```sh
   bun cmd/build.ts -debug
   ```

Only the read side is built: no write formats, no write filters, no disk-write
or POSIX backends. That list is `libarchiveSources` in `cmd/amalgam.ts`.

`ext/a-libarchive/config_windows.h` is **hand-written, not generated** — the
script never overwrites it. `PLATFORM_CONFIG_H` selects it. An upstream update
that adds a new `HAVE_*` probe may need a matching line there.

libarchive has many same-named file-local statics (each read filter has its own
`struct private_data`, `archive_ppmd8.c` is a near-copy of `archive_ppmd7.c`).
`libarchiveRenames` gives one side of each pair a per-file prefix and
`libarchiveSeals` `#undef`s macros that would otherwise leak into a later file.
A new collision after an update shows up as a redefinition error from the
script's own `cl.exe` validation.
