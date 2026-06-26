# djvudec thread-safety analysis

How thread-safe is the `ext/djvudec` decoder (`djvu.c` / `djvu.h`), and what it
would take to make concurrent rendering fully safe.

Context: `EngineDjvuDec` renders DjVu pages on SumatraPDF's background render
threads (up to `maxRenderThreads`). djvudec is decode-only and was written as a
single-threaded library, so we currently render through a **pool of independent
`djvu_doc` handles** (one acquired per `RenderPage`, each its own `djvu_ctx` +
`djvu_doc` over the same read-only in-memory file). See `src/EngineDjvuDec.cpp`.

## What's shared / mutable in djvudec

Audited the whole decoder for shared mutable state:

- **`djvu_ctx`** — only the alloc/free/error callbacks + a `user` pointer.
  Immutable after `djvu_ctx_new`.
- **`djvu_doc`** — the `const uint8_t* data` file pointer plus the parsed DIRM
  tables (`comps`, `pages`). These are populated during `djvu_doc_open` and are
  **read-only during rendering**. There are **no decoded-bitmap caches** on the
  doc or page structs: `djvu_page_render` decodes into freshly `djvu_alloc`'d
  local buffers and returns a new `djvu_image`. Shared JB2 dictionaries (DJVI
  INCL components) are re-decoded per render — there is no doc-level cache.
- **Default allocator** — `djvu_alloc`/`djvu_free` just call `malloc`/`free`
  (thread-safe). A *custom* allocator/error callback passed to `djvu_ctx_new`
  would be the caller's responsibility to make thread-safe; we pass `NULL`.
- **`djvu_errorf`** uses a stack-local `char buf[512]` — fine.
- **No function-scope `static` scratch buffers** anywhere in the file.

Consequence: a single `djvu_doc` is **already essentially re-entrant** for
concurrent page renders — with exactly one exception.

## The one real hazard: `s_interp`

```c
static short s_interp[FRACSIZE][512];   /* FRACSIZE = 16  ->  16 KB */
static int  s_interp_ready = 0;
static void prepare_interp(void) {
    if (s_interp_ready) return;
    /* build s_interp ... */
    s_interp_ready = 1;
}
```

A **process-global, lazily-built lookup table** used by the IW44 scaler
(`prepare_interp()` is called from the GPixmapScaler). It is exercised only on
**photo / compound** pages (BG44 backgrounds that get scaled), **not** on
bitonal (JB2-only) scanned-text pages.

Two render threads can race here:
- build-while-read (one thread reads `s_interp` while another is filling it), or
- the `s_interp_ready = 1` store being reordered ahead of the table stores, so a
  second thread sees `ready == 1` and reads a partially-built table.

This is a genuine data race / UB.

### Important: a separate context does NOT fix this

`s_interp` is a file-scope global shared by **all** contexts and docs, so giving
each render thread its own `djvu_ctx` / `djvu_doc` (what the engine does today)
does **not** remove the race. The current per-doc pool is therefore safe for
**bitonal** DjVu but has a **latent race on photo/compound pages** rendered
concurrently. (Bitonal scans are why the smoke tests passed.)

The table is small (16 KB) and a pure, deterministic function of its indices,
used read-only after it's built.

## What it would take to make it fully safe

The decoder is already thread-safe except for that single lazy init, so the only
work is making `prepare_interp()` safe. Options, cheapest first:

- **(A) One-time global init (recommended).** Add `void djvu_global_init(void)`
  to `djvu.h`/`djvu.c` that eagerly calls `prepare_interp()`, and call it once on
  our side via `std::once_flag` before any render thread runs. ~3 lines in the
  vendored file + one call site; keeps `djvu.c` free of platform threading
  primitives. (Note: folding the init into `djvu_ctx_new` is *not* enough on its
  own, because we create pool contexts from multiple threads — it still needs a
  real once-guard.)
- **(B) Self-contained atomic flag.** Make `s_interp_ready` a C11 `atomic_int`
  with release on store / acquire on load; a benign double-build is fine since
  all threads write identical values. No caller cooperation, but pulls
  `<stdatomic.h>` into `djvu.c`.
- **(C) Precompute as `static const`.** Code-gen the 16 KB table so there is no
  runtime init at all (inherently safe, read-only). Cleanest semantically; adds
  ~8192 generated constants to the file.

After the table is safe, the doc is fully re-entrant, so on the engine side we
can either:
- keep the **doc pool** as-is (works, slightly more memory), or
- **simplify**: share one `djvu_doc` across all render threads with no render
  lock — same parallelism, less code/memory. (Shared dictionaries are re-decoded
  per page either way.)

## Recommendation

Do **(A)** — minimal, portable, and it closes a real (if narrow) correctness bug
in the current pool for photo DjVu. Optionally follow up by collapsing the pool
to a single shared doc.
