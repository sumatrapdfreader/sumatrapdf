# ComicInfo.xml in SumatraPDF

ComicInfo.xml is a small XML metadata file commonly embedded in comic book archives (`.cbz`, `.cbr`, `.cb7`, `.cbt`). It describes the book (title, series, creators, page list, and optional per-page bookmarks). SumatraPDF reads it when opening comic archives.

Schema reference: [The Anansi Project — ComicInfo documentation](https://anansi-project.github.io/docs/comicinfo/documentation)

## Where it lives

The file must be named `ComicInfo.xml` and sit at the **root** of the archive (not in a subfolder). SumatraPDF looks it up by that exact name when loading a comic.

Supported containers (all use the same logic):

- `.cbz` (ZIP)
- `.cbr` (RAR)
- `.cb7` (7-Zip)
- `.cbt` (tar)

## What SumatraPDF uses it for

### Document properties

When `ComicInfo.xml` is present, SumatraPDF extracts:

| ComicInfo element | Shown as |
|-------------------|----------|
| `Title` | Title |
| `Writer`, `Penciller` (primary credits) | Author |
| `Year`, `Month` | Creation date |
| `Summary` | Subject |
| App id / last-modified from ComicBookInfo JSON (see below) | Creator app / modification date |

Many other ComicInfo fields (`Series`, `Publisher`, `PageCount`, `Characters`, …) are **not** mapped to the properties dialog today, though they may appear in the raw XML inside the archive.

### Table of contents (bookmarks)

Since issue [#1201](https://github.com/sumatrapdfreader/sumatrapdf/issues/1201), SumatraPDF can build the **Table of Contents** from ComicInfo page bookmarks.

Each page is described under `<Pages>`:

```xml
<Page Image="0" Bookmark="Cover" Type="FrontCover" />
<Page Image="2" Bookmark="Chapter 1" Type="Story" />
```

- **`Image`** — 0-based index into the sorted image page list (first image = 0).
- **`Bookmark`** — optional label. When present and non-empty, SumatraPDF adds a TOC entry with this text pointing at `Image + 1` (1-based page number in the viewer).
- **`Type`** — structural page kind (see below). **Not** used for the TOC today.

**TOC rules:**

1. If **any** `<Page>` has a non-empty `Bookmark` attribute → TOC lists **only** those bookmark entries, sorted by `Image`.
2. Otherwise → TOC is a flat list of **image filenames** (e.g. `P00001.jpg`, `001.png`), one entry per page — same as before ComicInfo bookmark support.
3. Bookmark entries outside the valid page range are skipped.

Open the TOC panel (F9) and click an entry to jump to that page.

## Page `Type` values (not used for TOC)

ComicInfo defines a `Type` on each page. These describe page role, not custom titles:

| Type | Meaning |
|------|---------|
| `FrontCover` | Cover |
| `InnerCover` | Secondary cover inside the book |
| `Roundup` | Summary of previous issues |
| `Story` | Story pages |
| `Advertisement` | Ads |
| `Editorial` | Editorial |
| `Letters` | Letter column |
| `Preview` | Sneak preview |
| `BackCover` | Back cover |
| `Other` | Catch-all |
| `Delete` | Page should not be shown (per spec; SumatraPDF does not hide these pages today) |

Scraped metadata (ComicVine, ComicRack, etc.) often fills `Type`, `ImageWidth`, `ImageHeight`, and `ImageSize` but **omits `Bookmark`**. Those files still get a filename-based TOC.

## Other metadata and bookmark sources

SumatraPDF distinguishes several related mechanisms:

| Source | In the file? | Used for TOC? | Notes |
|--------|--------------|---------------|-------|
| ComicInfo `Bookmark` | Yes | Yes, when present | User- or tool-added labels |
| Image filenames | Yes | Yes, fallback | Default navigation |
| ComicInfo `Type` | Yes | No | Could be used in future |
| **ComicBookInfo** (ZIP comment JSON) | Yes (older CBZ) | No | Title, authors, date → properties only |
| **Favorites** (F12 / Ctrl+B) | No (app settings) | Separate panel | Per-user, per-file; not in ComicInfo |

**ComicBookInfo** is a legacy JSON blob stored in the ZIP archive comment. SumatraPDF still parses it for basic properties when ComicInfo is absent or incomplete. It has no per-page bookmarks.

**Favorites** are SumatraPDF’s own bookmarks, stored in settings — they do not travel with the `.cbz` file.

## Example ComicInfo.xml

Minimal file with TOC bookmarks (see `tests/issue-1201.cbz` in the repo):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ComicInfo>
  <Title>My Comic</Title>
  <Pages>
    <Page Image="0" Type="FrontCover" Bookmark="Cover" />
    <Page Image="10" Type="Story" Bookmark="Act 2" />
  </Pages>
</ComicInfo>
```

Expected TOC: **Cover** (page 1), **Act 2** (page 11).

## Adding bookmarks to an existing CBZ

1. Extract `ComicInfo.xml` from the archive (or create one).
2. Add `<Pages>` / `<Page Image="N" Bookmark="…"/>` entries (`Image` is 0-based).
3. Put `ComicInfo.xml` back at the archive root.
4. Reopen in SumatraPDF — TOC should show bookmark titles instead of filenames.

Tools such as ComicRack can add `Bookmark` values when you bookmark a page inside that application.

## Implementation notes

- Parser: `ComicInfoParser` in `src/EngineImages.cpp` (`EngineCbx`).
- Encoding: UTF-8, with optional UTF-8/UTF-16 BOM detection.
- Regression test: `tests/issue-1201.ts` (fixture `tests/issue-1201.cbz`).
- Headless TOC check: `-dbg-control` command `TestGetToc` (id 29).

To find archives with `ComicInfo.xml` in a large library:

```bash
# list all comic archives, then scan for ComicInfo.xml in zip central directory
cmd /c "dir /s /b x:\path\to\comics\*.cbz x:\path\to\comics\*.cbr" > tests/tmp/all-cbz.txt
bun tests/ad-hoc-scan-comicinfo-from-list.ts
```

Output: `tests/tmp/issue-1201-comics.txt` (paths only; does not mean they contain `Bookmark` attributes).