// List, download and symbolicate crash reports of the ng macOS build.
//
//   bun cmd/ng-crashes.ts                 list (oldest first); download + symbolicate missing
//   bun cmd/ng-crashes.ts <id>            download one report, symbolicate, print
//   bun cmd/ng-crashes.ts -file <path>    symbolicate a local report (e.g. crashinfo/sumatrapdfcrash.txt)
//   --local                               use http://127.0.0.1:9321
//   --since <id or yyyy-mm-dd>            only reports whose id is greater
//
// A report has each frame as "module + offset" and each module's Mach-O UUID
// (src/base/CrashHandler_mac.cpp). Symbolication is atos against the .dSYM
// with that UUID, found via Spotlight, out/mac/*/ or .work/ng-crashes/symbols/,
// where the .dSYM.zip of an uploaded build (build-ng-mac.ts) is downloaded to.
// Reports are cached as .work/ng-crashes/<id>.crash, symbolicated as <id>.txt.
import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { basename, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";

const ROOT = resolve(join(import.meta.dir, ".."));
const CACHE_DIR = join(ROOT, ".work", "ng-crashes");
const SYMBOLS_DIR = join(CACHE_DIR, "symbols");
const PROD_SERVER = "https://www.sumatrapdfreader.org";
const LOCAL_SERVER = "http://127.0.0.1:9321";
const APP = "sumatrapdfng-mac";
const FILES_HOST = "https://files.sumatrapdfreader.org/software/sumatrapdfng/mac";
const SECRETS_GO = resolve(ROOT, "..", "hack", "webapps", "sumatra-website", "server", "secrets.go");

type Frame = { lineNo: number; idx: number; addr: bigint; module: string };
type Module = { name: string; loadAddr: string; uuid: string; path: string };

function run(cmd: string, args: string[]): string {
  const r = spawnSync(cmd, args, { encoding: "utf8", maxBuffer: 64 * 1024 * 1024 });
  return r.status === 0 ? r.stdout : "";
}

// "UUID: CF9941D8-... (arm64) /path/SumatraPDF.dSYM/Contents/Resources/DWARF/SumatraPDF"
function dwarfFileWithUuid(dsym: string, uuid: string): string {
  for (const line of run("dwarfdump", ["--uuid", dsym]).split("\n")) {
    const m = line.match(/^UUID: (\S+) \(\S+\) (.+)$/);
    if (m && m[1].toUpperCase() === uuid) return m[2];
  }
  return "";
}

function dsymCandidates(uuid: string): string[] {
  const res = run("mdfind", [`com_apple_xcode_dsym_uuids == ${uuid}`])
    .split("\n")
    .filter(Boolean);
  const dirs = [SYMBOLS_DIR];
  if (existsSync(SYMBOLS_DIR)) {
    for (const e of readdirSync(SYMBOLS_DIR, { withFileTypes: true })) {
      if (e.isDirectory()) dirs.push(join(SYMBOLS_DIR, e.name));
    }
  }
  const macOut = join(ROOT, "out", "mac");
  if (existsSync(macOut)) {
    for (const cfg of readdirSync(macOut)) dirs.push(join(macOut, cfg));
  }
  for (const dir of dirs) {
    if (!existsSync(dir)) continue;
    for (const name of readdirSync(dir)) {
      if (name.endsWith(".dSYM")) res.push(join(dir, name));
    }
  }
  return res;
}

const dwarfCache = new Map<string, string>();

function findDwarfFile(uuid: string): string {
  let res = dwarfCache.get(uuid);
  if (res !== undefined) return res;
  res = "";
  for (const dsym of dsymCandidates(uuid)) {
    res = dwarfFileWithUuid(dsym, uuid);
    if (res) break;
  }
  dwarfCache.set(uuid, res);
  return res;
}

// Symbols of the uploaded build that wrote the report, into
// .work/ng-crashes/symbols/<ver>-<arch>/SumatraPDF.dSYM. A report of a build
// that was never uploaded has none.
async function downloadSymbols(report: string): Promise<void> {
  // "Ver: 26.10.03.1" or "Ver: 26.10.03.1 (dbg)"
  const ver = report.match(/^Ver: (\d+(?:\.\d+)*)/m)?.[1];
  const arch = report.match(/^Arch: (\S+)$/m)?.[1] === "x86_64" ? "x64" : "arm64";
  if (!ver) return;
  const dir = join(SYMBOLS_DIR, `${ver}-${arch}`);
  if (existsSync(dir)) return;
  const url = `${FILES_HOST}/${ver}/SumatraPDF-mac-${arch}.dSYM.zip`;
  const rsp = await fetch(url);
  if (!rsp.ok) return;
  mkdirSync(dir, { recursive: true });
  const zip = join(dir, "symbols.zip");
  await Bun.write(zip, rsp);
  run("ditto", ["-x", "-k", zip, dir]);
  rmSync(zip);
  console.error(`downloaded ${url}`);
}

function hasSumatraSymbols(report: string): boolean {
  const mod = parseModules(report.split("\n")).get("SumatraPDF");
  return !mod || findDwarfFile(mod.uuid) !== "";
}

async function symbolicate(report: string): Promise<string> {
  if (!hasSumatraSymbols(report)) {
    await downloadSymbols(report);
    dwarfCache.clear();
  }
  return symbolicateWith(report);
}

function parseModules(lines: string[]): Map<string, Module> {
  const res = new Map<string, Module>();
  const start = lines.indexOf("--- modules ---");
  if (start < 0) return res;
  for (const line of lines.slice(start + 1)) {
    const m = line.match(/^(\S+) (0x[0-9a-f]+) ([0-9A-F-]{36}) (.+)$/);
    if (m) res.set(m[1], { name: m[1], loadAddr: m[2], uuid: m[3], path: m[4] });
  }
  return res;
}

function parseFrames(lines: string[]): Frame[] {
  const res: Frame[] = [];
  lines.forEach((line, lineNo) => {
    const m = line.match(/^(\d+) (0x[0-9a-f]{16}) (\S+) \+ 0x[0-9a-f]+/);
    if (m) res.push({ lineNo, idx: parseInt(m[1]), addr: BigInt(m[2]), module: m[3] });
  });
  return res;
}

// atos prints "CrashMe() (in SumatraPDF) (Base.h:416)"; keep "CrashMe() (Base.h:416)"
function atos(dwarf: string, arch: string, loadAddr: string, addrs: bigint[]): string[] {
  const args = ["-o", dwarf, "-arch", arch, "-l", loadAddr, ...addrs.map((a) => "0x" + a.toString(16))];
  const out = run("atos", args).split("\n");
  return addrs.map((_, i) => (out[i] ?? "").replace(/ \(in [^)]+\)/, ""));
}

function symbolicateWith(report: string): string {
  const lines = report.split("\n");
  const modules = parseModules(lines);
  const frames = parseFrames(lines);
  const arch = report.match(/^Arch: (\S+)$/m)?.[1] ?? "arm64";
  const notes: string[] = [];
  for (const mod of modules.values()) {
    const modFrames = frames.filter((f) => f.module === mod.name);
    if (modFrames.length === 0) continue;
    const dwarf = findDwarfFile(mod.uuid);
    if (!dwarf) {
      // system libraries have no .dSYM; their frames keep the exported name
      if (mod.name === "SumatraPDF") notes.push(`no .dSYM with UUID ${mod.uuid} for ${mod.name}`);
      continue;
    }
    // a return address is the instruction after the call: back up into the call
    const addrs = modFrames.map((f) => (f.idx > 0 ? f.addr - 1n : f.addr));
    const syms = atos(dwarf, arch, mod.loadAddr, addrs);
    modFrames.forEach((f, i) => {
      if (!syms[i] || syms[i].startsWith("0x")) return;
      const head = lines[f.lineNo].replace(/ \(.*\)$/, "");
      lines[f.lineNo] = `${head}  ${syms[i]}`;
    });
  }
  const res = lines.join("\n");
  return notes.length > 0 ? notes.map((n) => `ng-crashes: ${n}\n`).join("") + res : res;
}

function loadPassword(): string {
  if (!existsSync(SECRETS_GO)) throw new Error(`missing secrets file: ${SECRETS_GO}`);
  const m = readFileSync(SECRETS_GO, "utf8").match(/MinidumpPassword\s*=\s*"([^"]+)"/);
  if (!m) throw new Error(`MinidumpPassword not found in ${SECRETS_GO}`);
  return m[1];
}

async function fetchAuthed(url: string): Promise<Response> {
  const auth = "Basic " + Buffer.from(":" + loadPassword()).toString("base64");
  const rsp = await fetch(url, { headers: { Authorization: auth } });
  if (!rsp.ok) throw new Error(`GET ${url}: ${rsp.status} ${await rsp.text()}`);
  return rsp;
}

// returns the path of the symbolicated report
async function ensureReport(server: string, id: string): Promise<string> {
  mkdirSync(CACHE_DIR, { recursive: true });
  const rawPath = join(CACHE_DIR, id + ".crash");
  const symPath = join(CACHE_DIR, id + ".txt");
  if (!existsSync(rawPath)) {
    const rsp = await fetchAuthed(`${server}/app/${APP}/crash/${id}`);
    writeFileSync(rawPath, await rsp.text());
  }
  if (!existsSync(symPath)) {
    writeFileSync(symPath, await symbolicate(readFileSync(rawPath, "utf8")));
  }
  return symPath;
}

// "Signal: SIGSEGV (11) code 2 addr 0x0 | CrashMe() (Base.h:416)"
function summary(report: string): string {
  const lines = report.split("\n");
  const sig = lines.find((l) => l.startsWith("Signal: ") || l.startsWith("Cond: ")) ?? "";
  const start = lines.findIndex((l) => l.startsWith("--- thread "));
  const top = start >= 0 ? (lines[start + 1] ?? "") : "";
  return `${sig} | ${top.replace(/^\d+ 0x[0-9a-f]+ /, "")}`;
}

async function listAll(server: string, since: string): Promise<void> {
  const q = since ? `?since=${encodeURIComponent(since)}` : "";
  const rsp = await fetchAuthed(`${server}/app/${APP}/crashes.txt${q}`);
  const rows = (await rsp.text()).split("\n").filter(Boolean);
  for (const row of rows) {
    const [id, ver, date] = row.split(",");
    const symPath = await ensureReport(server, id);
    console.log(`${id} ${ver} ${date} ${summary(readFileSync(symPath, "utf8"))}`);
  }
  console.log(`${rows.length} crash reports, in ${CACHE_DIR}`);
}

function usage(): void {
  console.log(`Usage:
  bun cmd/ng-crashes.ts [--local] [--since <id or yyyy-mm-dd>]   list; download + symbolicate missing
  bun cmd/ng-crashes.ts [--local] <id>                           download, symbolicate, print
  bun cmd/ng-crashes.ts -file <path>                             symbolicate a local report, print`);
}

async function main(): Promise<void> {
  let server = PROD_SERVER;
  let id = "";
  let file = "";
  let since = "";
  const argv = process.argv.slice(2);
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === "-h" || a === "-help" || a === "--help") return usage();
    if (a === "--local") server = LOCAL_SERVER;
    else if (a === "--since") since = argv[++i] ?? "";
    else if (a === "-file" || a === "--file") file = argv[++i] ?? "";
    else if (a.startsWith("-")) throw new Error(`unknown flag ${a}`);
    else id = a;
  }
  if (file) {
    console.log(await symbolicate(readFileSync(file, "utf8")));
    return;
  }
  if (id) {
    console.log(readFileSync(await ensureReport(server, basename(id, ".crash")), "utf8"));
    return;
  }
  await listAll(server, since);
}

await main();
