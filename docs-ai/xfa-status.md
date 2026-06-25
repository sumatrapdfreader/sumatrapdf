# XFA form filling — status and handoff notes

Branch: `xfa` (as of June 2026). Goal: interactive editing of **hybrid** and **pure**
XFA forms in SumatraPDF — click fields on the canvas, edit in place, persist via
save. AcroForm filling is already on master (`docs-ai/pdf-form-filling-plan.md`);
this doc tracks the parallel **XFA overlay** path.

---

## What is done

### MuPDF / engine (`mupdf/source/pdf/xfa/`, `src/EngineMupdf.cpp`)

| Area | Status |
|------|--------|
| Hybrid XFA render (PDF page + XFA fields overlay) | Working |
| Pure XFA page render (`useXfaPages`) | Working |
| Field probe layout (`PDF_XFA_RENDER_PROBE_FIELDS`) | Working (headless + hybrid paint) |
| Field kinds: text, checkbox, radio (exclusive groups), **choice** (`dropDownList`) | Working |
| `pdf_xfa_get_field_kind` / choice count + options APIs | Working |
| Serialize / reload datasets (`pdf_xfa_serialize_data`, save round-trip) | Working |
| Safer value reads (`pdf_xfa_value_node_text` — avoid UAF on `<value><text>`) | Done |
| Choice detection via `<items>` **before** probing `checkButton`/`radioButton` UI | Critical fix (probing `dropDownList` ui in `get_field_kind` crashed) |
| Do **not** use `fz_strdup` in `pdf_xfa_object_text` for choice paths | Caused 100% crash on choice PDF |

`XfaFieldKind` (`src/XfaTypes.h`): `Unknown=0`, `Text=1`, `Checkbox=2`, `Radio=3`, `Choice=4`.

### Sumatra UI (`src/XfaFormFields.cpp`, `src/Canvas.cpp`)

| Interaction | Hybrid XFA | Pure XFA |
|-------------|------------|----------|
| Checkbox toggle | Yes | Yes (since `EngineIsXfaForm`) |
| Radio / exclusive group | Yes | Yes |
| Text field (`Edit` overlay) | Yes | Yes |
| Choice / `dropDownList` (`LISTBOX` below field) | Yes | Yes |
| Tab / Shift+Tab between tab-stop fields | Yes | Yes |
| Hover cursors (I-beam / hand) | Yes | Yes |
| Save persists XFA datasets | Yes (`EngineMarkXfaPageModified` + existing save path) | Yes |

**Choice editor behavior** (`StartXfaChoiceEdit`):

- Win32 `LISTBOX` child on `hwndCanvas`, positioned under the field (or above if no room).
- Populated from `EngineGetXfaFieldChoiceOptionTemp`.
- Commit: `LB_GETCURSEL` in `CommitXfaFieldEdit`; **list-item click** (`WM_LBUTTONUP`) is reliable; Enter alone is not always enough for automation.
- `WndProcXfaFormCtrl`: Escape discards; Enter / item click / Tab commits.

**Pure vs hybrid gating** (commit `213cbd3fc`):

- Old gate: `EngineIsHybridXfa` only → pure XFA forms were visible but not clickable.
- New gate: `EngineIsXfaForm` (`useXfaPages`) in `Canvas.cpp`.
- Field cache: hybrid builds probes during `BuildHybridXfaPageDisplayList`; pure XFA builds **lazily on first click** via `EnsureXfaFieldProbesForPage` + `CaptureXfaFieldProbes` in `EnsureXfaFieldCache`.
- Do **not** run `PDF_XFA_RENDER_PROBE_FIELDS` on every pure-XFA display-list build — caused intermittent startup crashes; probe-on-first-click is the stable approach.

### Key commits on `xfa` (recent)

| Commit | Summary |
|--------|---------|
| `8e1f1708c` | Persist XFA form edits to PDF on save |
| `ff9b9e911` | Exclusive widget groups (f1040 `c1_01` filing status) |
| `25882882c` | `c1_01` save round-trip test |
| `080dcae2d` | Tab navigation; `f1_01` text save test |
| `0319c90af` | Choice / `dropDownList` support (engine + UI + dbg-control tests) |
| `213cbd3fc` | Pure-XFA GUI editing; `TestXfaGuiFieldInteract`; `ad-hoc-xfa-choice-gui.ts` |

---

## Tests

### Fixtures (`tests/ad-hoc-xfa-data/`)

| File | Purpose |
|------|---------|
| `ad-hoc-xfa.pdf` | Minimal pure XFA (2 pages); regen: `python tests/ad-hoc-xfa.gen.py` |
| `ad-hoc-xfa-choice.pdf` | Pure XFA + single `dropDownList` (`filingStatus`); regen: `python tests/ad-hoc-xfa-choice.gen.py` |
| `ad-hoc-f1040.pdf` | Hybrid IRS Form 1040 (visual + layout + radio/save tests) |

Default choice field probe rect (PDF pts, y-up): `filingStatus=72,684,288,702`.
Default-fit screen center for GUI click: **(276, 937)** — use `TestXfaGuiFieldInteract` if zoom/layout changes.

### Test scripts

| Script | Registered in `before-release.ts` | Notes |
|--------|-----------------------------------|-------|
| `tests/ad-hoc-xfa.ts` | Yes | `TestXfa` on pure + plain PDF; **flaky** (see below) |
| `tests/ad-hoc-xfa-corpus.ts` | Yes | External corpus; slow |
| `tests/ad-hoc-xfa-f1040-visual.ts` | Yes | Metrics-based PNG regression |
| `tests/ad-hoc-xfa-f1040-serialize.ts` | Yes | |
| `tests/ad-hoc-xfa-f1040-save.ts` | Yes | Text + radio save |
| `tests/ad-hoc-xfa-f1040-radio.ts` | Yes | `c1_01` exclusive group |
| `tests/ad-hoc-xfa-choice.ts` | Yes | Choice serialize + save round-trip; **per-step retries** (15×) |
| `tests/ad-hoc-xfa-layout.ts` | Yes | f1040 dependents column layout |
| `tests/ad-hoc-xfa-choice-gui.ts` | **No** (ad-hoc GUI) | Win32 automation; passes with fallback coords |

Run:

```bash
bun tests/ad-hoc-xfa-choice.ts          # engine round-trip (reliable with retries)
bun tests/ad-hoc-xfa-choice-gui.ts      # GUI open/select/commit
bun tests/before-release.ts             # full pre-release incl. XFA ad-hoc set
```

GUI automation helpers: `tests/win-automation.ts`, `tests/winapi.ts` (message-posting, not `SendInput`).

### `-dbg-control` commands (`cmd/control.ts`)

| Cmd | ID | Purpose |
|-----|----|---------|
| `TestXfa` | 29 | Headless XFA detection + render metrics |
| `TestXfaFieldRects` | 31 | Field bounding boxes (PDF pts) |
| `TestXfaSerializeData` | 32 | Datasets XML |
| `TestXfaSetFieldSerializeData` | 33 | Set field + serialize |
| `TestXfaSaveFieldRoundTrip` | 34 | Set, save, reload |
| `TestXfaSelectRadioSerializeData` | 35 | Radio by bounds |
| `TestXfaSelectRadioSaveRoundTrip` | 36 | Radio save round-trip |
| `TestXfaFieldKind` | 37 | `kind=` + `choice_count=` |
| `TestXfaGuiFieldInteract` | 38 | **GUI window** hit-test at PDF coords; reports `screen=x,y`, `choice_count`, `screen_rc` |

`TestXfaGuiFieldInteract` float args must be **strings** on the wire (`"180"`, `"693"`, `"1"`).
`startEdit=1` from dbg-control does **not** create the ListBox (`CreateWindow` off UI thread); use real canvas clicks for GUI tests.

Choice test pattern (reliable):

```ts
// Separate runControlCommand per step, 15 retries each — one long dbg-control session is flaky.
await runWithRetry("TestXfaFieldKind", () => runControlCommand(EXE, ControlCommand.TestXfaFieldKind, [PDF, FIELD]));
```

---

## Known issues

### Dbg-control / `TestXfa` flakiness

- `runControlCommand` spawns a fresh `SumatraPDF-dll.exe -dbg-control` per call.
- `TestXfa` on `tests/ad-hoc-xfa.pdf` intermittently crashes the process (`control pipe closed while reading`); ~30–40% failure rate observed; can hang `before-release` for hours if the runner blocks.
- `ad-hoc-xfa-choice.ts` mitigates with **per-step retries**; `ad-hoc-xfa.ts` does not yet.
- **Do not** run a long-lived single dbg-control session for choice PDF — prefer one command per spawn.

### GUI / capture

- `captureWindowToPng` (PrintWindow) on the canvas is often **solid black** for XFA pages; do not use it as the sole pass/fail signal.
- Injected `SendInput` is dropped on the test machine; use `SendMessage`/`PostMessage` via `tests/win-automation.ts`.
- Choice commit in automation: `LB_SETCURSEL` then **click the list item** on the `LISTBOX` hwnd, not only Enter.

### Temp / junk (do not commit)

- `tests/__pycache__/`
- `tests/ad-hoc-xfa-data/ad-hoc-xfa-choice-text.pdf` (scratch; not the canonical fixture)

---

## Deferred / future work

### High value

1. **Retries for `ad-hoc-xfa.ts`** (and optionally `queryXfa` helper in `tests/xfa-test-util.ts`) — same pattern as `ad-hoc-xfa-choice.ts`.
2. **Investigate `TestXfa` crash root cause** — why does loading `ad-hoc-xfa.pdf` in dbg-control die intermittently after choice/XFA probe work?
3. **Register or skip `ad-hoc-xfa-choice-gui.ts` in `before-release.ts`** — only if we want GUI verification every release (slower, needs display).

### UX / feature gaps

4. **Cross-page Tab** — `AdvanceXfaFieldTabStop` is same-page only today.
5. **Space on focused checkbox/radio** — keyboard activation while field has focus.
6. **`choiceList` UI** (always-open list, not `dropDownList`) — not implemented; may need different overlay than floating `LISTBOX`.
7. **Hybrid f1040 choice fields** — f1040 probe list is mostly `f1_*` text + `c1_*` radios; no `dropDownList` in current fixture. Add a hybrid choice test PDF if real forms need it.
8. **Coordinate helper** — expose `CvtToScreen` for a field name via dbg-control (avoid hardcoded `276,937` in GUI test).

### MuPDF / render

9. **Probe during paint** — hybrid uses `FIELDS_ONLY | NO_BACKGROUND | PROBE_FIELDS`; pure XFA must keep probes off the main display list (see above).
10. **`pdf_xfa_get_field_kind`** — never walk `dropDownList` → `checkButton` ui; always prefer `<items>` for choice detection.

### Out of scope (explicitly ignored in recent work)

- WSL / `latex.ts` / `ad-hoc-synctex-wsl` failures in `before-release.ts`
- Full XFA spec coverage (subforms, scripts, barcodes, signatures)
- Replacing XFA with AcroForm

---

## Key source files (quick map)

| File | Role |
|------|------|
| `mupdf/source/pdf/xfa/xfa-render.c` | Layout, paint, field probes |
| `mupdf/source/pdf/xfa/xfa-data.c` | Datasets, field values, choice items |
| `mupdf/source/pdf/pdf-xfa.c` | Public XFA API, render flags |
| `src/EngineMupdf.cpp` | `hybridXfa` / `useXfaPages`, field cache, `EngineIsXfaForm`, choice APIs |
| `src/XfaFormFields.cpp` | `StartXfaChoiceEdit`, `CommitXfaFieldEdit`, Tab stops |
| `src/Canvas.cpp` | Click → `GetXfaFieldAtPos` → `StartXfaFieldInteraction` |
| `src/DisplayModel.cpp` | `GetXfaFieldAtPos`, `CvtToScreen` / `CvtFromScreen` |
| `src/SumatraTest.cpp` | Headless + `TestXfaGuiFieldInteract` |
| `tests/xfa-test-util.ts` | `queryXfa`, `parseXfaLine` |

Engine flags:

- `hybridXfa = !pdf_document_is_pure_xfa(...)` — static PDF page exists under XFA.
- `useXfaPages` — page count and paint from `pdf_xfa`.
- `EngineIsHybridXfa()` — hybrid only (render path).
- `EngineIsXfaForm()` — any XFA form (GUI interaction gate).

---

## Picking up work — suggested order

1. Build: `bun ./cmd/build.ts` → test with `./out/dbg64/SumatraPDF-dll.exe -for-testing`.
2. Smoke: `bun tests/ad-hoc-xfa-choice.ts` then `bun tests/ad-hoc-xfa-choice-gui.ts`.
3. Hybrid regression: `bun tests/ad-hoc-xfa-f1040-radio.ts`, `ad-hoc-xfa-f1040-save.ts`, `ad-hoc-xfa-f1040-visual.ts`.
4. If touching probes/kinds: regen `ad-hoc-xfa-choice.pdf` and re-run choice tests.
5. Before merge: `bun tests/before-release.ts` (expect possible `ad-hoc-xfa.ts` / corpus / latex flakes — see Known issues).

Manual check:

```
out/dbg64/SumatraPDF-dll.exe -for-testing tests/ad-hoc-xfa-data/ad-hoc-xfa-choice.pdf
```

Click **Filing status** → pick **Married filing jointly** → save → reopen.

---

## Related docs

- `docs-ai/pdf-form-filling-plan.md` — AcroForm path (master); defers XFA-only forms in v1 scope note (now partially addressed on `xfa` branch).
- `Agents.md` — build, test, dbg-control protocol, bug repro paths (`C:\Users\kjk\OneDrive\!sumatra\bugs\`).