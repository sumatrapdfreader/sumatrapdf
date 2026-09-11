// Analyze one cached (or downloaded) minidump: symbols + cdb → analyze.txt.
//
//   bun cmd/analyze-crash.ts <crash-id>
//   bun cmd/analyze-crash.ts -reanalyze <crash-id>
//   bun cmd/analyze-crash.ts --local <crash-id>
import { existsSync, readdirSync, writeFileSync, readFileSync } from "node:fs";
import { join } from "node:path";

import {
  CACHE_DIR,
  LOCAL_SERVER,
  PROD_SERVER,
  analyzePath,
  downloadDumpIfMissing,
  dumpPath,
  extractDumpLog,
  isAnalyzed,
  isLogExtracted,
  isSettingsExtracted,
  logPath,
  relLog,
  runAnalysis,
  type DumpRow,
} from "./crashes";

function usage(): void {
  console.log(`Usage:
  bun cmd/analyze-crash.ts [--local] [-reanalyze] <crash-id>

Looks in .work/crashes/<id>/ for the minidump. If missing, downloads it.
Extracts log/settings, fetches PDBs for that build, runs cdb (!analyze -v; ~*kb),
writes analyze.txt and summary.txt, prints the summary.`);
}

function parseArgs(argv: string[]): { server: string; id: string; reanalyze: boolean } {
  let server = PROD_SERVER;
  let id = "";
  let reanalyze = false;
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === "--help" || a === "-h") {
      usage();
      process.exit(0);
    }
    if (a === "--local") {
      server = LOCAL_SERVER;
      continue;
    }
    if (a === "-reanalyze" || a === "-re-analyze" || a === "--reanalyze" || a === "--re-analyze") {
      reanalyze = true;
      continue;
    }
    if (a.startsWith("-")) {
      throw new Error(`unknown flag ${a}`);
    }
    if (id) {
      throw new Error("only one crash id");
    }
    id = a;
  }
  if (!id) {
    usage();
    process.exit(1);
  }
  return { server, id, reanalyze };
}

function normalizeId(raw: string): string {
  let s = raw.trim();
  s = s.replace(/\\/g, "/");
  const slash = s.lastIndexOf("/");
  if (slash >= 0) {
    s = s.slice(slash + 1);
  }
  s = s.replace(/\.dmp$/i, "");
  return s;
}

function listLocalIds(): string[] {
  if (!existsSync(CACHE_DIR)) {
    return [];
  }
  return readdirSync(CACHE_DIR, { withFileTypes: true })
    .filter((d) => d.isDirectory() && d.name !== "symbols")
    .map((d) => d.name);
}

function resolveId(raw: string): string {
  const id = normalizeId(raw);
  const local = listLocalIds();
  if (local.includes(id)) {
    return id;
  }
  const matches = local.filter((x) => x === id || x.endsWith(id) || x.startsWith(id) || x.includes(id));
  if (matches.length === 1) {
    return matches[0];
  }
  if (matches.length > 1) {
    throw new Error(`crash id '${raw}' is ambiguous:\n  ${matches.join("\n  ")}`);
  }
  return id;
}

function readIf(p: string): string {
  return existsSync(p) ? readFileSync(p, "utf8") : "";
}

function field(text: string, name: string): string {
  const re = new RegExp(`^${name}:\\s*(.+)$`, "im");
  const m = re.exec(text);
  return m ? m[1].trim() : "";
}

function versionFromLog(log: string): string {
  return field(log, "Ver");
}

function gitFromLog(log: string): string {
  const m = /^Git:\s*([0-9a-f]{7,40})/im.exec(log);
  return m ? m[1] : "";
}

// the ReportIf() that fired, for a debug report. It, not the failure bucket,
// is what identifies one: the bucket always names the crash handler's own wait
function condFromLog(log: string): string {
  return field(log, "Cond");
}

function analyzeField(txt: string, name: string): string {
  const re = new RegExp(`^${name}:\\s*(.+)$`, "im");
  const m = re.exec(txt);
  return m ? m[1].trim() : "";
}

type StackFrame = {
  func: string;
  file: string;
  line: string;
};

function repoPathFromDbg(p: string): string {
  const n = p.replaceAll("/", "\\");
  const m = n.match(/sumatrapdf\\(src|ext)\\(.+)$/i);
  if (!m) {
    return "";
  }
  return `${m[1].toLowerCase() === "ext" ? "ext" : "src"}/${m[2].replaceAll("\\", "/")}`;
}

function parseInRepoFrames(analyzeTxt: string): StackFrame[] {
  const crashed = analyzeTxt.indexOf("=== crashed thread ===");
  let body = analyzeTxt;
  if (crashed >= 0) {
    const rest = analyzeTxt.slice(crashed);
    const next = rest.search(/\n=== /);
    body = next >= 0 ? rest.slice(0, next) : rest;
  }
  const frames: StackFrame[] = [];
  const seen = new Set<string>();
  const re = /!([^\s\[]+)(?:\s+\[([^\]]+) @ (\d+)\])?/;
  // a debug report's stack starts inside the crash handler itself (it parks
  // there while another thread writes the .dmp); the caller is what matters
  const skipFiles = new Set(["src/base/CrashHandler.cpp", "src/base/DbgHelpDyn.cpp"]);
  for (const line of body.split(/\r?\n/)) {
    const m = re.exec(line);
    if (!m) {
      continue;
    }
    const func = m[1].replace(/\+0x[0-9a-f]+$/i, "");
    const file = m[2] ? repoPathFromDbg(m[2]) : "";
    const lineNo = m[3] || "";
    if (!file || skipFiles.has(file)) {
      continue;
    }
    const key = `${func}|${file}|${lineNo}`;
    if (seen.has(key)) {
      continue;
    }
    seen.add(key);
    frames.push({ func, file, line: lineNo });
  }
  return frames;
}

function logTail(log: string, n: number): string {
  const idx = log.search(/^-------- Log[- ]/m);
  const body = idx >= 0 ? log.slice(idx) : log;
  const lines = body.replace(/\s+$/, "").split(/\r?\n/);
  if (lines.length <= n) {
    return lines.join("\n");
  }
  return lines.slice(-n).join("\n");
}

function summaryPath(id: string): string {
  return join(CACHE_DIR, id, "summary.txt");
}

function relCrashFile(id: string, name: string): string {
  return join(".work", "crashes", id, name).replaceAll("\\", "/");
}

function buildSummary(id: string, log: string, analyzeTxt: string): string {
  const ver = versionFromLog(log);
  const git = gitFromLog(log);
  const exception = analyzeField(analyzeTxt, "EXCEPTION_CODE_STR") || analyzeField(analyzeTxt, "ExceptionCode");
  const bucket = analyzeField(analyzeTxt, "FAILURE_BUCKET_ID");
  const readAddr = analyzeField(analyzeTxt, "READ_ADDRESS");
  const writeAddr = analyzeField(analyzeTxt, "WRITE_ADDRESS");
  const frames = parseInRepoFrames(analyzeTxt);
  const site = frames[0] ? `${frames[0].func}  ${frames[0].file}:${frames[0].line}` : "";
  const cond = condFromLog(log);
  const lines: string[] = [`id: ${id}`, `ver: ${ver || "?"}`, `git: ${git || "?"}`, `exception: ${exception || "?"}`];
  if (cond) {
    lines.push(`cond: ${cond}`);
  }
  if (bucket) {
    lines.push(`bucket: ${bucket}`);
  }
  if (readAddr) {
    lines.push(`read_address: ${readAddr}`);
  }
  if (writeAddr) {
    lines.push(`write_address: ${writeAddr}`);
  }
  if (site) {
    lines.push(`site: ${site}`);
  }
  lines.push("");
  if (frames.length) {
    lines.push("stack (in-repo):");
    for (const f of frames) {
      lines.push(`  ${f.func}  ${f.file}:${f.line}`);
    }
    lines.push("");
  }
  lines.push("files:");
  lines.push(`  ${relCrashFile(id, "analyze.txt")}`);
  if (isLogExtracted(id)) {
    lines.push(`  ${relCrashFile(id, "log.txt")}`);
  }
  if (isSettingsExtracted(id)) {
    lines.push(`  ${relCrashFile(id, "settings.txt")}`);
  }
  lines.push(`  ${relCrashFile(id, "summary.txt")}`);
  lines.push("");
  const tail = logTail(log, 40);
  if (tail) {
    lines.push("log tail:", tail, "");
  }
  return `${lines.join("\n")}\n`;
}

async function main(): Promise<void> {
  const { server, id: rawId, reanalyze } = parseArgs(process.argv.slice(2));
  const id = resolveId(rawId);
  const dmp = dumpPath(id);
  if (!existsSync(dmp)) {
    await downloadDumpIfMissing(server, id);
  }
  if (!existsSync(dmp)) {
    throw new Error(`no minidump at ${dmp} (and download failed or was skipped)`);
  }
  extractDumpLog(id, reanalyze);
  const log = isLogExtracted(id) ? readIf(logPath(id)) : "";
  const version = versionFromLog(log);
  if (!version) {
    throw new Error(`no Ver: in ${relLog(id) || "log"}; cannot pick PDBs`);
  }
  const row: DumpRow = { id, version, date: "", size: 0, ip: "" };
  if (reanalyze || !isAnalyzed(id)) {
    await runAnalysis(row, reanalyze);
  }
  const analyzeTxt = isAnalyzed(id) ? readIf(analyzePath(id)) : "";
  const summary = buildSummary(id, log, analyzeTxt);
  writeFileSync(summaryPath(id), summary);
  process.stdout.write(summary);
}

if (import.meta.main) {
  try {
    await main();
  } catch (e) {
    console.error(e instanceof Error ? e.message : e);
    process.exit(1);
  }
}
