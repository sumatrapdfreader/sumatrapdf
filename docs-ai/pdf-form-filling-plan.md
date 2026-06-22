# PDF form filling — implementation plan

Goal: let the user fill interactive PDF (AcroForm) fields **directly on the page**
— click a text field and type, click a checkbox to toggle, click a dropdown to
pick — no side panel, no modal dialog. Save reuses the existing annotation-save
pipeline.

Scope (v1): text fields (single + multiline), checkboxes, radio buttons,
comboboxes, listboxes. **Defer:** digital signatures, XFA-only forms, push
buttons that run arbitrary JS, barcode fields.

---

## Implementation status — DONE (phases 0–4, all on master)

Built and validated against `f1040.pdf` (a hybrid-XFA form whose AcroForm layer
fills fine) plus a synthetic `formtest.pdf` for the field types the 1040 lacks.

| Phase | Commit | What shipped |
|-------|--------|--------------|
| 0 | `1949ffa8a` | Click a checkbox / radio to toggle it in place. Needed: export the mupdf form-mutation APIs from `libmupdf.def`; make widgets hit-testable (mupdf 1.27 keeps them in a separate `page->widgets` list, so walk `pdf_first_widget` in `GetFzPageInfo`). |
| 1 | `082faf66d` | Click a text field → floating Win32 `Edit` over the field (positioned via `CvtToScreen`), commit on Enter/blur via `pdf_set_text_field_value`. New `src/FormFields.cpp`. |
| 2 | `627848a9f` | Combobox/listbox dropdown (`pdf_choice_widget_*`); radio groups with proper mutual exclusion. Two mupdf gotchas: regenerate the whole page (`pdf_update_page`) so radio siblings / calc fields refresh; `pdf_toggle_widget` mishandles distinct-on-state radios, so radios go through `pdf_set_field_value` instead. |
| cleanup | `053e23d46` | Move widgets out of the shared annotation list into `FzPageInfo::widgets` + `GetWidgetAtPos`, so form fields stop polluting comment hovers / the edit-annotations panel. |
| 3 | `dbbf00f85` | Tab / Shift+Tab navigation (`EngineMupdfGetAdjacentWidget`); overlay font from the field's `/DA` size × zoom; max-length / comb enforcement (`EM_SETLIMITTEXT`); crash-safe teardown of the overlay on doc close + reload (no dangling widget pointer). |
| 4a | `222e45dab` | Save round-trip verified (reuses the annotation `pdf_save_document` pipeline — field values + checkbox state persist across save/reopen); generalized the close-time prompt wording from "Unsaved annotations" to "Unsaved changes"; hover cursors (I-beam over text/choice, hand over buttons). |
| 4b | `f7fd4f998` | Scroll the next field into view on Tab when it's past the fold (`ScrollScreenToRect`). |

### Known limitation — JavaScript-calculated fields NOT supported

Verified: our mupdf build *does* compile JS (`FZ_ENABLE_JS=1`, mujs `one.c`
built, `pdf-js.c` in the build), but SumatraPDF deliberately never calls
`pdf_enable_js` — `EngineMupdf.cpp:2830` has `// TODO: support javascript` and
asserts `pdf_js_supported()` is false. So `doc->js` is null and
`pdf_calculate_form` is a no-op: fields whose value is computed by form
JavaScript (e.g. auto-summed totals) will not recompute. The non-JS appearance
regeneration via `pdf_update_page` still runs (that's what keeps radio-group
siblings correct). Enabling JS calc would mean calling `pdf_enable_js` and
removing that assertion — a separate, security-sensitive decision (PDF
JavaScript execution), out of scope here.

Other still-open polish: cross-page Tab (navigation is currently same-page),
comb cells aren't drawn in the edit overlay (plain edit + max-length only),
XFA-only forms fall back to read-only.

Reference implementation analyzed: `mupdf/platform/gl/gl-form.c`,
`gl-input.c`, `gl-main.c` (the mupdf OpenGL viewer's form filling). It polls
widgets each frame and edits text/choice in a **modal dialog**; we want
**in-place** editing instead, but the mupdf API surface and the
mutate → update → re-render loop are the same.

---

## 1. What already exists in SumatraPDF (leverage, don't rebuild)

- **Widgets are already rendered.** The page render path runs widget appearance
  streams: `EngineMupdf.cpp:3538-3541` (`pdf_run_page_widgets_with_usage` /
  `pdf_run_page_with_usage`). So a checked checkbox / filled text already shows;
  they're just non-interactive.
- **Widgets are already hit-testable.** `GetFzPageInfo` walks
  `pdf_first_annot`/`pdf_next_annot` (includes widgets) into `pi->annotations`
  (`EngineMupdf.cpp:3261-3271`); `EngineMupdfGetAnnotationAtPos`
  (`EngineMupdf.cpp:4660-4704`) returns the smallest annot containing a point —
  widgets included. `AnnotationType::Widget` exists (`Annotation.h`, value 20 ==
  `PDF_ANNOT_WIDGET`).
- **Mutation → re-render pipeline exists.** Every annotation setter in
  `Annotation.cpp` (e.g. `SetContents` 351-373) does: `ScopedCritSec(&docLock)` →
  `pdf_set_*` → `pdf_update_annot` → (outside docLock) `MarkNotificationAsModified`
  which drops `pageInfo->displayList` under `renderLock` and sets the dirty flag
  (`EngineMupdf.cpp:4721-4775`); the caller then calls `MainWindowRerender`
  (`SumatraPDF.cpp:2917`). Form edits funnel through the **same** path.
- **Save / dirty / unsaved-changes is reusable as-is.** `modifiedAnnotations` →
  `EngineMupdfHasUnsavedAnnotations` (`EngineMupdf.cpp:4610`) →
  `EngineMupdfSaveUpdated` / `pdf_save_document` (~4390) → `CmdSaveAnnotations`
  (Ctrl+Shift+S). A filled field is just a modified `pdf_obj`.
- **Coordinate mapping exists.** `DisplayModel::CvtFromScreen(Point/Rect,pageNo)`
  (1197/1216) for click→page; `CvtToScreen(pageNo, RectF)` (1191) to place an
  overlay over a field rect; `GetPageNoByPoint` (1097).
- **Floated child-edit pattern exists.** `HomePage.cpp:962` creates a `WC_EDITW`
  as a child of `win->hwndCanvas` — exactly the pattern for an in-place text box
  over a field.

## 2. What's missing (the actual work)

1. Classify a widget: type (text/checkbox/radio/combo/listbox/signature),
   field flags (read-only, multiline, password, comb, required), value, options.
2. Mutate a widget value through mupdf under `docLock`, regenerate appearance,
   invalidate.
3. Route input: a **click-a-field** branch in the canvas, a **keyboard/focus
   path** (there is *no* canvas keyboard handler today — keyboard is frame-level),
   and a per-window "field being edited" state.
4. The direct-manipulation overlays: floated edit for text, dropdown for choice,
   immediate toggle for buttons; field highlight + Tab navigation.

---

## 3. mupdf API surface (verified against gl-form.c / form.h / annot.h)

Enumerate / classify:
- `pdf_annot *pdf_first_widget(ctx, pdf_page*)`, `pdf_next_widget(ctx, pdf_annot*)`
- `enum pdf_widget_type pdf_widget_type(ctx, pdf_annot*)` →
  `PDF_WIDGET_TYPE_{BUTTON,CHECKBOX,COMBOBOX,LISTBOX,RADIOBUTTON,SIGNATURE,TEXT}`
- `fz_rect pdf_bound_widget(ctx, pdf_annot*)` (page space)
- `int pdf_annot_field_flags(ctx, pdf_annot*)` → `PDF_FIELD_IS_READ_ONLY`,
  `PDF_TX_FIELD_IS_MULTILINE|PASSWORD|COMB`, `PDF_BTN_FIELD_IS_RADIO|PUSHBUTTON`,
  `PDF_CH_FIELD_IS_COMBO|EDIT|MULTI_SELECT`, `PDF_FIELD_IS_REQUIRED`, …
- `const char *pdf_annot_field_label(ctx, pdf_annot*)`,
  `const char *pdf_annot_field_value(ctx, pdf_annot*)`
- `int pdf_text_widget_max_len(ctx, pdf_annot*)`,
  `int pdf_text_widget_format(ctx, pdf_annot*)` (NONE/NUMBER/SPECIAL/DATE/TIME)

Read/write values:
- Text commit (runs validate/format/calculate/JS):
  `int pdf_set_text_field_value(ctx, pdf_annot*, const char *value)` — returns
  true if accepted.
- Text live keystroke filter (maxlen/comb/format, no side effects):
  `int pdf_edit_text_field_value(ctx, pdf_annot*, value, change, int *selStart,
  int *selEnd, char **newvalue)`.
- Choice: `int pdf_choice_widget_options(ctx, pdf_annot*, int exportval,
  const char *opts[])` (call with NULL to get count), `pdf_choice_widget_value`,
  `int pdf_set_choice_field_value(ctx, pdf_annot*, const char *value)`,
  `pdf_choice_widget_is_multiselect`.
- Checkbox/radio: `int pdf_toggle_widget(ctx, pdf_annot*)` (used by gl-form.c:740-744;
  **confirm it's present in our mupdf header during Phase 0** — fallback is
  `pdf_button_field_on_state` + `pdf_set_field_value` / the event helpers).
- Generic: `int pdf_set_annot_field_value(ctx, pdf_document*, pdf_annot*, text,
  int ignore_trigger_events)`.

Event triggers (run the field's AA JavaScript; optional for v1):
`pdf_annot_event_{enter,exit,down,up,focus,blur}(ctx, pdf_annot*)`.

Re-render / recalc:
- `int pdf_update_page(ctx, pdf_page*)` — regenerates out-of-date appearance
  streams, runs form recalc, **returns true if redraw needed**. (gl-main.c:1101.)
- `int pdf_update_annot(ctx, pdf_annot*)` — per-annot version (annot.h:881).
- `int pdf_update_open_pages(ctx, pdf_document*)` — a JS calculate can change a
  field on another page; use after edits if the form has calculations.

Important header note (annot.h:865-899): pass the **same** `pdf_annot*` objects
to `pdf_update_annot` that were last used to render; don't reload page/annots in
between or change-tracking breaks. Our display-list cache already keeps them
stable per `FzPageInfo`.

---

## 4. Architecture (three layers)

### 4a. Engine layer — `EngineMupdf.cpp` + `Annotation.cpp`
New, thin, mirroring the existing annotation setters (same locking):

- `WidgetInfo EngineMupdfGetWidgetInfo(Annotation*)` → `{ widgetType, fieldFlags,
  value, label, maxLen, txFormat, options[] }` (read under `docLock`/ctx).
- `bool EngineMupdfSetWidgetText(Annotation*, const char* val)` →
  `docLock { pdf_set_text_field_value; pdf_update_annot }`, then (outside docLock)
  `MarkNotificationAsModified`.
- `bool EngineMupdfToggleWidget(Annotation*)` → `pdf_toggle_widget` (or set state).
- `bool EngineMupdfSetWidgetChoice(Annotation*, const char* val)` /
  `...GetWidgetChoices(...)`.
- Optional `Annotation* EngineMupdfGetWidgetAtPos(...)` that skips read-only and
  signature widgets and returns type info, layered on `GetAnnotationAtPos`.

Re-render: after a successful set, call the existing `MarkNotificationAsModified`
+ `MainWindowRerender(win)`. For forms with JS calculations, also
`pdf_update_open_pages` before rerender (Phase 4).

### 4b. Input layer — `Canvas.cpp` + a per-window editing state
- New state on `WindowTab`/`MainWindow`: `Annotation* editedWidget` (+ overlay
  HWND, page no). A `MouseAction` or a separate flag for "editing a field".
- **Left button down** (`OnMouseLeftButtonDown`, `Canvas.cpp:1281`, annot branch
  ~1311): if `annot->type == Widget` and not read-only, intercept *before* the
  drag/selection path. Classify:
  - checkbox/radio → toggle immediately, re-render, done.
  - text → begin in-place text edit (4c).
  - combobox/listbox → open dropdown (4c).
  - signature/pushbutton → ignore (v1) or show info.
- **Hover** (`OnMouseMove`/`OnSetCursorMouseNone`, `Canvas.cpp:1027`/`2201`): set
  cursor (I-beam over text, hand over button/choice) and optionally draw a field
  highlight; reuse the existing `annotationUnderCursor` hover detection.
- **Keyboard / focus**: while a text field is being edited, the floated child
  edit owns the keystrokes (it's a real HWND), so we get caret/selection/IME for
  free and **don't** need a new canvas keyboard handler. Tab/Esc/Enter are caught
  in the overlay's `PreTranslateMessage` (like the find bar) to commit / move /
  cancel.

### 4c. Direct-manipulation UI (the overlays)
- **Text field:** float a `WC_EDITW` (or the wingui `Edit`) child of
  `hwndCanvas`, positioned with `CvtToScreen(pageNo, fieldBounds)`, seeded with
  `pdf_annot_field_value`, `ES_MULTILINE` when `PDF_TX_FIELD_IS_MULTILINE`,
  `ES_PASSWORD` for password fields, font size ≈ field DA size × zoom. On commit
  (Enter for single-line, focus-loss, Tab) write back via
  `EngineMupdfSetWidgetText`; destroy the overlay; re-render. Reuses Win32 text
  editing — **no custom caret/text layer** (matches the analysis recommendation
  and the floated-edit pattern at `HomePage.cpp:962`).
- **Checkbox / radio:** no text entry — toggle on click and re-render. A radio
  click selects its group's option (mupdf handles the group via the field).
- **Combobox / listbox:** float a small dropdown (reuse the wingui `ListBox` /
  `DropDown` from `CommandPalette`/`wingui`) over the field, fill from
  `pdf_choice_widget_options`, commit the picked value with
  `pdf_set_choice_field_value`. Editable comboboxes (`PDF_CH_FIELD_IS_EDIT`) also
  allow typing (Phase 2/3).
- **Navigation:** Tab / Shift+Tab → commit current and move to next/prev field;
  Enter → commit (and, for single-line, advance); Esc → cancel without writing.
  Field order: build an ordered list of editable widgets per page (by the field
  tab order if present, else top-to-bottom/left-to-right) for Tab traversal.
- **Highlight:** tint editable fields lightly on hover / when the "fill mode" is
  active so the user can see what's fillable (like browsers' form highlight).

---

## 5. Threading & re-render rules (the sharp edges — same ones the find work hit)

- All `pdf_*` field mutation runs on the **UI thread** holding **`docLock`**.
- Call `pdf_update_annot` (or `pdf_update_page`) **inside** the mutation so the
  appearance stream regenerates; otherwise the cached display list re-renders the
  **old** appearance.
- Call `MarkNotificationAsModified` **outside `docLock`** — it takes `pagesLock`
  then `docLock`/`renderLock`; calling it while holding `docLock` deadlocks
  (documented at `Annotation.cpp:206`). It drops the cached display list under
  `renderLock`.
- Then `MainWindowRerender(win)` (cancels the background renderer, repaints).
- The background render thread must not run a page while we mutate it — the
  display-list drop + `CancelRendering` in `MainWindowRerender` handle this, the
  same way annotation edits already work.

## 6. Phased plan

- **Phase 0 — spike / de-risk (½–1 day).** Confirm `f1040.pdf` is a fillable
  AcroForm (not XFA-only) in our mupdf build. On left-click of a **checkbox**
  widget: classify via `pdf_widget_type`, toggle (`pdf_toggle_widget`), re-render
  via `MarkNotificationAsModified` + `MainWindowRerender`. This proves the whole
  mutate → appearance → invalidate loop end-to-end with the least UI.
- **Phase 1 — text fields.** Floated `WC_EDITW` overlay, single-line, commit via
  `pdf_set_text_field_value`, re-render, save round-trips (fill → Ctrl+Shift+S →
  reopen shows the value). Get focus/commit/cancel solid.
- **Phase 2 — buttons + choice.** Radio groups, comboboxes & listboxes via the
  dropdown overlay and `pdf_choice_widget_*`.
- **Phase 3 — polish / navigation.** Tab/Shift+Tab/Enter/Esc traversal, hover
  cursors + field highlight, multiline & password & comb fields (live filter via
  `pdf_edit_text_field_value`), read-only/required affordances, match the edit
  font size to the field.
- **Phase 4 — robustness.** JS calculate/format fields
  (`pdf_update_open_pages`), unsaved-changes prompt on close (reuse the
  annotation dirty flag + tab "unsaved" marker), "fill mode" toggle / command,
  reset-form, RTL, high-DPI/zoom/rotation correctness.

## 7. Test plan — `f1040.pdf`

Local copy: `C:\Users\kjk\OneDrive\!sumatra\sumtest\annots\f1040.pdf`. Bring up
on a **simpler** fillable form first (fewer fields, no JS) before the 1040.

On the 1040 verify, per phase:
- **Checkboxes:** filing-status boxes (Single / Married filing jointly / …),
  "You as a dependent", digital-assets Yes/No — toggle, re-render, persist.
- **Radio groups:** any mutually-exclusive group selects one and clears siblings.
- **Text fields:** first/last name, home address — type, multiline where present.
- **Comb fields:** SSN / EIN (fixed-cell comb) — characters land in cells,
  max-length enforced.
- **Numeric fields:** the dollar-amount lines — number format; if the form has
  calculated totals, confirm `pdf_update_open_pages` recomputes them (or
  document the limitation if JS calc isn't run).
- **Save round-trip:** fill several fields, Ctrl+Shift+S, reopen → values
  present; re-open in Adobe/Chrome → values present and valid.
- **Zoom/rotate:** overlays line up with fields at 50% / 200% and rotated.

## 8. Key decisions / open questions

- **Win32 `WC_EDITW` vs wingui `Edit` vs custom caret** → start with a floated
  Win32/wingui edit (free caret/IME/selection); custom on-page caret only if the
  font-mismatch-while-editing looks bad. Decided: floated edit.
- **Live validation vs commit-only** → Phase 1 commit-only
  (`pdf_set_text_field_value` returns accept/reject); add live
  `pdf_edit_text_field_value` filtering for comb/maxlen in Phase 3.
- **JS / calculate** → v1 runs whatever the `pdf_set_*`/`pdf_update_page` path
  runs (mupdf executes form JS if built with it); don't promise full JS. Note
  whether our mupdf build enables JS.
- **XFA** → detect XFA-only forms and fall back to read-only (mupdf fills the
  AcroForm layer, not dynamic XFA).
- **Confirm `pdf_toggle_widget`** exists in our mupdf headers (gl-form.c uses it;
  one analysis pass didn't find it in an older header) — checked in Phase 0.
- **Interaction with the new find UI / annotation edit panel** → forms are a
  third input mode; make sure the floated field-edit, the find bar, and the
  annotation drag don't fight over focus/clicks.

## 9. Files to touch

- `src/EngineMupdf.cpp` — widget classify + mutate helpers (new), reuse
  `MarkNotificationAsModified` (4721), `GetAnnotationAtPos` (4660), render path.
- `src/Annotation.cpp/.h` — `WidgetInfo` + setters mirroring `SetContents` (351).
- `src/Canvas.cpp` — click-a-field branch in `OnMouseLeftButtonDown` (1281) /
  `OnMouseLeftButtonUp` (1453); hover cursor (1027/2201).
- New `src/FormFields.cpp/.h` (or fold into Canvas) — the editing-state machine +
  the floated edit / dropdown overlays + Tab navigation.
- `src/DisplayModel.cpp` — `CvtToScreen`/`CvtFromScreen` (already there).
- `src/SumatraPDF.cpp` — `MainWindowRerender` (2917), a "Fill form" command /
  mode if desired, unsaved-changes prompt.
- Save path: `EngineMupdfSaveUpdated` / `CmdSaveAnnotations` (reused as-is).
