---
name: fix-crash
description: Analyze a SumatraPDF minidump by crash id (cdb, symbols, stack, log) and fix the bug when the cause is clear. Use when the user gives a crash id, minidump, .dmp, or asks to fix/analyze a crash. Use when the user runs /fix-crash.
---

# Fix a crash

Crash id looks like `2026-09-08-02-40-963a`. Dumps live in `.work/crashes/<id>/`.

## 1. Analyze the dump

Run (do not use `bun cmd/crashes.ts <id>` — that starts a local server):

```
bun cmd/analyze-crash.ts <crash-id>
```

`-reanalyze` to force cdb again. `--local` for `http://127.0.0.1:9321`.

The helper: finds or downloads the `.dmp`, extracts `log.txt` / `settings.txt`, fetches PDBs for that build, runs `cdb` (`!analyze -v`, crashed-thread `kb`, `~*kb`), writes `analyze.txt` and `summary.txt`, prints the summary.

If the dump is missing and download fails, stop and say so.

## 2. Read artifacts

Stdout / `summary.txt` first. Then only what you still need:

- `analyze.txt` — `=== crashed thread ===` and `=== !analyze -v ===` (exception, STACK_TEXT, FAILURE_BUCKET). Skip `=== all threads ===` unless the crashed stack is empty or looks like a hang.
- `log.txt` — header (`Ver`, `Git`, `Exe`) and the log tail (files opened, last actions).
- `settings.txt` — only if the stack or log points at settings.

Map debugger paths `...\sumatrapdf\src\...` / `ext\...` to this repo. Ignore CRT (`memcpy.asm`, `vctools`).

## 3. Source at the crashing revision

`Git:` in the log is the build. Do not checkout.

```
git log -1 --oneline <sha>
git log <sha>..HEAD -- <stack files>
git show <sha>:<path>
```

Read the current files too. If `git log <sha>..HEAD -- <files>` already contains a fix, say so and stop unless the user wants a further change.

## 4. Diagnose, then fix

Explain: exception, first in-repo frame, why it died, whether HEAD still has the bug.

**Do not guess a patch.** If the stack is unsymbolicated, the dump is a hang, or the bug is data-dependent with no repro, report that and stop.

If the cause is clear and the user asked to fix it:

- Bug-fix rule: write the test first, see it fail, then fix, see it pass. Test naming: `tests/issue-<n>.ts` when there is a GitHub issue; otherwise a focused unit test or say nothing covers it.
- Repro PDFs: `C:\Users\kjk\OneDrive\!sumatra\bugs\bug-<n>...`. Launch with `-for-testing`.
- `ext/mupdf` changes need a matching `ext/patches/` file in the same commit.
- clang-format `src/` edits; do not format `ext/`.
- Do not commit unless asked.

If a GitHub issue number is known, the commit subject ends with `(fixes #<n>)`.
