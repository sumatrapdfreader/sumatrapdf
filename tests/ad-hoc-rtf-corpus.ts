// Exercises RTF support on a big corpus of real files and records the results in
// docs/rtf_to_check.md. One process per document, so a crash only costs that file.
// Identical files are tested once. State is kept in .work/rtf-corpus, so a run resumes.
//
//   bun tests/ad-hoc-rtf-corpus.ts scan       find the files, write the list
//   bun tests/ad-hoc-rtf-corpus.ts load       load and render every page with ASan
//   bun tests/ad-hoc-rtf-corpus.ts text       compare our text with RichEdit's
//   bun tests/ad-hoc-rtf-corpus.ts report     rewrite the list from the state
//
// Build the tool first: bun cmd/ng-build.ts -dbg -asan test_engines
import {
  copyFileSync,
  existsSync,
  mkdirSync,
  readFileSync,
  readdirSync,
  rmSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { dirname, join } from "node:path";

const ROOT = join(import.meta.dir, "..");
const CORPUS_DIR = "E:\\backup\\books";
const WORK = join(ROOT, ".work", "rtf-corpus");
const STATE = join(WORK, "state.json");
const TMP = join(WORK, "tmp");
const LOGS = join(WORK, "logs");
const TEXT = join(WORK, "text");
const MD = join(ROOT, "docs", "rtf_to_check.md");
const TOOL = join(ROOT, "out", "win", "dbg-asan", "test_engines.exe");
const N_WORKERS = 16;
const LOAD_TIMEOUT_MS = 300_000;
const RICHEDIT_TIMEOUT_MS = 120_000;
const RICHEDIT_BATCH = 25;
// share of RichEdit's words we must have, and of ours it must have
const TEXT_OK_RATIO = 0.98;

type FileEntry = { path: string; size: number; hash: string };
type LoadResult = { status: string; pages?: number; ms: number; detail?: string };
type TextResult = { status: string; words?: number; ourWords?: number; missing?: number; extra?: number };
type State = {
  files: FileEntry[];
  load: Record<string, LoadResult>;
  text: Record<string, TextResult>;
  notes: Record<string, string>;
};

function loadState(): State {
  return JSON.parse(readFileSync(STATE, "utf8"));
}

function saveState(s: State): void {
  writeFileSync(STATE + ".tmp", JSON.stringify(s));
  rmSync(STATE, { force: true });
  copyFileSync(STATE + ".tmp", STATE);
  rmSync(STATE + ".tmp", { force: true });
}

function* walk(dir: string): Generator<string> {
  let names: import("node:fs").Dirent[] = [];
  try {
    names = readdirSync(dir, { withFileTypes: true });
  } catch {
    return;
  }
  for (const d of names) {
    const p = join(dir, d.name);
    if (d.isDirectory()) {
      yield* walk(p);
    } else if (d.isFile() && d.name.toLowerCase().endsWith(".rtf")) {
      yield p;
    }
  }
}

function scan(): void {
  mkdirSync(WORK, { recursive: true });
  const files: FileEntry[] = [];
  for (const path of walk(CORPUS_DIR)) {
    const hash = new Bun.CryptoHasher("md5").update(readFileSync(path)).digest("hex");
    files.push({ path, size: statSync(path).size, hash });
  }
  files.sort((a, b) => a.path.localeCompare(b.path));
  const prev: State | null = existsSync(STATE) ? loadState() : null;
  const s: State = { files, load: prev?.load ?? {}, text: prev?.text ?? {}, notes: prev?.notes ?? {} };
  saveState(s);
  console.log(`${files.length} files, ${new Set(files.map((f) => f.hash)).size} unique`);
  report(s);
}

// one file per content hash
function uniqueFiles(s: State): FileEntry[] {
  const seen = new Set<string>();
  const res: FileEntry[] = [];
  for (const f of s.files) {
    if (!seen.has(f.hash)) {
      seen.add(f.hash);
      res.push(f);
    }
  }
  return res;
}

async function runTool(
  args: string[],
  timeoutMs: number,
): Promise<{ code: number | null; out: string; timedOut: boolean }> {
  const proc = Bun.spawn([TOOL, ...args], { stdout: "pipe", stderr: "pipe", stdin: "ignore", cwd: dirname(TOOL) });
  let timedOut = false;
  const timer = setTimeout(() => {
    timedOut = true;
    proc.kill();
  }, timeoutMs);
  const [stdout, stderr] = await Promise.all([new Response(proc.stdout).text(), new Response(proc.stderr).text()]);
  const code = await proc.exited;
  clearTimeout(timer);
  return { code, out: stdout + "\n" + stderr, timedOut };
}

function classify(r: { code: number | null; out: string; timedOut: boolean }, isRtf: boolean): LoadResult {
  const pages = Number(/^pages: (\d+)/m.exec(r.out)?.[1] ?? -1);
  const res: LoadResult = { status: "ok", ms: 0 };
  if (pages >= 0) {
    res.pages = pages;
  }
  if (r.timedOut) {
    res.status = "TIMEOUT";
  } else if (r.out.includes("AddressSanitizer")) {
    res.status = "ASAN";
    res.detail = /ERROR: AddressSanitizer: (\S+)/.exec(r.out)?.[1];
  } else if (/assertion failed/i.test(r.out)) {
    res.status = "ASSERT";
    res.detail = /assertion failed.*/i.exec(r.out)?.[0];
  } else if (r.out.includes("failed to load")) {
    res.status = isRtf ? "LOAD-FAILED" : "not-rtf";
  } else if (r.out.includes("render failed")) {
    res.status = "RENDER-FAILED";
  } else if (r.code !== 0) {
    res.status = "CRASH";
    res.detail = `exit ${r.code}`;
  } else if (pages === 0) {
    res.status = "NO-PAGES";
  }
  return res;
}

async function parallel<T>(items: T[], n: number, fn: (item: T) => Promise<void>): Promise<void> {
  let i = 0;
  const worker = async () => {
    while (i < items.length) {
      await fn(items[i++]!);
    }
  };
  await Promise.all(Array.from({ length: n }, worker));
}

async function load(): Promise<void> {
  const s = loadState();
  mkdirSync(TMP, { recursive: true });
  mkdirSync(LOGS, { recursive: true });
  const todo = uniqueFiles(s).filter((f) => !s.load[f.hash]);
  console.log(`load: ${todo.length} to do`);
  let done = 0;
  await parallel(todo, N_WORKERS, async (f) => {
    const tmp = join(TMP, f.hash + ".rtf");
    const started = Date.now();
    let res: LoadResult;
    try {
      copyFileSync(f.path, tmp);
      const isRtf = readFileSync(tmp).subarray(0, 5).toString("latin1") === "{\\rtf";
      const r = await runTool([tmp], LOAD_TIMEOUT_MS);
      res = classify(r, isRtf);
      if (res.status !== "ok" && res.status !== "not-rtf") {
        writeFileSync(join(LOGS, f.hash + ".txt"), `${f.path}\n\n${r.out}`);
      }
    } catch (e) {
      res = { status: "HARNESS-ERROR", ms: 0, detail: String(e) };
    }
    res.ms = Date.now() - started;
    rmSync(tmp, { force: true });
    s.load[f.hash] = res;
    done++;
    if (done % 100 === 0) {
      saveState(s);
      report(s);
      console.log(`load: ${done}/${todo.length}`);
    }
  });
  saveState(s);
  report(s);
  console.log("load: finished");
}

// RichEdit, the engine of WordPad, as the reference. Dumps "<hash>.re.txt" per file.
const kRichEditPs1 = String.raw`
param([string]$listPath)
Add-Type -AssemblyName System.Windows.Forms
foreach ($line in [IO.File]::ReadAllLines($listPath)) {
  $parts = $line.Split("|")
  try {
    $r = New-Object System.Windows.Forms.RichTextBox
    $r.LoadFile($parts[0], [System.Windows.Forms.RichTextBoxStreamType]::RichText)
    [IO.File]::WriteAllText($parts[1], $r.Text, [Text.Encoding]::UTF8)
    $r.Dispose()
  } catch {
    [IO.File]::WriteAllText($parts[1] + ".err", $_.Exception.Message)
  }
}
`;

function words(text: string): Map<string, number> {
  const m = new Map<string, number>();
  // RichEdit's text keeps soft hyphens, which aren't word breaks
  for (const w of text
    .replaceAll("­", "")
    .toLowerCase()
    .split(/[^\p{L}\p{N}]+/u)) {
    if (w) {
      m.set(w, (m.get(w) ?? 0) + 1);
    }
  }
  return m;
}

function sumCounts(m: Map<string, number>): number {
  let n = 0;
  for (const v of m.values()) {
    n += v;
  }
  return n;
}

function compareText(ours: string, theirs: string): TextResult {
  const a = words(ours);
  const b = words(theirs);
  let common = 0;
  for (const [w, n] of b) {
    common += Math.min(n, a.get(w) ?? 0);
  }
  const ourWords = sumCounts(a);
  const theirWords = sumCounts(b);
  const missing = theirWords - common;
  const extra = ourWords - common;
  const ok =
    missing <= Math.max(2, theirWords * (1 - TEXT_OK_RATIO)) && extra <= Math.max(2, ourWords * (1 - TEXT_OK_RATIO));
  return { status: ok ? "match" : "DIFFERS", words: theirWords, ourWords, missing, extra };
}

async function textPhase(): Promise<void> {
  const s = loadState();
  mkdirSync(TMP, { recursive: true });
  mkdirSync(TEXT, { recursive: true });
  const todo = uniqueFiles(s).filter((f) => !s.text[f.hash] && s.load[f.hash]?.status === "ok");
  console.log(`text: ${todo.length} to do`);
  const ps1 = join(WORK, "richedit.ps1");
  writeFileSync(ps1, kRichEditPs1);
  const batches: FileEntry[][] = [];
  for (let i = 0; i < todo.length; i += RICHEDIT_BATCH) {
    batches.push(todo.slice(i, i + RICHEDIT_BATCH));
  }
  let done = 0;
  await parallel(batches, 12, async (batch) => {
    const list = join(WORK, `batch-${batch[0]!.hash}.txt`);
    const rows: string[] = [];
    for (const f of batch) {
      const tmp = join(TMP, f.hash + ".rtf");
      copyFileSync(f.path, tmp);
      rows.push(`${tmp}|${join(TEXT, f.hash + ".re.txt")}`);
    }
    writeFileSync(list, rows.join("\r\n"));
    const proc = Bun.spawn(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", ps1, list], {
      stdout: "ignore",
      stderr: "ignore",
      stdin: "ignore",
    });
    const timer = setTimeout(() => proc.kill(), RICHEDIT_TIMEOUT_MS * 3);
    await proc.exited;
    clearTimeout(timer);
    rmSync(list, { force: true });
    for (const f of batch) {
      const tmp = join(TMP, f.hash + ".rtf");
      const ourPath = join(TEXT, f.hash + ".ours.txt");
      const rePath = join(TEXT, f.hash + ".re.txt");
      let res: TextResult;
      if (!existsSync(rePath)) {
        res = { status: "richedit-failed" };
      } else {
        const r = await runTool([tmp, "-dump-text", ourPath], LOAD_TIMEOUT_MS);
        if (r.code !== 0 || !existsSync(ourPath)) {
          res = { status: "DUMP-FAILED" };
        } else {
          const ours = readFileSync(ourPath, "utf8");
          const theirs = readFileSync(rePath, "utf8");
          res = compareText(ours, theirs);
        }
      }
      rmSync(tmp, { force: true });
      s.text[f.hash] = res;
      done++;
    }
    if (done % 200 < RICHEDIT_BATCH) {
      saveState(s);
      report(s);
      console.log(`text: ${done}/${todo.length}`);
    }
  });
  saveState(s);
  report(s);
  console.log("text: finished");
}

function counts(values: string[]): string {
  const m = new Map<string, number>();
  for (const v of values) {
    m.set(v, (m.get(v) ?? 0) + 1);
  }
  return [...m.entries()]
    .sort((a, b) => b[1] - a[1])
    .map(([k, v]) => `${k}: ${v}`)
    .join(", ");
}

function escapeCell(s: string): string {
  return s.replace(/\|/g, "\\|");
}

function report(s: State): void {
  const uniq = uniqueFiles(s);
  const loadDone = uniq.filter((f) => s.load[f.hash]);
  const textDone = uniq.filter((f) => s.text[f.hash]);
  const out: string[] = [];
  out.push("# RTF files to check");
  out.push("");
  out.push(`Corpus: \`${CORPUS_DIR}\`, all \`*.rtf\` files, recursively. Generated by \`tests/ad-hoc-rtf-corpus.ts\`.`);
  out.push("");
  out.push("## Progress");
  out.push("");
  out.push(`- Files: ${s.files.length}, unique by content: ${uniq.length}. Identical files are tested once.`);
  out.push(`- Load and render every page with ASan (\`test_engines\`): ${loadDone.length} / ${uniq.length}`);
  out.push(`  - ${counts(loadDone.map((f) => s.load[f.hash]!.status)) || "not started"}`);
  out.push(`- Text compared with RichEdit (the WordPad engine): ${textDone.length} / ${uniq.length}`);
  out.push(`  - ${counts(textDone.map((f) => s.text[f.hash]!.status)) || "not started"}`);
  out.push("");
  const notes = readNotes();
  if (notes) {
    out.push(notes.trimEnd());
    out.push("");
  }
  const bad = uniq.filter((f) => {
    const l = s.load[f.hash]?.status;
    return l && l !== "ok" && l !== "not-rtf";
  });
  out.push("## Load and render problems");
  out.push("");
  if (bad.length === 0) {
    out.push("None so far.");
  } else {
    out.push("| Status | Detail | Size | File |");
    out.push("|---|---|---:|---|");
    for (const f of bad) {
      const l = s.load[f.hash]!;
      out.push(`| ${l.status} | ${l.detail ?? ""} | ${f.size} | ${escapeCell(f.path)} |`);
    }
  }
  out.push("");
  const differs = uniq
    .filter((f) => s.text[f.hash]?.status === "DIFFERS")
    .sort((a, b) => (s.text[b.hash]!.missing ?? 0) - (s.text[a.hash]!.missing ?? 0));
  out.push("## Text differs from RichEdit");
  out.push("");
  out.push("Words counted case-insensitively. Missing: RichEdit has them and we don't. Extra: the reverse.");
  out.push("");
  if (differs.length === 0) {
    out.push("None so far.");
  } else {
    out.push("| RichEdit words | Our words | Missing | Extra | File |");
    out.push("|---:|---:|---:|---:|---|");
    for (const f of differs) {
      const t = s.text[f.hash]!;
      out.push(`| ${t.words} | ${t.ourWords} | ${t.missing} | ${t.extra} | ${escapeCell(f.path)} |`);
    }
  }
  out.push("");
  out.push("## All files");
  out.push("");
  out.push("| Load | Pages | Text | Size | File |");
  out.push("|---|---:|---|---:|---|");
  for (const f of s.files) {
    const l = s.load[f.hash];
    const t = s.text[f.hash];
    out.push(`| ${l?.status ?? ""} | ${l?.pages ?? ""} | ${t?.status ?? ""} | ${f.size} | ${escapeCell(f.path)} |`);
  }
  out.push("");
  writeFileSync(MD, out.join("\n"));
}

// hand-written findings, kept between regenerations of the list
function readNotes(): string {
  const p = join(WORK, "notes.md");
  return existsSync(p) ? readFileSync(p, "utf8") : "";
}

const cmd = process.argv[2];
if (cmd === "scan") {
  scan();
} else if (cmd === "load") {
  await load();
} else if (cmd === "text") {
  await textPhase();
} else if (cmd === "report") {
  report(loadState());
} else {
  console.log("usage: bun tests/ad-hoc-rtf-corpus.ts scan|load|text|report");
}
