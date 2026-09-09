---
name: update-libarchive
description: Regenerate the amalgamated libarchive copy in ext/a-libarchive from upstream with cmd/amalgam.ts. Use when asked to update, upgrade or re-amalgamate libarchive, or when the user runs /update-libarchive.
---

# Update amalgamated libarchive

`ext/a-libarchive/` is generated — never hand-edit it. `bun cmd/amalgam.ts -libarchive` clones upstream into
`.work/deps/libarchive`, amalgamates it, and validates the result by compiling it with `cl.exe`.

## Steps

1. Regenerate. With no further arguments it re-runs the repo and revision recorded in
   `ext/versions.txt`:

   ```sh
   bun cmd/amalgam.ts -libarchive
   ```

   To move to a different upstream revision, pass it explicitly:

   ```sh
   bun cmd/amalgam.ts -libarchive <repo-url> <tag-or-commit>
   ```

   It writes `libarchive.c`, `archive.h`, `archive_entry.h`, `COPYING` and `version.txt` into `ext/a-libarchive/`.
   Add `-keep` to reuse the existing checkout while iterating on the amalgamation rules.

2. Check `ext/a-libarchive/version.txt` — it records the project homepage, source repo URL, requested
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

- Only the read side is built: no write formats, no write filters, no disk-write or POSIX backends. That list is `libarchiveSources` in `cmd/amalgam.ts`.
- `ext/a-libarchive/config_windows.h` is **hand-written, not generated** — the script never overwrites it. `PLATFORM_CONFIG_H` selects it. An upstream update that adds a new `HAVE_*` probe may need a matching line there.
- libarchive has many same-named file-local statics (each read filter has its own `struct private_data`, `archive_ppmd8.c` is a near-copy of `archive_ppmd7.c`). `libarchiveRenames` gives one side of each pair a per-file prefix and `libarchiveSeals` `#undef`s macros that would otherwise leak into a later file. A new collision after an update shows up as a redefinition error from the script's own `cl.exe` validation.
