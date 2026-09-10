---
name: fix-crashes
description: Download today's SumatraPDF crash minidumps, analyze them with cdb, fix the ones whose cause is clear and commit after each fix. Skips crashes already analyzed today or already covered by an earlier fix. Use when the user runs /fix-crashes.
---

# Fix today's crashes

Batch loop over today's minidumps. For one known crash id use `/fix-crash` instead;
`.claude/skills/fix-crash/SKILL.md` has the detail on reading `analyze.txt` / `log.txt`
that this skill assumes.

Committing after each fix is part of this skill: the usual "never commit
automatically" rule does not apply here. Do **not** push.

## 0. Memory files

Both live under `.work/` (gitignored), so they survive across sessions but never
reach the repo. Create them if missing.

- `.work/crashes/fixed.md` — cross-day memory of crash **signatures**, so a crash
  like one already handled is skipped without running cdb on it again.
- `.work/crashes/analyzed-<yyyy-mm-dd>.md` — one line per crash id handled on that
  day (the day the crash happened, i.e. the id's date prefix).

`fixed.md` entry:

```
## <short name, e.g. null EngineBase in TocLoad>
- signature: <exception> at <func> (<file>:<line>)
- bucket: <FAILURE_BUCKET_ID, if any>
- status: fixed <sha> | no-fix: <reason>
- first seen: <yyyy-mm-dd>   ids: <id>, <id>
- notes: cause in one or two lines, plus what to look for in a similar dump
```

`analyzed-<day>.md` line:

```
- <crash-id> — <fixed <sha> | dup of "<fixed.md section>" | no-fix: <reason>> — <one line>
```

## 1. List today's crashes

```
bun cmd/crashes.ts --list --today
```

Prints `id,version,date,size,ip` and exits (no downloads, no local server). Drop
ids already in `.work/crashes/analyzed-<today>.md`. If nothing is left, say so and stop.

## 2. Per crash: analyze

```
bun cmd/analyze-crash.ts <crash-id>
```

Writes `.work/crashes/<id>/{analyze.txt,log.txt,settings.txt,summary.txt}` and prints
the summary (exception, bucket, in-repo stack, log tail).

Build the signature from `exception` + the first in-repo frame + `bucket`. If it
matches a `fixed.md` entry, append that id to the entry's `ids:`, write a `dup of`
line to `analyzed-<today>.md`, and move to the next crash — do not re-diagnose.

## 3. Per crash: diagnose

`Git:` in `log.txt` is the build. Don't checkout; read at that revision:

```
git log <sha>..HEAD -- <stack files>
git show <sha>:<path>
```

If `git log <sha>..HEAD` already contains the fix, record it as `no-fix: already fixed in <sha>`
and move on. Map `...\sumatrapdf\src\...` / `ext\...` to this repo; ignore CRT frames.

**Do not guess a patch.** Record `no-fix: <reason>` and move on when the stack is
unsymbolicated, the dump is a hang, the crash is entirely in third-party or OS code,
or the bug is data-dependent with no repro. Three no-fix crashes in a row is not
failure — most dumps are not actionable.

## 4. Per crash: fix and commit

Only when the cause is clear:

- write the test first, see it fail, then fix, see it pass. `tests/issue-<n>.ts`
  when there is a GitHub issue; otherwise a unit test, or say plainly that nothing covers it
- repro files: `C:\Users\kjk\OneDrive\!sumatra\bugs\bug-<n>...`; launch with `-for-testing`
- `ext/mupdf` edits need a matching `ext/patches/` file in the same commit
- clang-format the `src/` files you touched; never format `ext/`
- build (`bun cmd/build.ts -dbg`) and run only the tests the change can break

Then commit that one fix on its own:

```
Fix <what> (fixes #<n>)      # the (fixes #n) part only when there is an issue

<why, wrapped at 72>

crash: <crash-id> (<exception> at <func>, <file>:<line>)
```

After the commit, update `fixed.md` (status `fixed <sha>`) and `analyzed-<today>.md`,
then go to the next crash. One crash at a time — never batch several fixes into one commit.

## 5. Report

When the list is exhausted, print a table: crash id, signature, outcome (fixed `<sha>` /
dup / no-fix reason). Say that nothing was pushed.
