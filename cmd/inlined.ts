// Build release x64 and list inlined functions/methods from the PDB.
//
//   bun cmd/inlined.ts
//   bun cmd/inlined.ts -no-build
import { existsSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { detectVisualStudio2026, runLogged } from "./util";

const defaultOut = "inlined.txt";
const defaultPdbs = [join("out", "rel64", "SumatraPDF.pdb"), join("out", "rel64", "libsumatrapdf.pdb")];

type Site = { callee: string; caller: string };

function usage(exitCode = 1): never {
  console.error(`Usage: bun cmd/inlined.ts [options]

Builds Release|x64 SumatraPDF, then lists inlined functions from the PDB.

Options:
  -no-build         Do not build; use existing PDBs
  -pdb <path>       PDB to read (repeatable; default: ${defaultPdbs.join(", ")})
  -out <path>       Report path (default: ${defaultOut})
  -h, --help        Show this help`);
  process.exit(exitCode);
}

function parseArgs(): { build: boolean; pdbs: string[]; out: string } {
  const args = process.argv.slice(2);
  let build = true;
  const pdbs: string[] = [];
  let out = defaultOut;
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a === "-h" || a === "--help") {
      usage(0);
    }
    if (a === "-no-build") {
      build = false;
      continue;
    }
    if (a === "-pdb") {
      const p = args[++i];
      if (!p) {
        usage();
      }
      pdbs.push(p);
      continue;
    }
    if (a === "-out") {
      const p = args[++i];
      if (!p) {
        usage();
      }
      out = p;
      continue;
    }
    usage();
  }
  return { build, pdbs, out };
}

async function buildReleaseX64(): Promise<void> {
  const { msbuildPath } = detectVisualStudio2026();
  const sln = String.raw`vs2022\SumatraPDF.sln`;
  console.log("Building Release|x64 SumatraPDF...");
  const t0 = performance.now();
  await runLogged(msbuildPath, [sln, "/t:SumatraPDF", "/p:Configuration=Release;Platform=x64", "/m"]);
  console.log(`Build took ${((performance.now() - t0) / 1000).toFixed(1)}s`);
}

async function ensurePdbutil(): Promise<void> {
  const proc = Bun.spawn(["llvm-pdbutil", "--help"], { stdout: "pipe", stderr: "pipe" });
  const code = await proc.exited;
  if (code !== 0) {
    throw new Error("llvm-pdbutil not found in PATH. Run from a Visual Studio developer environment.");
  }
}

function extractBacktickName(line: string): string {
  const i = line.indexOf("`");
  if (i < 0) {
    return "";
  }
  const j = line.indexOf("`", i + 1);
  if (j < 0) {
    return "";
  }
  return line.slice(i + 1, j).trim();
}

function extractInlineeName(line: string): string {
  const m = line.match(/inlinee\s*=\s*0x[0-9A-Fa-f]+\s+\((.*)\),\s*parent\s*=/);
  return m ? m[1].trim() : "";
}

function recOffset(line: string): number | undefined {
  const m = line.match(/^\s*(\d+)\s+\|/);
  return m ? parseInt(m[1], 10) : undefined;
}

function recEnd(line: string): number | undefined {
  const m = line.match(/\bend = (\d+)/);
  return m ? parseInt(m[1], 10) : undefined;
}

async function collectSites(pdb: string): Promise<Site[]> {
  const proc = Bun.spawn(["llvm-pdbutil", "dump", "--symbols", pdb], {
    stdout: "pipe",
    stderr: "ignore",
  });
  if (!proc.stdout) {
    throw new Error("llvm-pdbutil produced no stdout");
  }

  type Scope = { kind: "proc" | "block" | "inline"; name: string; end: number };
  const stack: Scope[] = [];
  const sites: Site[] = [];
  let pending: { kind: Scope["kind"]; name: string } | undefined;
  let buf = "";
  const dec = new TextDecoder();
  const reader = proc.stdout.getReader();

  const currentProc = (): string => {
    for (let i = stack.length - 1; i >= 0; i--) {
      if (stack[i].kind === "proc") {
        return stack[i].name;
      }
    }
    return "";
  };

  const closeAt = (off: number) => {
    while (stack.length > 0 && stack[stack.length - 1].end === off) {
      stack.pop();
    }
  };

  const finishPending = (line: string) => {
    if (!pending) {
      return;
    }
    const end = recEnd(line) ?? 0;
    if (pending.kind === "inline") {
      const callee = extractInlineeName(line);
      pending.name = callee;
      const caller = currentProc();
      if (callee && caller) {
        sites.push({ callee, caller });
      }
    }
    stack.push({ kind: pending.kind, name: pending.name, end });
    pending = undefined;
  };

  const onLine = (line: string) => {
    const off = recOffset(line);
    if (off !== undefined) {
      if (pending) {
        finishPending("");
      }
      closeAt(off);
      if (/\|\s+S_(?:G|L)PROC32(?:_ID)?\b/.test(line) || /\|\s+S_THUNK32\b/.test(line)) {
        pending = { kind: "proc", name: extractBacktickName(line) };
        return;
      }
      if (/\|\s+S_BLOCK32\b/.test(line)) {
        pending = { kind: "block", name: "" };
        return;
      }
      if (/\|\s+S_INLINESITE\b/.test(line) && !/\|\s+S_INLINESITE_END\b/.test(line)) {
        pending = { kind: "inline", name: "" };
      }
      return;
    }
    if (pending) {
      finishPending(line);
    }
  };

  for (;;) {
    const { done, value } = await reader.read();
    if (done) {
      break;
    }
    buf += dec.decode(value, { stream: true });
    for (;;) {
      const nl = buf.indexOf("\n");
      if (nl < 0) {
        break;
      }
      onLine(buf.slice(0, nl).replace(/\r$/, ""));
      buf = buf.slice(nl + 1);
    }
  }
  if (buf) {
    onLine(buf.replace(/\r$/, ""));
  }
  const code = await proc.exited;
  if (code !== 0) {
    throw new Error(`llvm-pdbutil dump --symbols ${pdb} failed with ${code}`);
  }
  return sites;
}

type CalleeRow = { name: string; sites: number; callers: Map<string, number> };
type CallerRow = { name: string; sites: number; callees: Map<string, number> };

function aggregate(sites: Site[]): { callees: CalleeRow[]; callers: CallerRow[] } {
  const byCallee = new Map<string, CalleeRow>();
  const byCaller = new Map<string, CallerRow>();
  for (const s of sites) {
    let ce = byCallee.get(s.callee);
    if (!ce) {
      ce = { name: s.callee, sites: 0, callers: new Map() };
      byCallee.set(s.callee, ce);
    }
    ce.sites++;
    ce.callers.set(s.caller, (ce.callers.get(s.caller) || 0) + 1);

    let cr = byCaller.get(s.caller);
    if (!cr) {
      cr = { name: s.caller, sites: 0, callees: new Map() };
      byCaller.set(s.caller, cr);
    }
    cr.sites++;
    cr.callees.set(s.callee, (cr.callees.get(s.callee) || 0) + 1);
  }
  const callees = [...byCallee.values()].sort((a, b) => b.sites - a.sites || a.name.localeCompare(b.name));
  const callers = [...byCaller.values()].sort((a, b) => b.sites - a.sites || a.name.localeCompare(b.name));
  return { callees, callers };
}

function topEntries(m: Map<string, number>, n: number): string {
  const items = [...m.entries()].sort((a, b) => b[1] - a[1] || a[0].localeCompare(b[0]));
  const shown = items.slice(0, n).map(([name, c]) => `${name} (${c})`);
  if (items.length > n) {
    shown.push(`+${items.length - n} more`);
  }
  return shown.join(", ");
}

function formatReport(pdbs: string[], sites: Site[]): string {
  const { callees, callers } = aggregate(sites);
  const lines: string[] = [];
  lines.push(`Inlined functions from ${pdbs.join(", ")}`);
  lines.push(`${sites.length} inline sites, ${callees.length} callees, ${callers.length} functions with inlines`);
  lines.push("");
  lines.push("Callees (most inline sites first)");
  lines.push("---------------------------------");
  const w = String(callees[0]?.sites ?? 0).length;
  for (const c of callees) {
    lines.push(`${String(c.sites).padStart(w)}  ${c.name}`);
    lines.push(`${"".padStart(w)}    ${c.callers.size} funcs: ${topEntries(c.callers, 8)}`);
  }
  lines.push("");
  lines.push("Functions with inlines (most sites first)");
  lines.push("-----------------------------------------");
  const w2 = String(callers[0]?.sites ?? 0).length;
  for (const c of callers) {
    lines.push(`${String(c.sites).padStart(w2)}  ${c.name}`);
    lines.push(`${"".padStart(w2)}    ${c.callees.size} callees: ${topEntries(c.callees, 8)}`);
  }
  lines.push("");
  return lines.join("\n");
}

async function main(): Promise<void> {
  const opts = parseArgs();
  if (opts.build) {
    await buildReleaseX64();
  }
  await ensurePdbutil();

  const pdbs = (opts.pdbs.length > 0 ? opts.pdbs : defaultPdbs).filter((p) => existsSync(p));
  if (pdbs.length === 0) {
    throw new Error("no PDB found; build first or pass -pdb");
  }

  const sites: Site[] = [];
  for (const pdb of pdbs) {
    console.log(`Reading inline sites from ${pdb}...`);
    const t0 = performance.now();
    const got = await collectSites(pdb);
    console.log(`  ${got.length} sites in ${((performance.now() - t0) / 1000).toFixed(1)}s`);
    sites.push(...got);
  }

  const report = formatReport(pdbs, sites);
  writeFileSync(opts.out, report, "utf8");
  console.log(report);
  console.log(`Wrote ${opts.out}`);
}

if (import.meta.main) {
  try {
    await main();
  } catch (e) {
    console.error(e instanceof Error ? e.message : e);
    process.exit(1);
  }
}
