// List minidumps from the crash server, download missing dumps, run cdb.
//
//   bun cmd/crashes.ts              list (oldest first); analyze missing
//   bun cmd/crashes.ts --local      same, against http://127.0.0.1:9321
//   bun cmd/crashes.ts <id>         download dump + pdb + exe, run !analyze
import { existsSync, mkdirSync, writeFileSync, readFileSync, statSync, unlinkSync, copyFileSync } from "node:fs";
import { join, resolve, relative } from "node:path";
import { homedir, cpus } from "node:os";
import { spawn, spawnSync } from "node:child_process";

const ROOT = resolve(join(import.meta.dir, ".."));
const CACHE_DIR = join(ROOT, ".work", "crashes");
const WIN_SYM_CACHE = join(homedir(), ".symbols");
const MS_SYMBOL_SERVER = "https://msdl.microsoft.com/download/symbols";
const PROD_SERVER = "https://www.sumatrapdfreader.org";
const LOCAL_SERVER = "http://127.0.0.1:9321";
const SECRETS_GO = String.raw`D:\src\hack\webapps\sumatra-website\server\secrets.go`;

type DumpRow = {
  id: string;
  version: string;
  date: string;
  size: number;
  ip: string;
};

function usage(): void {
  console.log(`Usage:
  bun cmd/crashes.ts [--local]                 list; download+analyze dumps we don't have yet
  bun cmd/crashes.ts [--local] <id>            download dump, pdb, exe, run cdb (!analyze -v; ~*kb)
  bun cmd/crashes.ts -reanalyze [--local] [id] force cdb again (dump/pdb/exe stay cached)
  bun cmd/crashes.ts --server <url> ...        override server base URL
After listing, serves a local page (like sumatrapdfreader.org/crashes/) and opens the browser.`);
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
    if (a === "--server") {
      const url = argv[++i];
      if (!url) {
        throw new Error("--server needs a URL");
      }
      server = url.replace(/\/$/, "");
      continue;
    }
    if (a.startsWith("-")) {
      throw new Error(`unknown flag ${a}`);
    }
    if (id) {
      throw new Error("only one minidump id");
    }
    id = a;
  }
  return { server, id, reanalyze };
}

function parseList(text: string): DumpRow[] {
  const rows: DumpRow[] = [];
  for (const line of text.split(/\r?\n/)) {
    const s = line.trim();
    if (!s) {
      continue;
    }
    const parts = s.split(",");
    if (parts.length !== 5) {
      throw new Error(`bad minidump list line: ${s}`);
    }
    rows.push({
      id: parts[0],
      version: parts[1],
      date: parts[2],
      size: parseInt(parts[3], 10),
      ip: parts[4],
    });
  }
  rows.sort((a, b) => a.date.localeCompare(b.date));
  return rows;
}

function fmtSize(n: number): string {
  if (n < 1024) {
    return `${n} B`;
  }
  if (n < 1024 * 1024) {
    return `${(n / 1024).toFixed(1)} KB`;
  }
  return `${(n / (1024 * 1024)).toFixed(2)} MB`;
}

function dumpDir(id: string): string {
  return join(CACHE_DIR, id);
}

function dumpPath(id: string): string {
  return join(dumpDir(id), `${id}.dmp`);
}

function analyzePath(id: string): string {
  return join(dumpDir(id), "analyze.txt");
}

function logPath(id: string): string {
  return join(dumpDir(id), "log.txt");
}

function settingsPath(id: string): string {
  return join(dumpDir(id), "settings.txt");
}

function relAnalyze(id: string): string {
  return relative(ROOT, analyzePath(id)).replaceAll("\\", "/");
}

function relLog(id: string): string {
  return relative(ROOT, logPath(id)).replaceAll("\\", "/");
}

function relSettings(id: string): string {
  return relative(ROOT, settingsPath(id)).replaceAll("\\", "/");
}

function toLF(s: string): string {
  return s.replace(/\r\n/g, "\n");
}

const kSettingsMark = "--- settings ---";
const kSettingsMarkOld = "----- Settings file ----------";

function splitMinidumpComment(text: string): { log: string; settings: string } {
  let idx = -1;
  let markLen = 0;
  for (const m of [kSettingsMark, kSettingsMarkOld]) {
    const i = text.indexOf(m);
    if (i >= 0 && (idx < 0 || i < idx)) {
      idx = i;
      markLen = m.length;
    }
  }
  if (idx < 0) {
    return { log: text.replace(/\s+$/, ""), settings: "" };
  }
  return {
    log: text.slice(0, idx).replace(/\s+$/, ""),
    settings: toLF(text.slice(idx + markLen))
      .replace(/^\s+/, "")
      .replace(/\s+$/, ""),
  };
}

const kMinidumpSignature = 0x504d444d; // 'MDMP'
const kCommentStreamA = 10;
const kCommentStreamW = 11;
const kMinidumpHeaderSize = 32;
const kMinidumpDirEntrySize = 12;

function u32le(buf: Uint8Array, off: number): number {
  return (buf[off] | (buf[off + 1] << 8) | (buf[off + 2] << 16) | (buf[off + 3] << 24)) >>> 0;
}

function decodeCommentA(buf: Uint8Array): string {
  let end = buf.length;
  while (end > 0 && buf[end - 1] === 0) {
    end--;
  }
  return new TextDecoder("utf-8").decode(buf.subarray(0, end));
}

function decodeCommentW(buf: Uint8Array): string {
  let n = buf.length;
  if (n % 2) {
    n--;
  }
  while (n >= 2 && buf[n - 2] === 0 && buf[n - 1] === 0) {
    n -= 2;
  }
  return new TextDecoder("utf-16le").decode(buf.subarray(0, n));
}

// MiniDumpWriteDump CommentStreamA/W (log + settings).
function extractMinidumpComment(dmp: Uint8Array): string {
  if (dmp.length < kMinidumpHeaderSize) {
    return "";
  }
  if (u32le(dmp, 0) !== kMinidumpSignature) {
    return "";
  }
  const nStreams = u32le(dmp, 8);
  const dirRva = u32le(dmp, 12);
  if (nStreams === 0 || nStreams > 256) {
    return "";
  }
  const dirEnd = dirRva + nStreams * kMinidumpDirEntrySize;
  if (dirRva < kMinidumpHeaderSize || dirEnd > dmp.length) {
    return "";
  }
  let commentA = "";
  let commentW = "";
  for (let i = 0; i < nStreams; i++) {
    const off = dirRva + i * kMinidumpDirEntrySize;
    const type = u32le(dmp, off);
    const dataSize = u32le(dmp, off + 4);
    const rva = u32le(dmp, off + 8);
    if (dataSize === 0 || rva + dataSize > dmp.length) {
      continue;
    }
    const slice = dmp.subarray(rva, rva + dataSize);
    if (type === kCommentStreamA) {
      commentA = decodeCommentA(slice);
    } else if (type === kCommentStreamW) {
      commentW = decodeCommentW(slice);
    }
  }
  return commentA || commentW;
}

function fileNonEmpty(p: string): boolean {
  if (!existsSync(p)) {
    return false;
  }
  try {
    return statSync(p).size > 0;
  } catch {
    return false;
  }
}

function isLogExtracted(id: string): boolean {
  return fileNonEmpty(logPath(id));
}

function isSettingsExtracted(id: string): boolean {
  return fileNonEmpty(settingsPath(id));
}

function writeCommentParts(id: string, text: string): void {
  const { log, settings } = splitMinidumpComment(text);
  if (log) {
    writeFileSync(logPath(id), log);
  }
  const sp = settingsPath(id);
  if (settings) {
    writeFileSync(sp, settings);
  } else if (existsSync(sp)) {
    unlinkSync(sp);
  }
}

function extractDumpLog(id: string, force = false): void {
  if (!force && isLogExtracted(id)) {
    if (!isSettingsExtracted(id)) {
      writeCommentParts(id, readFileSync(logPath(id), "utf8"));
    }
    return;
  }
  const p = dumpPath(id);
  if (!existsSync(p)) {
    return;
  }
  try {
    const text = extractMinidumpComment(new Uint8Array(readFileSync(p)));
    if (!text) {
      return;
    }
    writeCommentParts(id, text);
  } catch (e) {
    console.error(`${id}: comment extract: ${e instanceof Error ? e.message : e}`);
  }
}

function isAnalyzed(id: string): boolean {
  const p = analyzePath(id);
  if (!existsSync(p)) {
    return false;
  }
  try {
    return statSync(p).size > 0;
  } catch {
    return false;
  }
}

function printRows(rows: DumpRow[]): void {
  if (rows.length === 0) {
    console.log("no minidumps");
    return;
  }
  const cols = {
    id: Math.max(2, ...rows.map((r) => r.id.length)),
    version: Math.max(7, ...rows.map((r) => r.version.length)),
    date: Math.max(4, ...rows.map((r) => r.date.length)),
    size: Math.max(4, ...rows.map((r) => fmtSize(r.size).length)),
    ip: Math.max(2, ...rows.map((r) => r.ip.length)),
  };
  const hdr = `${"id".padEnd(cols.id)}  ${"version".padEnd(cols.version)}  ${"date".padEnd(cols.date)}  ${"size".padStart(cols.size)}  ${"ip".padEnd(cols.ip)}`;
  console.log(hdr);
  console.log("-".repeat(hdr.length));
  for (const r of rows) {
    console.log(
      `${r.id.padEnd(cols.id)}  ${r.version.padEnd(cols.version)}  ${r.date.padEnd(cols.date)}  ${fmtSize(r.size).padStart(cols.size)}  ${r.ip.padEnd(cols.ip)}`,
    );
    if (isAnalyzed(r.id)) {
      console.log(relAnalyze(r.id));
    }
    if (isLogExtracted(r.id)) {
      console.log(relLog(r.id));
    }
    if (isSettingsExtracted(r.id)) {
      console.log(relSettings(r.id));
    }
  }
  console.log(`${rows.length} minidump${rows.length === 1 ? "" : "s"}`);
}

let cachedMinidumpPassword = "";

function loadMinidumpPassword(): string {
  if (cachedMinidumpPassword) {
    return cachedMinidumpPassword;
  }
  if (!existsSync(SECRETS_GO)) {
    throw new Error(`missing secrets file: ${SECRETS_GO}`);
  }
  const m = readFileSync(SECRETS_GO, "utf8").match(/MinidumpPassword\s*=\s*"([^"]+)"/);
  if (!m) {
    throw new Error(`MinidumpPassword not found in ${SECRETS_GO}`);
  }
  cachedMinidumpPassword = m[1];
  return cachedMinidumpPassword;
}

function dumpAuth(password: string): { Authorization: string } {
  return { Authorization: "Basic " + Buffer.from(":" + password).toString("base64") };
}

async function fetchText(url: string, headers?: HeadersInit): Promise<string> {
  const res = await fetch(url, headers ? { headers } : undefined);
  if (!res.ok) {
    throw new Error(`GET ${url} -> ${res.status}`);
  }
  return await res.text();
}

async function fetchBytes(url: string, headers?: HeadersInit): Promise<Uint8Array> {
  const res = await fetch(url, headers ? { headers } : undefined);
  if (!res.ok) {
    throw new Error(`GET ${url} -> ${res.status}`);
  }
  return new Uint8Array(await res.arrayBuffer());
}

function findCdb(): string {
  const env = process.env.PATH ?? "";
  for (const dir of env.split(";")) {
    if (!dir) {
      continue;
    }
    const p = join(dir, "cdb.exe");
    if (existsSync(p)) {
      return p;
    }
  }
  const kits = [
    String.raw`C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe`,
    String.raw`C:\Program Files\Windows Kits\10\Debuggers\x64\cdb.exe`,
  ];
  for (const p of kits) {
    if (existsSync(p)) {
      return p;
    }
  }
  const where = spawnSync("where.exe", ["cdb.exe"], { encoding: "utf8" });
  if (where.status === 0) {
    const first = where.stdout
      .split(/\r?\n/)
      .map((s) => s.trim())
      .find((s) => s.length > 0);
    if (first && existsSync(first)) {
      return first;
    }
  }
  return "";
}

function archSuffix(version: string): string {
  if (/arm64/i.test(version)) {
    return "arm64";
  }
  if (/\b32-bit\b/i.test(version)) {
    return "32";
  }
  return "64";
}

function prerelVer(version: string): string {
  const v = version.trim();
  if (/^\d+$/.test(v)) {
    return v;
  }
  const m = /^(\d+\.\d+)\.(\d+)/.exec(v);
  return m ? m[2] : "";
}

function symbolCacheKey(version: string): string {
  const ver = prerelVer(version);
  const arch = archSuffix(version);
  if (ver) {
    return arch === "64" ? ver : `${ver}-${arch}`;
  }
  return version.trim().replace(/[^\w.-]+/g, "_") || "unknown";
}

// ext is ".pdb.lzsa" or ".exe"; the arch part of the name is the same for both
function dlUrlForVersion(version: string, ext: string): string {
  const v = version.trim();
  const arch = archSuffix(v);
  const suff = arch === "64" ? `-64${ext}` : arch === "arm64" ? `-arm64${ext}` : `-32${ext}`;
  // 32-bit releases have no arch suffix
  const relSuff = arch === "32" ? ext : suff;
  const prerel = prerelVer(v);
  if (prerel) {
    return `${PROD_SERVER}/dl/prerel/${prerel}/SumatraPDF-prerel${suff}`;
  }
  const rel = /^(\d+\.\d+)/.exec(v);
  if (rel) {
    return `${PROD_SERVER}/dl/rel/${rel[1]}/SumatraPDF-${rel[1]}${relSuff}`;
  }
  return "";
}

function readU32(buf: Uint8Array, off: number): number {
  return buf[off] | (buf[off + 1] << 8) | (buf[off + 2] << 16) | (buf[off + 3] << 24);
}

function readCString(buf: Uint8Array, off: number): string {
  let end = off;
  while (end < buf.length && buf[end] !== 0) {
    end++;
  }
  return new TextDecoder("utf-8").decode(buf.subarray(off, end));
}

function x86BcjDecode(data: Uint8Array): void {
  const kMaskToAllowedStatus = [1, 1, 1, 0, 1, 0, 0, 0];
  const kMaskToBitNumber = [0, 1, 2, 2, 3, 3, 3, 3];
  const testMs = (b: number) => b === 0 || b === 0xff;
  const size = data.length;
  if (size < 5) {
    return;
  }
  let bufferPos = 0;
  let prevPosT = -1;
  let prevMask = 0;
  let ip = 5;
  for (;;) {
    let p = bufferPos;
    const limit = size - 4;
    while (p < limit && (data[p] & 0xfe) !== 0xe8) {
      p++;
    }
    bufferPos = p;
    if (p >= limit) {
      break;
    }
    prevPosT = bufferPos - prevPosT;
    if (prevPosT > 3) {
      prevMask = 0;
    } else {
      prevMask = (prevMask << (prevPosT - 1)) & 0x7;
      if (prevMask !== 0) {
        const b = data[p + 4 - kMaskToBitNumber[prevMask]];
        if (!kMaskToAllowedStatus[prevMask] || testMs(b)) {
          prevPosT = bufferPos;
          prevMask = ((prevMask << 1) & 0x7) | 1;
          bufferPos++;
          continue;
        }
      }
    }
    prevPosT = bufferPos;
    if (testMs(data[p + 4])) {
      let src = (data[p + 4] << 24) | (data[p + 3] << 16) | (data[p + 2] << 8) | data[p + 1];
      src = src >>> 0;
      let dest = 0;
      for (;;) {
        dest = (src - (ip + bufferPos)) >>> 0;
        if (prevMask === 0) {
          break;
        }
        const index = kMaskToBitNumber[prevMask] * 8;
        const b = (dest >>> (24 - index)) & 0xff;
        if (!testMs(b)) {
          break;
        }
        src = (dest ^ ((1 << (32 - index)) - 1)) >>> 0;
      }
      data[p + 4] = ~((((dest >>> 24) & 1) - 1) >>> 0) & 0xff;
      data[p + 3] = (dest >>> 16) & 0xff;
      data[p + 2] = (dest >>> 8) & 0xff;
      data[p + 1] = dest & 0xff;
      bufferPos += 5;
    } else {
      prevMask = ((prevMask << 1) & 0x7) | 1;
      bufferPos++;
    }
  }
}

function pythonLzma(): string[] | null {
  for (const cmd of [["py", "-3"], ["python"], ["python3"]]) {
    const r = spawnSync(cmd[0], [...cmd.slice(1), "-c", "import lzma"], { encoding: "utf8" });
    if (r.status === 0) {
      return cmd;
    }
  }
  return null;
}

function lzmaDecompress(propsAndPayload: Uint8Array, unpackedSize: number): Uint8Array {
  const py = pythonLzma();
  if (!py) {
    throw new Error("python with lzma is required to unpack .pdb.lzsa");
  }
  const script = `
import lzma, sys
n = int(sys.argv[1])
d = sys.stdin.buffer.read()
# LzSA stores no unpacked size in the LZMA header; -1 is FORMAT_ALONE "unknown"
header = d[:5] + (0xFFFFFFFFFFFFFFFF).to_bytes(8, "little")
out = lzma.decompress(header + d[5:], format=lzma.FORMAT_ALONE)
if len(out) != n:
    raise SystemExit(f"lzma size {len(out)} want {n}")
sys.stdout.buffer.write(out)
`;
  const r = spawnSync(py[0], [...py.slice(1), "-c", script, String(unpackedSize)], {
    input: Buffer.from(propsAndPayload),
    encoding: "buffer",
    maxBuffer: unpackedSize + 16 * 1024 * 1024,
  });
  if (r.status !== 0) {
    throw new Error(`lzma decompress failed: ${r.stderr?.toString() || r.status}`);
  }
  return new Uint8Array(r.stdout);
}

function extractLzsaPdb(archive: Uint8Array, destDir: string): void {
  if (archive.length < 8) {
    throw new Error("lzsa too small");
  }
  const magic = readU32(archive, 0);
  if (magic !== 0x41537a4c) {
    throw new Error("not an LzSA archive");
  }
  const nFiles = readU32(archive, 4);
  type FileEnt = { name: string; compressedSize: number; uncompressedSize: number; dataOff: number };
  const files: FileEnt[] = [];
  let off = 8;
  for (let i = 0; i < nFiles; i++) {
    const hdrSize = readU32(archive, off);
    const compressedSize = readU32(archive, off + 4);
    const uncompressedSize = readU32(archive, off + 8);
    const name = readCString(archive, off + 24);
    files.push({ name, compressedSize, uncompressedSize, dataOff: 0 });
    off += hdrSize;
  }
  off += 4; // header crc
  for (const f of files) {
    f.dataOff = off;
    off += f.compressedSize;
  }
  mkdirSync(destDir, { recursive: true });
  for (const f of files) {
    const chunk = archive.subarray(f.dataOff, f.dataOff + f.compressedSize);
    if (chunk.length < 1) {
      throw new Error(`empty lzsa file ${f.name}`);
    }
    let raw: Uint8Array;
    const filter = chunk[0];
    if (filter === 0xff) {
      raw = chunk.subarray(1);
    } else {
      raw = lzmaDecompress(chunk.subarray(1), f.uncompressedSize);
      if (filter === 1) {
        x86BcjDecode(raw);
      }
    }
    writeFileSync(join(destDir, f.name), raw);
  }
}

function hasSumatraPdbs(dir: string): boolean {
  return existsSync(join(dir, "SumatraPDF.pdb")) && existsSync(join(dir, "libsumatrapdf.pdb"));
}

function localDbgSymDir(version: string): string {
  if (!/\(dbg\)/i.test(version)) {
    return "";
  }
  for (const d of [join(ROOT, "out", "dbg64"), join(ROOT, "out", "dbg64_asan")]) {
    if (hasSumatraPdbs(d)) {
      return d;
    }
  }
  return "";
}

const inFlightSymbols = new Map<string, Promise<string>>();

async function ensureSymbols(row: DumpRow): Promise<string> {
  const local = localDbgSymDir(row.version);
  if (local) {
    return local;
  }
  const key = symbolCacheKey(row.version);
  const dir = join(CACHE_DIR, "symbols", key);
  if (hasSumatraPdbs(dir)) {
    return dir;
  }
  let p = inFlightSymbols.get(key);
  if (p) {
    return await p;
  }
  p = (async () => {
    const url = dlUrlForVersion(row.version, ".pdb.lzsa");
    if (!url) {
      throw new Error(`no pdb source for version '${row.version}'`);
    }
    mkdirSync(dir, { recursive: true });
    const lzsaPath = join(dir, "pdb.lzsa");
    if (!existsSync(lzsaPath) || statSync(lzsaPath).size === 0) {
      console.log(`pdb: downloading ${url}`);
      writeFileSync(lzsaPath, await fetchBytes(url));
    }
    extractLzsaPdb(readFileSync(lzsaPath), dir);
    if (!hasSumatraPdbs(dir)) {
      throw new Error(`pdb lzsa missing SumatraPDF.pdb or libsumatrapdf.pdb (${url})`);
    }
    return dir;
  })();
  inFlightSymbols.set(key, p);
  try {
    return await p;
  } finally {
    inFlightSymbols.delete(key);
  }
}

const inFlightExe = new Map<string, Promise<string>>();

// cdb needs SumatraPDF.exe to map the image (the dump has no code pages), otherwise
// it prints "Unable to load image ... Win32 error 0n2" and can't disassemble
async function ensureExe(row: DumpRow): Promise<string> {
  const local = localDbgSymDir(row.version);
  if (local && existsSync(join(local, "SumatraPDF.exe"))) {
    return local;
  }
  const key = symbolCacheKey(row.version);
  const dir = join(CACHE_DIR, "symbols", key);
  const exePath = join(dir, "SumatraPDF.exe");
  if (existsSync(exePath) && statSync(exePath).size > 0) {
    return dir;
  }
  let p = inFlightExe.get(key);
  if (p) {
    return await p;
  }
  p = (async () => {
    const url = dlUrlForVersion(row.version, ".exe");
    if (!url) {
      throw new Error(`no exe source for version '${row.version}'`);
    }
    mkdirSync(dir, { recursive: true });
    console.log(`exe: downloading ${url}`);
    writeFileSync(exePath, await fetchBytes(url));
    return dir;
  })();
  inFlightExe.set(key, p);
  try {
    return await p;
  } finally {
    inFlightExe.delete(key);
  }
}

// missing exe only degrades the analysis, so never fail on it
async function ensureExeQuiet(row: DumpRow): Promise<string> {
  try {
    return await ensureExe(row);
  } catch (e) {
    console.log(`exe: ${e instanceof Error ? e.message : e}`);
    return "";
  }
}

// the dump identifies the image by the path it ran from, so a renamed exe
// ("SumatraPDF-prerel-64 (3).exe") doesn't match our cached SumatraPDF.exe and
// cdb can't unwind past inlined frames. Give it a dir with a copy under that name.
function crashedExeName(id: string): string {
  if (!isLogExtracted(id)) {
    return "";
  }
  const m = /^Exe:[ \t]*(.*\.exe)[ \t]/im.exec(readFileSync(logPath(id), "utf8"));
  if (!m) {
    return "";
  }
  const name = m[1].split(/[\\/]/).pop() ?? "";
  return name === "SumatraPDF.exe" ? "" : name;
}

function renamedExeDir(id: string, exeDir: string): string {
  const name = exeDir ? crashedExeName(id) : "";
  if (!name) {
    return "";
  }
  const src = join(exeDir, "SumatraPDF.exe");
  if (!existsSync(src)) {
    return "";
  }
  const dir = join(dumpDir(id), "img");
  const dst = join(dir, name);
  if (!existsSync(dst) || statSync(dst).size !== statSync(src).size) {
    mkdirSync(dir, { recursive: true });
    copyFileSync(src, dst);
  }
  return dir;
}

async function downloadDumpIfMissing(server: string, id: string): Promise<void> {
  mkdirSync(dumpDir(id), { recursive: true });
  const dmpPath = dumpPath(id);
  if (existsSync(dmpPath)) {
    return;
  }
  const url = `${server}/minidump/${id}`;
  console.log(`dump: downloading ${url}`);
  writeFileSync(dmpPath, await fetchBytes(url, dumpAuth(loadMinidumpPassword())));
}

async function ensureDownloaded(server: string, row: DumpRow, reanalyze: boolean): Promise<void> {
  await downloadDumpIfMissing(server, row.id);
  extractDumpLog(row.id, reanalyze);
  await ensureSymbols(row);
  await ensureExeQuiet(row);
}

const MARK_CRASHED = "---CRASHED-STACK---";
const MARK_ANALYZE = "---ANALYZE---";
const MARK_THREADS = "---THREADS---";

function markerIndex(text: string, marker: string): number {
  const re = new RegExp(`^${marker.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}\\s*$`, "m");
  const m = re.exec(text);
  return m ? m.index : -1;
}

function sectionBetween(text: string, start: string, end: string | null): string {
  const i = markerIndex(text, start);
  if (i < 0) {
    return "";
  }
  const from = i + start.length;
  const j = end ? markerIndex(text.slice(from), end) : -1;
  const to = j < 0 ? text.length : from + j;
  return text.slice(from, to).replace(/^\r?\n/, "");
}

function extractStackText(raw: string): string {
  const lines = raw.split(/\r?\n/);
  const start = lines.findIndex((l) => l.startsWith("STACK_TEXT:"));
  if (start < 0) {
    return "";
  }
  let end = start + 1;
  while (end < lines.length) {
    const l = lines[end];
    if (/^[A-Z][A-Z0-9_ ]+:/.test(l) && !l.startsWith("STACK_TEXT:")) {
      break;
    }
    end++;
  }
  return lines.slice(start, end).join("\n").trim();
}

function trimQuit(s: string): string {
  const i = s.search(/^quit:\s*$/m);
  return i < 0 ? s.trim() : s.slice(0, i).trim();
}

function rewriteAnalyzeLog(raw: string): string {
  const crashed = trimQuit(sectionBetween(raw, MARK_CRASHED, MARK_ANALYZE)) || extractStackText(raw);
  const analyze = trimQuit(sectionBetween(raw, MARK_ANALYZE, MARK_THREADS)) || raw.trim();
  const threads = trimQuit(sectionBetween(raw, MARK_THREADS, null));
  const parts: string[] = [];
  if (crashed) {
    parts.push("=== crashed thread ===", crashed, "");
  }
  if (threads) {
    parts.push("=== all threads (~*kb) ===", threads, "");
  }
  parts.push("=== !analyze -v ===", analyze, "");
  return parts.join("\n");
}

const CDB_CMD = `.echo ${MARK_CRASHED}; .ecxr; kb; .echo ${MARK_ANALYZE}; !analyze -v; .echo ${MARK_THREADS}; ~*kb; qq`;

function runCdbAsync(cdb: string, args: string[], outPath: string): Promise<void> {
  return new Promise<void>((resolve, reject) => {
    let stdout = "";
    let stderr = "";
    const proc = spawn(cdb, args, { stdio: ["ignore", "pipe", "pipe"] });
    proc.stdout.on("data", (d) => {
      stdout += d;
    });
    proc.stderr.on("data", (d) => {
      stderr += d;
    });
    proc.on("error", (err) => {
      reject(err);
    });
    proc.on("close", () => {
      if (!existsSync(outPath) || statSync(outPath).size === 0) {
        writeFileSync(outPath, `${stdout}\n${stderr}`);
      }
      if (existsSync(outPath)) {
        writeFileSync(outPath, rewriteAnalyzeLog(readFileSync(outPath, "utf8")));
      }
      resolve();
    });
  });
}

async function runAnalysis(row: DumpRow, reanalyze: boolean): Promise<void> {
  if (!reanalyze && isAnalyzed(row.id)) {
    return;
  }
  const dmpPath = dumpPath(row.id);
  const outPath = analyzePath(row.id);
  if (reanalyze && existsSync(outPath)) {
    unlinkSync(outPath);
  }
  const symDir = await ensureSymbols(row);
  const exeDir = await ensureExeQuiet(row);
  const cdb = findCdb();
  if (!cdb) {
    console.log(`dump: ${dmpPath}`);
    console.log(`pdb:  ${symDir}`);
    console.log("cdb.exe not found; install Windows Debugging Tools to run !analyze");
    return;
  }
  mkdirSync(WIN_SYM_CACHE, { recursive: true });
  const nt = process.env._NT_SYMBOL_PATH?.trim();
  const symParts = [symDir, `srv*${WIN_SYM_CACHE}*${MS_SYMBOL_SERVER}`];
  if (nt) {
    symParts.push(nt);
  }
  const symPath = symParts.join(";");
  console.log(`cdb: ${cdb} (${row.id})`);
  console.log(`pdb: ${relative(ROOT, symDir).replaceAll("\\", "/")}`);
  const args = ["-z", dmpPath, "-y", symPath, "-lines"];
  const imgDirs = [exeDir, renamedExeDir(row.id, exeDir)].filter((d) => d !== "");
  if (imgDirs.length > 0) {
    args.push("-i", imgDirs.join(";"));
  }
  args.push("-logo", outPath, "-c", CDB_CMD);
  await runCdbAsync(cdb, args, outPath);
}

async function analyze(server: string, row: DumpRow, reanalyze: boolean): Promise<void> {
  if (!reanalyze && isAnalyzed(row.id)) {
    return;
  }
  await ensureDownloaded(server, row, reanalyze);
  await runAnalysis(row, reanalyze);
}

async function ensureAnalyzed(server: string, row: DumpRow, reanalyze: boolean): Promise<void> {
  if (!reanalyze && isAnalyzed(row.id)) {
    extractDumpLog(row.id);
    return;
  }
  await analyze(server, row, reanalyze);
}

async function mapConcurrent<T>(items: T[], limit: number, fn: (item: T) => Promise<void>): Promise<void> {
  let idx = 0;
  const workers = Array.from({ length: Math.min(limit, items.length) }, async () => {
    while (idx < items.length) {
      const cur = items[idx++];
      await fn(cur);
    }
  });
  await Promise.all(workers);
}

type ApiCrash = {
  Day: string;
  FileNameTxt: string;
  IP: string;
  Ver: string;
  CrashLine: string;
  SrcLoc: string;
  GitSha1: string;
  IsCrash: boolean;
};

function parseAnalyzeSummary(txt: string): { crashLine: string; srcLoc: string; isCrash: boolean } {
  const isCrash = !/Type:\s*hang/i.test(txt);
  let body = txt;
  const crashed = txt.indexOf("=== crashed thread ===");
  if (crashed >= 0) {
    const rest = txt.slice(crashed);
    const next = rest.search(/\n=== /);
    body = next >= 0 ? rest.slice(0, next) : rest;
  }
  const siteRe = / : ([A-Za-z0-9_.]+![^\s\[]+)/;
  const srcRe = /\[([^\]]+?) @ (\d+)\]/;
  let crashLine = "";
  let srcLoc = "";
  for (const line of body.split(/\r?\n/)) {
    const sm = siteRe.exec(line);
    if (!sm) {
      continue;
    }
    crashLine = sm[1];
    const src = srcRe.exec(line);
    if (src) {
      srcLoc = `${src[1]}:${src[2]}`;
    }
    break;
  }
  return { crashLine, srcLoc, isCrash };
}

function crashApiRow(row: DumpRow): ApiCrash {
  const txt = isAnalyzed(row.id) ? readFileSync(analyzePath(row.id), "utf8") : "";
  const { crashLine, srcLoc, isCrash } = parseAnalyzeSummary(txt);
  return {
    Day: row.date.slice(0, 10),
    FileNameTxt: row.id,
    IP: row.ip,
    Ver: row.version,
    CrashLine: crashLine,
    SrcLoc: srcLoc,
    GitSha1: "",
    IsCrash: isCrash,
  };
}

function readLog(id: string): string {
  return isLogExtracted(id) ? readFileSync(logPath(id), "utf8") : "";
}

function readSettings(id: string): string {
  return isSettingsExtracted(id) ? toLF(readFileSync(settingsPath(id), "utf8")) : "";
}

// analyze.txt with the minidump log and settings appended
function crashText(id: string, analyzeTxt: string): string {
  const parts = [analyzeTxt.replace(/\s+$/, "")];
  const log = readLog(id);
  if (log) {
    parts.push("=== minidump log ===", log.replace(/\s+$/, ""));
  }
  const settings = readSettings(id);
  if (settings) {
    parts.push("=== settings ===", settings.replace(/\s+$/, ""));
  }
  return `${parts.join("\n\n")}\n`;
}

function escapeHtml(s: string): string {
  return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

function crashHtml(id: string, analyzeTxt: string): string {
  const enc = encodeURIComponent(id);
  const logTxt = readLog(id);
  const settingsTxt = readSettings(id);
  const logBlock = logTxt ? `<h2>minidump log</h2>\n<pre>${escapeHtml(logTxt)}</pre>` : "";
  const settingsBlock = settingsTxt ? `<h2>settings</h2>\n<pre>${escapeHtml(settingsTxt)}</pre>` : "";
  return `<!doctype html>
<meta charset="utf-8">
<title>${escapeHtml(id)}</title>
<style>
  body { font-family: monospace; font-size: 9pt; margin: 1em; }
  h2 { margin: 1.2em 0 0.4em; font-size: 11pt; }
  pre { white-space: pre-wrap; word-break: break-word; }
  nav { margin-bottom: 1em; }
  nav a { margin-right: 1em; }
</style>
<nav>
  <a href="/">index</a>
  <a href="/crash/${enc}">analyze.txt</a>
  ${logTxt ? `<a href="/crash/${enc}.log">log.txt</a>` : ""}
  ${settingsTxt ? `<a href="/crash/${enc}.settings">settings.txt</a>` : ""}
</nav>
<h2>cdb !analyze</h2>
<pre>${escapeHtml(analyzeTxt)}</pre>
${logBlock}
${settingsBlock}
`;
}

function crashesIndexHtml(): string {
  return `<!doctype html>
<html>
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Sumatra PDF crashes</title>
  <script>
    const log = console.log;
    document.addEventListener("alpine:init", initAlpine);
    function len(o) { return o ? o.length : 0; }
    function shortVer(v) {
      v = v.replace(" pre-release", "");
      v = v.replace(" 64-bit", "");
      v = v.replace(" 32-bit", "");
      v = v.replace(" Wow64", " ");
      v = v.replace("  ", " ");
      return v.trim();
    }
    function cmpCrash(a, b) {
      if (a.IsCrash !== b.IsCrash) return a.IsCrash ? -1 : 1;
      if (a.ShortVer != b.ShortVer) return a.ShortVer < b.ShortVer ? 1 : -1;
      return 0;
    }
    let crashesPerDay = {};
    function setCurrentDay(currDay) {
      Alpine.store("crashes").currDay = currDay;
      let currDayCrashes = crashesPerDay[currDay] || [];
      currDayCrashes.sort(cmpCrash);
      Alpine.store("crashes").currDayCrashes = currDayCrashes;
    }
    async function initAlpine() {
      Alpine.store("crashes", { perDay: {}, days: [], currDay: "", currDayCrashes: [] });
      const crashes = await (await fetch("/api/crashes")).json();
      let currDay = "";
      let days = new Set();
      for (const crash of crashes) {
        crash.ShortVer = shortVer(crash.Ver);
        let day = crash.Day;
        if (currDay == "" || day > currDay) currDay = day;
        if (!crashesPerDay[day]) crashesPerDay[day] = [];
        crashesPerDay[day].push(crash);
        days.add(day);
      }
      Alpine.store("crashes").perDay = crashesPerDay;
      Alpine.store("crashes").days = Array.from(days).sort().reverse();
      setCurrentDay(currDay);
    }
    function crashesPerDayTxt(day) {
      let n = crashesPerDay[day] ? crashesPerDay[day].length : 0;
      return "[" + n + "]";
    }
    function textURL(crash) { return "/crash/" + crash.FileNameTxt; }
  </script>
  <script src="https://unpkg.com/alpinejs" defer></script>
  <style>
    html, body { font-family: monospace; font-size: 9pt; margin: 0; padding: 0; }
    body { display: flex; flex-direction: column; }
    .self-center { align-self: center; }
    .flex { display: flex; }
    .w-full { width: 100%; }
    .bold { font-weight: bold; }
    .gap-4 { gap: 1rem; }
    .mt-2 { margin-top: 0.5rem; }
    td { padding-left: 1rem; }
  </style>
</head>
<body>
  <div class="self-center mt-2">SumatraPDF Crashes</div>
  <div x-data="{currDay: $store.crashes.currDay}" class="flex self-center gap-4 mt-2"
    x-init="$watch('$store.crashes.currDay', value => { currDay = value; })">
    <template x-for="day in $store.crashes.days">
      <div class="flex">
        <template x-if="day === currDay">
          <div class="bold"><span x-text="day"></span>&nbsp;<span x-text="crashesPerDayTxt(day)"></span></div>
        </template>
        <template x-if="day !== currDay">
          <a href="#" @click="setCurrentDay(day)"><span x-text="day"></span>&nbsp;<span x-text="crashesPerDayTxt(day)"></span></a>
        </template>
      </div>
    </template>
  </div>
  <div x-data class="mt-2">
    <table>
      <tbody>
        <template x-for="crash in $store.crashes.currDayCrashes">
          <tr>
            <td><a :href="textURL(crash)" target="_blank">text</a></td>
            <td>
              <template x-if="crash.IsCrash">
                <div style="color: red; font-weight: bold;" x-text="shortVer(crash.ShortVer)"></div>
              </template>
              <template x-if="!crash.IsCrash">
                <div x-text="shortVer(crash.ShortVer)"></div>
              </template>
            </td>
            <td><div x-text="crash.IP"></div></td>
            <td><div x-text="crash.CrashLine"></div></td>
            <td><div x-text="crash.SrcLoc"></div></td>
          </tr>
        </template>
      </tbody>
    </table>
  </div>
  <hr class="w-full" />
</body>
</html>
`;
}

function handleCrashHttp(req: Request, rows: DumpRow[]): Response {
  const u = new URL(req.url);
  let p = u.pathname;
  if (p.length > 1 && p.endsWith("/")) {
    p = p.slice(0, -1);
  }
  if (p === "" || p === "/" || p === "/crashes") {
    return new Response(crashesIndexHtml(), { headers: { "content-type": "text/html; charset=utf-8" } });
  }
  if (p === "/api/crashes") {
    return Response.json(rows.map(crashApiRow));
  }
  const m = /^\/crash\/([^/]+?)(\.html|\.log|\.settings)?$/.exec(p);
  if (m) {
    const id = decodeURIComponent(m[1]);
    const ext = m[2] || "";
    if (!rows.some((r) => r.id === id)) {
      return new Response("not found", { status: 404 });
    }
    if (ext === ".log") {
      if (!isLogExtracted(id)) {
        return new Response("not found", { status: 404 });
      }
      return new Response(readFileSync(logPath(id), "utf8"), {
        headers: { "content-type": "text/plain; charset=utf-8" },
      });
    }
    if (ext === ".settings") {
      if (!isSettingsExtracted(id)) {
        return new Response("not found", { status: 404 });
      }
      return new Response(readSettings(id), {
        headers: { "content-type": "text/plain; charset=utf-8" },
      });
    }
    if (!isAnalyzed(id)) {
      return new Response("not found", { status: 404 });
    }
    const body = readFileSync(analyzePath(id), "utf8");
    if (ext === ".html") {
      return new Response(crashHtml(id, body), { headers: { "content-type": "text/html; charset=utf-8" } });
    }
    return new Response(crashText(id, body), { headers: { "content-type": "text/plain; charset=utf-8" } });
  }
  return new Response("not found", { status: 404 });
}

function openBrowser(url: string): void {
  spawn("cmd.exe", ["/c", "start", "", url], { detached: true, stdio: "ignore", windowsHide: true }).unref();
}

const UI_PORT = 7345;

async function serveCrashes(rows: DumpRow[]): Promise<void> {
  const server = Bun.serve({
    port: UI_PORT,
    hostname: "127.0.0.1",
    fetch(req) {
      return handleCrashHttp(req, rows);
    },
  });
  const url = `http://127.0.0.1:${server.port}/`;
  console.log(`serving ${url}  (Ctrl+C to stop)`);
  openBrowser(url);
  await new Promise<void>((resolve) => {
    const stop = () => {
      try {
        server.stop(true);
      } catch {
        // already stopped
      }
      resolve();
    };
    process.on("SIGINT", stop);
    process.on("SIGTERM", stop);
  });
}

async function main(): Promise<void> {
  const { server, id, reanalyze } = parseArgs(process.argv.slice(2));
  const password = loadMinidumpPassword();
  const list = parseList(await fetchText(`${server}/minidumps.txt`, dumpAuth(password)));
  if (id) {
    const row = list.find((r) => r.id === id);
    if (!row) {
      throw new Error(`minidump '${id}' not in ${server}/minidumps.txt`);
    }
    await ensureAnalyzed(server, row, reanalyze);
    console.log(relAnalyze(row.id));
    if (isLogExtracted(row.id)) {
      console.log(relLog(row.id));
    }
    if (isSettingsExtracted(row.id)) {
      console.log(relSettings(row.id));
    }
  } else {
    await mapConcurrent(list, 4, async (row) => {
      try {
        await ensureDownloaded(server, row, reanalyze);
      } catch (e) {
        console.error(`${row.id}: download: ${e instanceof Error ? e.message : e}`);
      }
    });
    const cdbWorkers = Math.max(1, cpus().length - 1);
    await mapConcurrent(list, cdbWorkers, async (row) => {
      try {
        await runAnalysis(row, reanalyze);
      } catch (e) {
        console.error(`${row.id}: analyze: ${e instanceof Error ? e.message : e}`);
      }
    });
    printRows(list);
  }
  await serveCrashes(list);
}

export type { DumpRow };
export {
  CACHE_DIR,
  PROD_SERVER,
  LOCAL_SERVER,
  dumpDir,
  dumpPath,
  analyzePath,
  logPath,
  settingsPath,
  relAnalyze,
  relLog,
  relSettings,
  extractDumpLog,
  isAnalyzed,
  isLogExtracted,
  isSettingsExtracted,
  downloadDumpIfMissing,
  ensureSymbols,
  runAnalysis,
  parseAnalyzeSummary,
};

if (import.meta.main) {
  try {
    await main();
  } catch (e) {
    console.error(e instanceof Error ? e.message : e);
    process.exit(1);
  }
}
