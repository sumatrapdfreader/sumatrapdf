# Offline dictionary / word lookup — plan for future work

Status: research done (2026-07-11), no code yet. This is fork feature **B** from
[plus-merge.md](plus-merge.md) — port decision pending; the recommendation below
is to port the UI/plumbing but replace the data layer with a StarDict reader.

## How the sumatrapdf-plus fork implements it

Fork source (extract with `git show plus/main:<path>`):
- `src/WordLookup.{cpp,h}` (~2,450 lines) — popup UI + lookup + dictionary loading
- `src/LookupAudio.{cpp,h}` — pronunciation playback (Media Foundation, decodes mp3 blobs)
- `src/FloatingPopupStyle.{cpp,h}` — shared popup chrome (we folded an equivalent
  into `SelectionToolbar.cpp` when porting feature H; factor it back out if porting this)
- `cmd/import-oaldpex-dict.ts`, `cmd/import-xdhy7-dict.ts` — offline converters
- Settings: `OfflineDictionaryPath` (dir; default `{exedir}/dict`),
  `EnableDoubleClickWordLookup` (bool, default true)
- Commands: `CmdLookupSelection`, `CmdToggleDoubleClickWordLookup`,
  `CmdSearchSelectionWithYoudaoDict` (online fallback, opens browser)

### Invocation (three paths)

1. **Double-click a word** on the canvas (`EnableDoubleClickWordLookup`). Routing
   in their `Canvas.cpp` double-click handler, gated on `isOverText`:
   - fixed-layout ebooks → `ShowEbookWordLookupAt`
   - CJK text → `ShowChineseWordLookupAt`: greedy longest-prefix match against the
     Chinese dictionary (8-hanzi window, 3-char look-back) since Chinese has no
     word boundaries
   - otherwise the double-clicked word → `ShowWordLookup`
2. **`CmdLookupSelection`** from palette/menu; also the "Look Up" button on their
   selection toolbar (we dropped that button in our feature-H port; re-add via
   `CommandAvailability` when this lands).
3. **Youdao web fallback** command (skip in our port, or generalize to the existing
   SearchSelectionWith* family).

### Popup UI (custom-drawn, NOT HTML)

Floating `WS_POPUP` card ~340px wide, rounded corners, theme-aware; shows:
headword + IPA transcription, part-of-speech **tabs** (max 8 senses), numbered
bilingual definitions (en/zh line pairs), one example sentence per sense, speaker
icon (hover/wave animation, plays pronunciation), loading-dots spinner (lookup on
a background thread), close button. Positions near the clicked word clamped to
the canvas; pauses read-aloud while open, resumes on close.

Caveat for porting: this layout is OALD-shaped (senses/tabs/bilingual pairs).
A generic dictionary reader needs a plain "formatted text entry" rendering mode
first; their rich layout becomes a special case.

### Lookup pipeline

normalize (trim + lowercase) → candidate list via hard-coded English morphology
(irregular plurals table, irregular verbs table, -s/-es/-ies/-ed/-ing stripping)
→ binary search over a fully-in-RAM sorted index → read entry slice from the data
file by offset/size → parse entry.

### Their storage format ("SumatraDict" — bespoke, not a standard)

| File | Content |
|---|---|
| `SumatraDict.idx` | TSV text, sorted: `word \t dataOffset \t dataSize \t audioOffset \t audioSize \t audioExt` |
| `SumatraDict.dat` | concatenated plain-text entries: `SDICT1\n` magic, then line-based fields (headword, IPA, sense count; per sense: label, POS, def count, en/zh definition pairs, example) |
| `SumatraDictAudio.dat` | concatenated mp3 blobs, sliced by offset/size (optional) |
| `SumatraDictZh.{idx,dat}` | separate Chinese dictionary (hanzi headwords, pinyin instead of IPA) |

### What dictionaries they actually support

Exactly **two**, via offline bun+Python converters (`readmdict`/`mdict-utils` +
BeautifulSoup) that scrape specific **MDict (.mdx/.mdd)** packages, with the
dictionaries' HTML class structure hardcoded:
- **OALDPE(X)** — community MDX repack of Oxford Advanced Learner's Dictionary
  10th ed with Chinese translations (copyrighted; circulates on Chinese
  dictionary forums; users must source it themselves)
- **现代汉语词典 7** (Contemporary Chinese Dictionary, monolingual)

Consequences: nothing works out of the box, converting requires Python, and the
only documented sources are pirated commercial content. Very China-centric.

## Dictionary format landscape

- **StarDict** (`.ifo` + `.idx` + `.dict[.dz]`, optional `.syn`) — de-facto FOSS
  standard, **best first target**. Simple documented format: `.idx` is a sorted
  list of `word\0 u32be-offset u32be-size`; `.dict.dz` is dictzip (gzip with a
  random-access extra field); `.ifo` is key=value metadata (incl. `sametypesequence`,
  typically `m`/`t`/`x`/`h` = plain text / IPA / xdxf / html). `.syn` maps synonym
  strings to entry indexes. Readable natively with no converter. Big
  freely-licensed ecosystem: ECDICT, FreeDict, CC-CEDICT, JMdict, Wiktionary and
  WordNet exports. Used by GoldenDict, KOReader, etc.
- **MDict (.mdx/.mdd)** — dominant in China and for premium repacks (OALD, LDOCE).
  Reverse-engineered, zlib blocks, entries are full HTML (GoldenDict renders them
  with a browser engine), content mostly copyrighted. Don't implement natively;
  the fork's convert-offline approach is right for this one.
- **DSL** (ABBYY Lingvo source) — plain-text markup, popular in Russian-speaking
  communities. Feasible later.
- **DICT/dictd**, **Babylon BGL**, **EPWING** (Japanese), **Slob** (Aard2) —
  legacy/niche, skip.
- Freely-licensed data to point users at (or bundle): **ECDICT** (en↔zh, also
  distributed as StarDict), **WordNet** (English monolingual, permissive license),
  **Wiktionary extracts** (wiktextract JSON, all languages), **CC-CEDICT** (zh→en).

## Recommended plan when porting

1. **Port from the fork mostly as-is:** popup window shell, double-click/command
   invocation, CJK longest-prefix segmentation, English morphology fallbacks,
   async lookup thread, audio playback, settings + commands. Re-add the "Look Up"
   button to `SelectionToolbar.cpp` (gate via `CommandAvailability`, hidden when
   no dictionary is installed). Factor popup chrome back out of SelectionToolbar
   into a shared `FloatingPopupStyle` when both need it.
2. **Replace the data layer:** implement a native **StarDict reader**
   (`.ifo`/`.idx`/`.dict.dz`/`.syn`; dictzip random access is the only nontrivial
   part — gzip `FEXTRA` chunk table, inflate per-chunk). Keep the sorted-index +
   binary-search + slice-read architecture — it maps 1:1. Support multiple
   dictionaries in a directory, looked up in priority order.
3. **Rendering:** start with a plain-text entry renderer (StarDict `m`/`t` types;
   strip tags for `h`/`x`); keep the fork's rich sense/tab layout only if we also
   keep their SumatraDict format as an "enhanced" package format.
4. **Converters:** optionally keep/generalize the fork's MDX scripts but emit
   StarDict instead of a third format; document ECDICT/Wiktionary/WordNet as the
   out-of-the-box sources.
5. **Port adaptation notes** (same issues as features D/E/H): fork uses old Vec
   API (`.Size()`/`.at()` → `len()`/`[]`), `utils/` → `base/` includes, no
   `#pragma once`, `str::` API drift, `ThemeUsesDarkChrome()` → check effective
   colors, PCH opt-out only if a file includes mupdf headers first (these don't).
   `WordLookup.cpp` touches Canvas/Toolbar/Theme/read-aloud — expect conflicts;
   port by feature diff, not cherry-pick.

## Open questions for Krzysztof

- Bundle a small freely-licensed dictionary with the installer, or download on
  first use, or leave entirely user-supplied?
- Is en↔zh bilingual layout worth special-casing, or is generic single-language
  rendering enough for the first version?
- Keep the Youdao command (China-centric) or fold online lookup into the existing
  `SearchSelectionWith*` handlers (e.g. Wiktionary web)?
