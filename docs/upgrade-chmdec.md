# Upgrading chmdec

How to update SumatraPDF's vendored plain-C CHM reader (`ext/chmdec/chm.c` +
`ext/chmdec/chm.h`) from the upstream dist.

## Upstream source

- **Repo:** https://github.com/kjk/chmdec (`main` branch)
- **Files to copy:** `dist/chm.c` and `dist/chm.h` only
- Replaces the older `ext/libchm` vendor (same API family; was
  https://github.com/kjk/libchm).

The amalgamated files are generated upstream by `bun cmd/build-dist.ts`. Never
edit `ext/chmdec/chm.c` or `ext/chmdec/chm.h` by hand in SumatraPDF.

## Steps

### 1. Fetch latest upstream dist

Clone or refresh a shallow copy, then record the commit hash:

```bash
git clone --depth 1 https://github.com/kjk/chmdec.git /tmp/chmdec-update
git -C /tmp/chmdec-update rev-parse HEAD
```

If `/tmp/chmdec-update` already exists:

```bash
git -C /tmp/chmdec-update pull --ff-only
```

### 2. Copy dist files into ext/chmdec

```bash
cp /tmp/chmdec-update/dist/chm.c ext/chmdec/chm.c
cp /tmp/chmdec-update/dist/chm.h ext/chmdec/chm.h
```

On Windows (PowerShell), use `Copy-Item ... -Force` — run the two copies as
separate commands (avoid `;` chaining in the shell tool).

Optionally refresh the license if it changed:

```bash
cp /tmp/chmdec-update/LICENSE.md ext/chmdec/LICENSE.md
```

**Do not clang-format** files under `ext/chmdec/` (vendored code).

### 3. Update ext/versions.txt

Record the import in `ext/versions.txt`:

```
chmdec          dist       YYYY-MM-DD
  https://github.com/kjk/chmdec
  commit <full-upstream-hash>
  amalgamated dist/chm.c + dist/chm.h only
```

Use today's date for `YYYY-MM-DD` and the hash from step 1.

### 4. Check for API changes

Diff the new `ext/chmdec/chm.h` against the previous version.

Pay special attention to:

- **New functions** — may need export entries and `ChmFile.cpp` updates
- **Removed/renamed functions** — update callers and exports
- **`chm_open` buffer lifetime** — the in-memory buffer is not copied; it must
  outlive the `chm_ctx` (SumatraPDF keeps file bytes in `ChmFile` until
  `chm_ctx_free`)

Current integration points:

| File | Role |
|------|------|
| `src/ChmFile.cpp` / `src/ChmFile.h` | CHM document open / entry read |
| `src/ChmDump.cpp` | `-dump-chm` diagnostics |
| `src/SumatraTest.cpp` | `-test-chm` LZX / open regression |
| `src/libsumatrapdf.def` | Hand-maintained export list for `libsumatrapdf.dll` |
| `premake5.lua` | `chmdec` static lib project |
| `ext/versions.txt` | Vendored dependency version log |

### 5. Update DLL exports (if API changed)

chmdec is linked into `libsumatrapdf.dll` (and into the static EXE). The DLL build of
`SumatraPDF.exe`, `PdfFilter`, and `PdfPreview` must **not** also link the
`chmdec` static lib — they import `chm_*` from `libsumatrapdf.dll` via
`src/libsumatrapdf.def`.

When new public functions are added, append them to the `; chmdec exports`
section in `src/libsumatrapdf.def`.

Current chmdec exports:

```
chm_ctx_new
chm_ctx_free
chm_open
chm_close
chm_read_entry
chm_get_entries
LZX_test_pretree_make_decode_table
```

`LZX_test_pretree_make_decode_table` is a test-only hook used by `-test-chm`
(`src/SumatraTest.cpp`); it is defined in `chm.c` but not declared in `chm.h`.

### 6. Build and smoke-test

```bash
bun cmd/build.ts -debug
# optional CHM regression (needs a sample .chm):
# out/dbg64/SumatraPDF.exe -for-testing -test-chm path\to\file.chm
# out/dbg64/SumatraPDF.exe -for-testing -dump-chm path\to\file.chm
```

Also run `bun tests/issue-chm-lzx.ts` if LZX decode tables changed.

## Commit message template

```
Update chmdec to kjk/chmdec <short-hash>

Imported dist/chm.c and dist/chm.h from https://github.com/kjk/chmdec @ <full-hash>.
```

## Notes

- `ext/chmdec` is a **single-file amalgamation** (like sqlite / heicdec): compile
  `chm.c` only; include `chm.h` from callers.
- The static library project is named `chmdec` in premake / MSBuild; sources
  keep the historical `chm_*` C API names.
- Static `SumatraPDF-static` and `test_engines` still link `chmdec` directly
  (no `libsumatrapdf.dll`). The DLL-split products import via def exports only.
