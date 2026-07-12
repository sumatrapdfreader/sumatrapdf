import { existsSync, writeFileSync } from "node:fs";
import { basename, join, normalize } from "node:path";

type ModuleInfo = {
  id: number;
  obj: string;
  lib: string;
  project: string;
};

type SectionContrib = {
  section: number;
  start: number;
  end: number;
  mod: number;
};

type SectionHeader = {
  section: number;
  name: string;
  rva: number;
  size: number;
};

type SymbolInfo = {
  kind: string;
  rva: number;
  section?: number;
  offset: number;
  size: number;
  name: string;
  module?: ModuleInfo;
};

type Bucket = {
  key: string;
  size: number;
  count: number;
  symbols: SymbolInfo[];
};

const defaultPdb = join("out", "rel64", "SumatraPDF-static.pdb");
const defaultExe = join("out", "rel64", "SumatraPDF-static.exe");

const stlSymbolPattern =
  /\bstd::|stdext::|`eh vector |std::(vector|basic_string|string|wstring|shared_ptr|unique_ptr|weak_ptr|optional|variant|function|map|set|unordered_map|unordered_set|deque|list|array|span|tuple|pair|allocator|char_traits|exception|ios|locale|mutex|thread|chrono)\b|\?\$?(vector|basic_string|shared_ptr|unique_ptr|weak_ptr|optional|variant|function|map|set|unordered_map|unordered_set|deque|list)@/i;

function usage(exitCode = 1): never {
  console.error(`Usage: bun cmd/stl-pollution.ts [options]

Analyzes STL-looking symbols in the static x64 release SumatraPDF PDB.

Options:
  -pdb <path>       PDB to analyze (default: ${defaultPdb})
  -exe <path>       EXE expected next to the PDB (default: ${defaultExe})
  -out <path>       Report path (default: <pdb-dir>/stl-pollution.txt)
  -top <n>          Number of symbols shown per bucket (default: 20)
  -raw              Keep raw llvm-pdbutil outputs next to the report
  -h, --help        Show this help
`);
  process.exit(exitCode);
}

function parseArgs() {
  const args = process.argv.slice(2);
  let pdb = defaultPdb;
  let exe = defaultExe;
  let out = "";
  let top = 20;
  let raw = false;

  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg === "-h" || arg === "--help") {
      usage(0);
    } else if (arg === "-raw") {
      raw = true;
    } else if (arg === "-pdb" || arg === "--pdb") {
      pdb = args[++i] ?? usage();
    } else if (arg === "-exe" || arg === "--exe") {
      exe = args[++i] ?? usage();
    } else if (arg === "-out" || arg === "--out") {
      out = args[++i] ?? usage();
    } else if (arg === "-top" || arg === "--top") {
      top = parseInt(args[++i] ?? "", 10);
      if (!Number.isFinite(top) || top <= 0) {
        usage();
      }
    } else {
      usage();
    }
  }

  if (!out) {
    out = join(pdb.replace(/[\\/][^\\/]+$/, ""), "stl-pollution.txt");
  }
  return { pdb, exe, out, top, raw };
}

async function runPdbutil(args: string[]): Promise<string> {
  const proc = Bun.spawn(["llvm-pdbutil", ...args], {
    stdout: "pipe",
    stderr: "pipe",
  });
  const stdout = await new Response(proc.stdout).text();
  const stderr = await new Response(proc.stderr).text();
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    throw new Error(
      `llvm-pdbutil ${args.join(" ")} failed with ${exitCode}\n${stderr}`,
    );
  }
  return stdout;
}

async function checkLlvmPdbutil(): Promise<void> {
  try {
    await runPdbutil(["--help"]);
  } catch {
    throw new Error(
      "llvm-pdbutil not found in PATH. Run from a Visual Studio developer environment.",
    );
  }
}

function formatSize(n: number): string {
  return `${n.toLocaleString()} B`;
}

function parseModules(raw: string): Map<number, ModuleInfo> {
  const modules = new Map<number, ModuleInfo>();
  let current: { id: number; obj: string } | undefined;
  const modRe = /^\s*Mod\s+(\d+)\s+\|\s+`([^`]+)`:/;
  const objRe = /^\s*Obj:\s+`([^`]+)`:/;

  for (const line of raw.split(/\r?\n/)) {
    const mod = line.match(modRe);
    if (mod) {
      current = { id: parseInt(mod[1], 10), obj: mod[2] };
      continue;
    }
    const obj = line.match(objRe);
    if (obj && current) {
      const lib = obj[1];
      modules.set(current.id, {
        id: current.id,
        obj: current.obj,
        lib,
        project: projectName(current.obj, lib),
      });
      current = undefined;
    }
  }
  return modules;
}

function parseSectionContribs(raw: string): SectionContrib[] {
  const res: SectionContrib[] = [];
  const re =
    /^\s*SC\[[^\]]+\]\s+\|\s+mod\s+=\s+(\d+),\s+([0-9A-Fa-f]+):([0-9A-Fa-f]+),\s+size\s+=\s+(\d+)/;
  for (const line of raw.split(/\r?\n/)) {
    const m = line.match(re);
    if (!m) {
      continue;
    }
    const mod = parseInt(m[1], 10);
    const section = parseInt(m[2], 16);
    const start = parseInt(m[3], 10);
    const size = parseInt(m[4], 10);
    res.push({ section, start, end: start + size, mod });
  }
  res.sort(
    (a, b) => a.section - b.section || a.start - b.start || a.end - b.end,
  );
  return res;
}

function parseSectionHeaders(raw: string): SectionHeader[] {
  const headers: SectionHeader[] = [];
  let section = 0;
  let name = "";
  let size = 0;
  let rva = 0;

  for (const line of raw.split(/\r?\n/)) {
    const sectionMatch = line.match(/^\s*SECTION HEADER #(\d+)/);
    if (sectionMatch) {
      if (section && name) {
        headers.push({ section, name, rva, size });
      }
      section = parseInt(sectionMatch[1], 10);
      name = "";
      size = 0;
      rva = 0;
      continue;
    }

    const nameMatch = line.match(/^\s*([.$_A-Za-z][^\s]*)\s+name$/);
    if (nameMatch) {
      name = nameMatch[1];
      continue;
    }

    const sizeMatch = line.match(/^\s*([0-9A-Fa-f]+)\s+virtual size$/);
    if (sizeMatch) {
      size = parseInt(sizeMatch[1], 16);
      continue;
    }

    const rvaMatch = line.match(/^\s*([0-9A-Fa-f]+)\s+virtual address$/);
    if (rvaMatch) {
      rva = parseInt(rvaMatch[1], 16);
    }
  }

  if (section && name) {
    headers.push({ section, name, rva, size });
  }
  return headers;
}

function parseGlobals(raw: string): SymbolInfo[] {
  const symbols: SymbolInfo[] = [];
  const re = /^\s+(\w+)\s+\[0x([0-9A-Fa-f]+)[^|]*\|\s+sizeof=(\d+)\]\s+(.+)$/;
  for (const line of raw.split(/\r?\n/)) {
    const m = line.match(re);
    if (!m) {
      continue;
    }
    const kind = m[1];
    const name = stripSymbolNoise(m[4].trim());
    if (!stlSymbolPattern.test(name)) {
      continue;
    }
    const rva = parseInt(m[2], 16);
    symbols.push({
      kind,
      rva,
      offset: rva,
      size: parseInt(m[3], 10),
      name,
    });
  }
  return symbols;
}

function stripSymbolNoise(name: string): string {
  if (name.startsWith("(FPO) ")) {
    return name.slice(6);
  }
  if (name.startsWith("(RSP) ")) {
    return name.slice(6);
  }
  return name;
}

function attachModules(
  symbols: SymbolInfo[],
  headers: SectionHeader[],
  contribs: SectionContrib[],
  modules: Map<number, ModuleInfo>,
): void {
  const bySection = new Map<number, SectionContrib[]>();
  for (const c of contribs) {
    let list = bySection.get(c.section);
    if (!list) {
      list = [];
      bySection.set(c.section, list);
    }
    list.push(c);
  }

  for (const s of symbols) {
    const header = findSectionHeader(headers, s.rva);
    if (!header) {
      continue;
    }
    s.section = header.section;
    s.offset = s.rva - header.rva;
    const contrib = findContrib(bySection.get(s.section) ?? [], s.offset);
    if (contrib) {
      s.module = modules.get(contrib.mod);
    }
  }
}

function findSectionHeader(
  headers: SectionHeader[],
  rva: number,
): SectionHeader | undefined {
  return headers.find((h) => rva >= h.rva && rva < h.rva + h.size);
}

function findContrib(
  contribs: SectionContrib[],
  offset: number,
): SectionContrib | undefined {
  let lo = 0;
  let hi = contribs.length - 1;
  while (lo <= hi) {
    const mid = (lo + hi) >> 1;
    const c = contribs[mid];
    if (offset < c.start) {
      hi = mid - 1;
    } else if (offset >= c.end) {
      lo = mid + 1;
    } else {
      return c;
    }
  }
  return;
}

function projectName(objPath: string, libPath: string): string {
  const s = normalize(`${libPath} ${objPath}`)
    .replaceAll("\\", "/")
    .toLowerCase();
  const originalLib = normalize(libPath).replaceAll("\\", "/");
  const originalObj = normalize(objPath).replaceAll("\\", "/");

  if (s.includes("/github/stl/") || s.includes("libcpmt_")) {
    return "msvc-stl";
  }
  if (s.includes("/vcstartup/") || s.includes("libcmt_")) {
    return "msvc-crt";
  }
  if (s.includes("/windows kits/")) {
    return "windows-sdk";
  }

  for (const marker of ["/out/rel64/obj-s/", "/out/rel64/obj/"]) {
    const idx = originalObj.toLowerCase().indexOf(marker);
    if (idx >= 0) {
      const rest = originalObj.slice(idx + marker.length);
      return rest.split("/")[0] || "(unknown)";
    }
  }

  const extIdx = originalLib.toLowerCase().indexOf("/ext/");
  if (extIdx >= 0) {
    return originalLib.slice(extIdx + 5).split("/")[0] || "ext";
  }
  const pkgIdx = originalLib.toLowerCase().indexOf("/packages/");
  if (pkgIdx >= 0) {
    return originalLib.slice(pkgIdx + 10).split("/")[0] || "packages";
  }
  const srcIdx = originalObj.toLowerCase().indexOf("/src/");
  if (srcIdx >= 0) {
    return "src";
  }
  return basename(originalLib || originalObj) || "(unknown)";
}

function objectName(m?: ModuleInfo): string {
  if (!m) {
    return "(unmapped)";
  }
  return normalize(m.obj).replaceAll("\\", "/");
}

function bucketBy(
  symbols: SymbolInfo[],
  keyFn: (s: SymbolInfo) => string,
): Bucket[] {
  const map = new Map<string, Bucket>();
  for (const s of symbols) {
    const key = keyFn(s);
    let b = map.get(key);
    if (!b) {
      b = { key, size: 0, count: 0, symbols: [] };
      map.set(key, b);
    }
    b.size += s.size;
    b.count++;
    b.symbols.push(s);
  }
  const buckets = [...map.values()];
  for (const b of buckets) {
    b.symbols.sort((a, b) => b.size - a.size);
  }
  buckets.sort((a, b) => b.size - a.size);
  return buckets;
}

function moduleStlRuntimeBuckets(
  modules: Map<number, ModuleInfo>,
  contribs: SectionContrib[],
): Bucket[] {
  const map = new Map<number, Bucket>();
  for (const c of contribs) {
    const m = modules.get(c.mod);
    if (!m || m.project !== "msvc-stl") {
      continue;
    }
    let b = map.get(m.id);
    if (!b) {
      b = { key: objectName(m), size: 0, count: 0, symbols: [] };
      map.set(m.id, b);
    }
    b.size += c.end - c.start;
    b.count++;
  }
  return [...map.values()].sort((a, b) => b.size - a.size);
}

function renderBucketTable(
  title: string,
  buckets: Bucket[],
  limit: number,
): string[] {
  const lines = [``, title, "-".repeat(title.length)];
  const shown = buckets.slice(0, limit);
  const width = Math.max(
    4,
    ...shown.map((b) => b.size.toLocaleString().length),
  );
  for (const b of shown) {
    lines.push(
      `${b.size.toLocaleString().padStart(width)}  ${b.count.toString().padStart(5)}  ${b.key}`,
    );
  }
  if (buckets.length > shown.length) {
    lines.push(`... ${buckets.length - shown.length} more`);
  }
  return lines;
}

function renderSymbolList(
  title: string,
  symbols: SymbolInfo[],
  limit: number,
): string[] {
  const lines = [``, title, "-".repeat(title.length)];
  const shown = symbols.slice(0, limit);
  const width = Math.max(
    4,
    ...shown.map((s) => s.size.toLocaleString().length),
  );
  for (const s of shown) {
    lines.push(
      `${s.size.toLocaleString().padStart(width)}  ${s.module?.project ?? "(unmapped)"}  ${objectName(s.module)}`,
    );
    lines.push(`        ${s.name}`);
  }
  if (symbols.length > shown.length) {
    lines.push(`... ${symbols.length - shown.length} more`);
  }
  return lines;
}

function renderReport(
  pdb: string,
  symbols: SymbolInfo[],
  modules: Map<number, ModuleInfo>,
  contribs: SectionContrib[],
  top: number,
): string {
  symbols.sort((a, b) => b.size - a.size);
  const total = symbols.reduce((sum, s) => sum + s.size, 0);
  const unmapped = symbols.filter((s) => !s.module);
  const projectBuckets = bucketBy(
    symbols,
    (s) => s.module?.project ?? "(unmapped)",
  );
  const objectBuckets = bucketBy(symbols, (s) => objectName(s.module));
  const runtimeBuckets = moduleStlRuntimeBuckets(modules, contribs);
  const runtimeTotal = runtimeBuckets.reduce((sum, b) => sum + b.size, 0);

  const lines: string[] = [];
  lines.push(`STL pollution report for ${pdb}`);
  lines.push(`Generated: ${new Date().toISOString()}`);
  lines.push(``);
  lines.push(`Matched STL-looking globals: ${symbols.length.toLocaleString()}`);
  lines.push(`Matched STL-looking global code/data size: ${formatSize(total)}`);
  lines.push(`Unmapped matched symbols: ${unmapped.length.toLocaleString()}`);
  lines.push(
    `MSVC STL runtime section-contrib size: ${formatSize(runtimeTotal)} across ${runtimeBuckets.length.toLocaleString()} objects`,
  );
  lines.push(``);
  lines.push(`Notes:`);
  lines.push(
    `- Symbol sizes come from "llvm-pdbutil pretty --globals --symbol-order=size".`,
  );
  lines.push(
    `- Ownership is inferred by converting symbol RVAs through "dump --section-headers", then mapping to "dump --section-contribs" module ranges.`,
  );
  lines.push(
    `- The "MSVC STL runtime" table is object-level section contribution size for libcpmt STL objects, not just matched std:: symbol names.`,
  );
  lines.push(
    `- Inline/template STL code is attributed to the object file that emitted it.`,
  );
  lines.push(...renderBucketTable("By Project", projectBuckets, top));
  lines.push(...renderBucketTable("By Object File", objectBuckets, top));
  lines.push(
    ...renderBucketTable("MSVC STL Runtime Objects", runtimeBuckets, top),
  );
  lines.push(...renderSymbolList("Largest Matched STL Symbols", symbols, top));

  lines.push(``);
  lines.push(`Top Symbols Per Project`);
  lines.push(`-----------------------`);
  for (const b of projectBuckets.slice(0, top)) {
    lines.push(``);
    lines.push(
      `${b.key}: ${formatSize(b.size)} in ${b.count.toLocaleString()} symbols`,
    );
    for (const s of b.symbols.slice(0, Math.min(10, top))) {
      lines.push(
        `  ${s.size.toLocaleString().padStart(8)}  ${objectName(s.module)}`,
      );
      lines.push(`            ${s.name}`);
    }
  }

  return lines.join("\n") + "\n";
}

async function main() {
  const { pdb, exe, out, top, raw } = parseArgs();
  if (!existsSync(exe)) {
    throw new Error(
      `expected static release exe at ${exe}; build SumatraPDF Release|x64 first`,
    );
  }
  if (!existsSync(pdb)) {
    throw new Error(
      `expected PDB at ${pdb}; build SumatraPDF Release|x64 first`,
    );
  }

  await checkLlvmPdbutil();

  console.log(`Reading globals from ${pdb}...`);
  const globalsRaw = await runPdbutil([
    "pretty",
    "--globals",
    "--symbol-order=size",
    pdb,
  ]);
  console.log(`Reading modules...`);
  const modulesRaw = await runPdbutil(["dump", "--modules", pdb]);
  console.log(`Reading section contributions...`);
  const contribsRaw = await runPdbutil(["dump", "--section-contribs", pdb]);
  console.log(`Reading section headers...`);
  const headersRaw = await runPdbutil(["dump", "--section-headers", pdb]);

  if (raw) {
    writeFileSync(`${out}.globals.raw.txt`, globalsRaw);
    writeFileSync(`${out}.modules.raw.txt`, modulesRaw);
    writeFileSync(`${out}.section-contribs.raw.txt`, contribsRaw);
    writeFileSync(`${out}.section-headers.raw.txt`, headersRaw);
  }

  const modules = parseModules(modulesRaw);
  const headers = parseSectionHeaders(headersRaw);
  const contribs = parseSectionContribs(contribsRaw);
  const symbols = parseGlobals(globalsRaw);
  attachModules(symbols, headers, contribs, modules);

  const report = renderReport(pdb, symbols, modules, contribs, top);
  writeFileSync(out, report);

  const total = symbols.reduce((sum, s) => sum + s.size, 0);
  console.log(
    `Matched ${symbols.length.toLocaleString()} STL-looking globals (${formatSize(total)}).`,
  );
  console.log(`Wrote ${out}`);
}

await main();
