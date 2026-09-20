// Analyze one cached (or downloaded) minidump: symbols + cdb → analyze.txt,
// summary.txt. Same cache files as cmd/crashes.ts, so neither redoes the other's work.
//
//   bun cmd/analyze-crash.ts <crash-id>
//   bun cmd/analyze-crash.ts -reanalyze <crash-id>
//   bun cmd/analyze-crash.ts --local <crash-id>
import { existsSync, readdirSync, readFileSync } from "node:fs";

import {
  CACHE_DIR,
  LOCAL_SERVER,
  PROD_SERVER,
  downloadDumpIfMissing,
  dumpPath,
  extractDumpLog,
  field,
  isAnalyzed,
  isLogExtracted,
  logPath,
  runAnalysis,
  writeSummary,
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
  const log = isLogExtracted(id) ? readFileSync(logPath(id), "utf8") : "";
  // no Ver: in the log means no pdbs; cdb still runs (unsymbolicated)
  const row: DumpRow = { id, version: field(log, "Ver"), date: "", size: 0, ip: "" };
  if (reanalyze || !isAnalyzed(id)) {
    await runAnalysis(row, reanalyze);
  }
  process.stdout.write(writeSummary(id));
}

if (import.meta.main) {
  try {
    await main();
  } catch (e) {
    console.error(e instanceof Error ? e.message : e);
    process.exit(1);
  }
}
