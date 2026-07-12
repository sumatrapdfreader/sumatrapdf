import { existsSync, statSync, writeFileSync } from "node:fs";
import { basename, join, normalize } from "node:path";

type ModuleInfo = {
  id: number;
  obj: string;
  lib: string;
  project: string;
};

type SectionContrib = {
  sectionName: string;
  section: number;
  start: number;
  size: number;
  mod: number;
};

type SectionHeader = {
  section: number;
  name: string;
  virtualSize: number;
  rawSize: number;
};

type Bucket = {
  key: string;
  size: number;
  count: number;
  sections: Map<string, number>;
};

const defaultExe = join("out", "rel64", "SumatraPDF-static.exe");
const defaultPdb = join("out", "rel64", "SumatraPDF-static.pdb");
const sectionColumns = [".text", ".rdata", ".data", ".pdata", ".rsrc", ".reloc"];

function usage(exitCode = 1): never {
  console.error(`Usage: bun cmd/exe-sizes.ts [options]

Breaks down the static x64 release SumatraPDF-static.exe size by project.

Options:
  -exe <path>       EXE to summarize (default: ${defaultExe})
  -pdb <path>       Matching PDB to analyze (default: ${defaultPdb})
  -out <path>       Report path (default: <exe-dir>/exe-sizes.txt)
  -top <n>          Number of object files shown (default: 40)
  -raw              Keep raw llvm-pdbutil dumps next to the report
  -h, --help        Show this help
`);
  process.exit(exitCode);
}

function parseArgs() {
  const args = process.argv.slice(2);
  let exe = defaultExe;
  let pdb = defaultPdb;
  let out = "";
  let top = 40;
  let raw = false;

  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg === "-h" || arg === "--help") {
      usage(0);
    } else if (arg === "-raw") {
      raw = true;
    } else if (arg === "-exe" || arg === "--exe") {
      exe = args[++i] ?? usage();
    } else if (arg === "-pdb" || arg === "--pdb") {
      pdb = args[++i] ?? usage();
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
    out = join(exe.replace(/[\\/][^\\/]+$/, ""), "exe-sizes.txt");
  }
  return { exe, pdb, out, top, raw };
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
    throw new Error(`llvm-pdbutil ${args.join(" ")} failed with ${exitCode}\n${stderr}`);
  }
  return stdout;
}

async function checkLlvmPdbutil(): Promise<void> {
  try {
    await runPdbutil(["--help"]);
  } catch {
    throw new Error("llvm-pdbutil not found in PATH. Run from a Visual Studio developer environment.");
  }
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
  const contribs: SectionContrib[] = [];
  const re =
    /^\s*SC\[([^\]]+)\]\s+\|\s+mod\s+=\s+(\d+),\s+([0-9A-Fa-f]+):(\d+),\s+size\s+=\s+(\d+)/;
  for (const line of raw.split(/\r?\n/)) {
    const m = line.match(re);
    if (!m) {
      continue;
    }
    contribs.push({
      sectionName: m[1],
      mod: parseInt(m[2], 10),
      section: parseInt(m[3], 16),
      start: parseInt(m[4], 10),
      size: parseInt(m[5], 10),
    });
  }
  return contribs;
}

function parseSectionHeaders(raw: string): SectionHeader[] {
  const headers: SectionHeader[] = [];
  let section = 0;
  let name = "";
  let virtualSize = 0;
  let rawSize = 0;

  for (const line of raw.split(/\r?\n/)) {
    const sectionMatch = line.match(/^\s*SECTION HEADER #(\d+)/);
    if (sectionMatch) {
      if (section && name) {
        headers.push({ section, name, virtualSize, rawSize });
      }
      section = parseInt(sectionMatch[1], 10);
      name = "";
      virtualSize = 0;
      rawSize = 0;
      continue;
    }

    const nameMatch = line.match(/^\s*([.$_A-Za-z][^\s]*)\s+name$/);
    if (nameMatch) {
      name = nameMatch[1];
      continue;
    }

    const virtualMatch = line.match(/^\s*([0-9A-Fa-f]+)\s+virtual size$/);
    if (virtualMatch) {
      virtualSize = parseInt(virtualMatch[1], 16);
      continue;
    }

    const rawMatch = line.match(/^\s*([0-9A-Fa-f]+)\s+size of raw data$/);
    if (rawMatch) {
      rawSize = parseInt(rawMatch[1], 16);
    }
  }

  if (section && name) {
    headers.push({ section, name, virtualSize, rawSize });
  }
  return headers;
}

function addToBucket(map: Map<string, Bucket>, key: string, size: number, sectionName: string): void {
  let bucket = map.get(key);
  if (!bucket) {
    bucket = { key, size: 0, count: 0, sections: new Map() };
    map.set(key, bucket);
  }
  bucket.size += size;
  bucket.count++;
  bucket.sections.set(sectionName, (bucket.sections.get(sectionName) ?? 0) + size);
}

function projectName(objPath: string, libPath: string): string {
  const originalObj = normalize(objPath).replaceAll("\\", "/");
  const originalLib = normalize(libPath).replaceAll("\\", "/");
  const s = `${originalLib} ${originalObj}`.toLowerCase();

  if (s.includes("/github/stl/") || s.includes("libcpmt")) {
    return "msvc-stl";
  }
  if (s.includes("/vcstartup/") || s.includes("libcmt") || s.includes("libvcruntime") || s.includes("libucrt")) {
    return "msvc-crt";
  }
  if (s.includes("/windows kits/")) {
    return "windows-sdk";
  }
  if (s.includes("/packages/microsoft.web.webview2.")) {
    return "webview2";
  }

  for (const marker of ["/out/rel64/obj-s/", "/out/rel64/obj/"]) {
    for (const path of [originalObj, originalLib]) {
      const idx = path.toLowerCase().indexOf(marker);
      if (idx < 0) {
        continue;
      }
      const rest = path.slice(idx + marker.length);
      const first = rest.split("/")[0] || "(unknown)";
      return first.toLowerCase().endsWith(".lib") ? basename(first, ".lib") : first;
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

function objectName(module?: ModuleInfo): string {
  if (!module) {
    return "(linker-generated)";
  }
  const obj = normalize(module.obj).replaceAll("\\", "/");
  const marker = "/out/rel64/obj-s/";
  const idx = obj.toLowerCase().indexOf(marker);
  if (idx >= 0) {
    return obj.slice(idx + marker.length);
  }
  return obj;
}

function sectionValue(bucket: Bucket, name: string): number {
  return bucket.sections.get(name) ?? 0;
}

function sortedBuckets(map: Map<string, Bucket>): Bucket[] {
  return [...map.values()].sort((a, b) => b.size - a.size || a.key.localeCompare(b.key));
}

function formatSize(n: number): string {
  return n.toLocaleString();
}

function formatPercent(n: number, total: number): string {
  if (total === 0) {
    return "0.0%";
  }
  return `${((n * 100) / total).toFixed(1)}%`;
}

function renderBucketTable(title: string, buckets: Bucket[], total: number, limit = Number.MAX_SAFE_INTEGER): string[] {
  const shown = buckets.slice(0, limit);
  const lines = ["", title, "-".repeat(title.length)];
  const headers = ["bytes", "percent", "contribs", "project", ...sectionColumns, "other"];
  const rows = shown.map((b) => {
    const knownSections = sectionColumns.reduce((sum, s) => sum + sectionValue(b, s), 0);
    return [
      formatSize(b.size),
      formatPercent(b.size, total),
      b.count.toLocaleString(),
      b.key,
      ...sectionColumns.map((s) => formatSize(sectionValue(b, s))),
      formatSize(b.size - knownSections),
    ];
  });
  const widths = headers.map((h, i) => Math.max(h.length, ...rows.map((r) => r[i].length)));
  lines.push(headers.map((h, i) => h.padStart(widths[i])).join("  "));
  for (const row of rows) {
    lines.push(row.map((v, i) => (i === 3 ? v.padEnd(widths[i]) : v.padStart(widths[i]))).join("  "));
  }
  if (buckets.length > shown.length) {
    lines.push(`... ${buckets.length - shown.length} more`);
  }
  return lines;
}

function renderSectionHeaders(headers: SectionHeader[]): string[] {
  const lines = ["", "PE Sections", "-----------"];
  lines.push("section  virtual bytes  raw bytes");
  for (const h of headers) {
    lines.push(`${h.name.padEnd(8)}  ${formatSize(h.virtualSize).padStart(13)}  ${formatSize(h.rawSize).padStart(9)}`);
  }
  const virtualTotal = headers.reduce((sum, h) => sum + h.virtualSize, 0);
  const rawTotal = headers.reduce((sum, h) => sum + h.rawSize, 0);
  lines.push(`${"total".padEnd(8)}  ${formatSize(virtualTotal).padStart(13)}  ${formatSize(rawTotal).padStart(9)}`);
  return lines;
}

function renderReport(
  exe: string,
  pdb: string,
  exeSize: number,
  modules: Map<number, ModuleInfo>,
  contribs: SectionContrib[],
  headers: SectionHeader[],
  top: number,
): string {
  const projectBuckets = new Map<string, Bucket>();
  const objectBuckets = new Map<string, Bucket>();
  let linkerGeneratedSize = 0;
  let attributedSize = 0;

  for (const contrib of contribs) {
    const module = modules.get(contrib.mod);
    const project = module?.project ?? "(linker-generated)";
    const object = objectName(module);
    attributedSize += contrib.size;
    if (!module) {
      linkerGeneratedSize += contrib.size;
    }
    addToBucket(projectBuckets, project, contrib.size, contrib.sectionName);
    addToBucket(objectBuckets, object, contrib.size, contrib.sectionName);
  }

  const projects = sortedBuckets(projectBuckets);
  const objects = sortedBuckets(objectBuckets);
  const lines: string[] = [];
  lines.push(`SumatraPDF-static.exe size report`);
  lines.push(`Generated: ${new Date().toISOString()}`);
  lines.push(`EXE: ${exe}`);
  lines.push(`PDB: ${pdb}`);
  lines.push("");
  lines.push(`EXE file size: ${formatSize(exeSize)} bytes`);
  lines.push(`Attributed section contribution size: ${formatSize(attributedSize)} bytes`);
  lines.push(`Unattributed file/padding/header size: ${formatSize(Math.max(0, exeSize - attributedSize))} bytes`);
  lines.push(`PDB modules: ${modules.size.toLocaleString()}`);
  lines.push(`Section contributions: ${contribs.length.toLocaleString()}`);
  lines.push(`Linker-generated contribution size: ${formatSize(linkerGeneratedSize)} bytes`);
  lines.push("");
  lines.push("Notes:");
  lines.push("- Sizes come from `llvm-pdbutil dump --section-contribs`, grouped through `dump --modules`.");
  lines.push("- This measures linked contribution size, not static .lib file size; unused library members are not counted.");
  lines.push("- PE headers, alignment padding, certificate data, and gaps are shown only in the unattributed total.");
  lines.push(...renderBucketTable("By Project", projects, attributedSize));
  lines.push(...renderBucketTable(`Top ${top} Object Files`, objects, attributedSize, top));
  lines.push(...renderSectionHeaders(headers));
  return lines.join("\n") + "\n";
}

async function main() {
  const { exe, pdb, out, top, raw } = parseArgs();
  if (!existsSync(exe)) {
    throw new Error(`expected static release exe at ${exe}; build SumatraPDF Release|x64 first`);
  }
  if (!existsSync(pdb)) {
    throw new Error(`expected PDB at ${pdb}; build SumatraPDF Release|x64 first`);
  }

  await checkLlvmPdbutil();

  console.log(`Reading modules from ${pdb}...`);
  const modulesRaw = await runPdbutil(["dump", "--modules", pdb]);
  console.log("Reading section contributions...");
  const contribsRaw = await runPdbutil(["dump", "--section-contribs", pdb]);
  console.log("Reading section headers...");
  const headersRaw = await runPdbutil(["dump", "--section-headers", pdb]);

  if (raw) {
    writeFileSync(`${out}.modules.raw.txt`, modulesRaw);
    writeFileSync(`${out}.section-contribs.raw.txt`, contribsRaw);
    writeFileSync(`${out}.section-headers.raw.txt`, headersRaw);
  }

  const modules = parseModules(modulesRaw);
  const contribs = parseSectionContribs(contribsRaw);
  const headers = parseSectionHeaders(headersRaw);
  const report = renderReport(exe, pdb, statSync(exe).size, modules, contribs, headers, top);
  writeFileSync(out, report);

  const attributedSize = contribs.reduce((sum, c) => sum + c.size, 0);
  console.log(`Attributed ${formatSize(attributedSize)} bytes across ${contribs.length.toLocaleString()} contributions.`);
  console.log(`Wrote ${out}`);
}

await main();
