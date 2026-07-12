# Upgrading djvudec

How to update SumatraPDF's vendored plain-C DjVu decoder (`ext/djvudec/djvu.c` +
`ext/djvudec/djvu.h`) from the upstream dist.

## Upstream source

- **Repo:** https://github.com/kjk/djvudec (`main` branch)
- **Files to copy:** `dist/djvu.c` and `dist/djvu.h` only
- **Do NOT use** https://codeberg.org/ccxvii/djvudec — that is an unrelated,
  older 2011 decoder with a different API (`djvudec.h`).

The amalgamated files are generated upstream by `bun cmd/build-dist.ts`. Never
edit `ext/djvudec/djvu.c` or `ext/djvudec/djvu.h` by hand in SumatraPDF.

## Steps

### 1. Fetch latest upstream dist

Clone or refresh a shallow copy, then record the commit hash:

```bash
git clone --depth 1 https://github.com/kjk/djvudec.git /tmp/djvudec-update
git -C /tmp/djvudec-update rev-parse HEAD
```

If `/tmp/djvudec-update` already exists:

```bash
git -C /tmp/djvudec-update pull --ff-only
```

### 2. Copy dist files into ext/djvudec

```bash
cp /tmp/djvudec-update/dist/djvu.c ext/djvudec/djvu.c
cp /tmp/djvudec-update/dist/djvu.h ext/djvudec/djvu.h
```

On Windows (PowerShell), use `Copy-Item ... -Force` — run the two copies as
separate commands (avoid `;` chaining in the shell tool).

**Do not clang-format** files under `ext/djvudec/` (vendored code).

### 3. Update ext/versions.txt

Record the import in `ext/versions.txt` (place the entry after `libdjvu`):

```
djvudec         main       YYYY-MM-DD
  https://github.com/kjk/djvudec
  commit <full-upstream-hash>
  amalgamated dist/djvu.c + dist/djvu.h only
```

Use today's date for `YYYY-MM-DD` and the hash from step 1.

### 4. Check for API changes

Diff the new `ext/djvudec/djvu.h` against the previous version. The public API
is also documented in upstream `src/djvu.h`.

Pay special attention to:

- **New functions** — may need export entries and `EngineDjvuDec.cpp` updates
- **Removed/renamed functions** — update `src/EngineDjvuDec.cpp` and exports
- **`djvu_init()`** — must be called once before concurrent decode (bilinear
  scaler table). `djvu_doc_open` also calls it, but that is not enough when
  render threads race on first open. SumatraPDF guards this in
  `EngineDjvuDec.cpp` via `DjvuDecInitOnce()` (SRWLOCK, same pattern as
  `EngineDjVu.cpp`).

Current integration points:

| File | Role |
|------|------|
| `src/EngineDjvuDec.cpp` | Engine using the djvu_* API |
| `src/EngineCreate.cpp` | Creates the djvudec engine for DjVu files |
| `cmd/scripts/gen_libmupdf.def.py` | Source of truth for djvudec DLL exports |
| `src/libmupdf.def` | Generated/auto-synced export list for `libmupdf.dll` |
| `premake5.lua` | `djvudec` static lib project |
| `ext/versions.txt` | Vendored dependency version log |

### 5. Update DLL exports (if API changed)

djvudec is statically linked into `libmupdf.dll`; `SumatraPDF.exe` imports
`djvu_*` symbols from that DLL via `src/libmupdf.def`.

When new public functions are added, append them to the `; djvudec exports`
section in **both**:

- `cmd/scripts/gen_libmupdf.def.py`
- `src/libmupdf.def`

Try regenerating the def file:

```bash
cd cmd/scripts && python gen_libmupdf.def.py
```

If that fails (`ModuleNotFoundError: util`), edit `src/libmupdf.def` manually
to match `gen_libmupdf.def.py`.

Current djvudec exports:

```
djvu_init
djvu_ctx_new
djvu_ctx_free
djvu_ctx_set_lazy_iw44
djvu_ctx_set_no_compose
djvu_ctx_set_iw_max_chunks
djvu_ctx_set_bgr
djvu_doc_open
djvu_doc_close
djvu_doc_page_count
djvu_doc_page_info
djvu_page_get_type
djvu_page_render
djvu_page_render_info
djvu_page_render_into
djvu_image_destroy
djvu_doc_page_id
djvu_doc_page_title
djvu_doc_page_by_name
djvu_page_text_get_zones
djvu_text_zones_destroy
djvu_doc_outline
djvu_outline_destroy
djvu_page_get_links
djvu_page_links_destroy
```

### 6. Update EngineDjvuDec (if needed)

Review `src/EngineDjvuDec.cpp` for any API mismatches. Common cases:

- New init function → ensure a once-guarded call before concurrent use
- New metadata/render helpers → wire up if SumatraPDF should expose them
- Changed struct fields → fix accessors

After editing `src/EngineDjvuDec.cpp`, run:

```bash
clang-format -i src/EngineDjvuDec.cpp
```

### 7. Build and smoke-test

```bash
bun ./cmd/build.ts
```

Test with:

```bash
./out/dbg64/SumatraPDF.exe -for-testing <path-to>.djvu
```

Use a `.djvu` from `C:\Users\kjk\OneDrive\!sumatra\bugs\` if available.

### 8. Commit message

Include the upstream commit hash:

```
Update djvudec to kjk/djvudec <short-hash>

Imported dist/djvu.c and dist/djvu.h from https://github.com/kjk/djvudec @ <full-hash>.
```

Append `prompt: ...` if the change was AI-assisted (per Agents.md).

## Architecture notes

- `ext/djvudec` is a **single-file amalgamation** (like sqlite): compile
  `djvu.c` directly; include `djvu.h`.
- `EngineDjvuDec` keeps a single shared `djvu_doc` (read-only after open);
  `djvu_page_render_into` / metadata calls are re-entrant on the same doc.
- Thread-safety analysis lives in `ai/djvudec-threading.md` and upstream
  `thread-safety.md`.
- djvudec is the **only** DjVu engine (the libdjvu-based `EngineDjVu` has been
  removed; `EngineCreate.cpp` creates `EngineDjvuDec` for all DjVu files).

## Checklist

- [ ] Copied `dist/djvu.c` and `dist/djvu.h` from kjk/djvudec
- [ ] Updated `ext/versions.txt` with import date and upstream commit hash
- [ ] Checked `djvu.h` for API changes
- [ ] Updated `gen_libmupdf.def.py` + `src/libmupdf.def` if exports changed
- [ ] Updated `EngineDjvuDec.cpp` if integration changed
- [ ] Did **not** clang-format `ext/djvudec/`
- [ ] `bun ./cmd/build.ts` succeeds
- [ ] Smoke-tested a `.djvu` file
