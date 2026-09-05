// List minidumps from the crash server, or download one and run cdb !analyze.
//
//   bun cmd/crashes.ts              list (oldest first, newest last)
//   bun cmd/crashes.ts --local      same, against http://127.0.0.1:9321
//   bun cmd/crashes.ts <id>         download dump + pdb, run !analyze
import { existsSync, mkdirSync, copyFileSync, writeFileSync, readFileSync } from "node:fs";
import { join, dirname, resolve } from "node:path";
import { spawnSync } from "node:child_process";

const ROOT = resolve(join(import.meta.dir, ".."));
const CACHE_DIR = join(ROOT, ".work", "crashes");
const PROD_SERVER = "https://www.sumatrapdfreader.org";
const LOCAL_SERVER = "http://127.0.0.1:9321";

type DumpRow = {
  id: string;
  version: string;
  date: string;
  size: number;
  ip: string;
};

function usage(): void {
  console.log(`Usage:
  bun cmd/crashes.ts [--local]           list minidumps (newest last)
  bun cmd/crashes.ts [--local] <id>      download dump, pdb, run cdb !analyze
  bun cmd/crashes.ts --server <url> ...  override server base URL`);
}

function parseArgs(argv: string[]): { server: string; id: string } {
  let server = PROD_SERVER;
  let id = "";
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
  return { server, id };
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
  }
  console.log(`${rows.length} minidump${rows.length === 1 ? "" : "s"}`);
}

async function fetchText(url: string): Promise<string> {
  const res = await fetch(url);
  if (!res.ok) {
    throw new Error(`GET ${url} -> ${res.status}`);
  }
  return await res.text();
}

async function fetchBytes(url: string): Promise<Uint8Array> {
  const res = await fetch(url);
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

function localPdbCandidates(version: string): string[] {
  const out = join(ROOT, "out");
  const names = ["SumatraPDF.pdb", "SumatraPDF-static.pdb"];
  const dirs: string[] = [];
  if (/\(dbg\)/i.test(version)) {
    dirs.push(join(out, "dbg64"), join(out, "dbg64_asan"));
  }
  dirs.push(join(out, "rel64"), join(out, "rel64_static"), join(out, "dbg64"));
  const paths: string[] = [];
  for (const d of dirs) {
    for (const n of names) {
      paths.push(join(d, n));
    }
  }
  return paths;
}

function pdbUrlForVersion(version: string): string {
  const v = version.trim();
  const arm = /arm64/i.test(v);
  const x64 = !arm && !/\b32-bit\b/i.test(v);
  const suff = arm ? "-arm64.pdb.lzsa" : x64 ? "-64.pdb.lzsa" : "-32.pdb.lzsa";
  const relSuff = arm ? "-arm64.pdb.lzsa" : x64 ? "-64.pdb.lzsa" : ".pdb.lzsa";
  // update-check `v` for pre-release is just PRE_RELEASE_VER (e.g. 16105)
  if (/^\d+$/.test(v)) {
    return `${PROD_SERVER}/dl/prerel/${v}/SumatraPDF-prerel${suff}`;
  }
  const prerel = /^(\d+\.\d+)\.(\d+)/.exec(v);
  if (prerel) {
    return `${PROD_SERVER}/dl/prerel/${prerel[2]}/SumatraPDF-prerel${suff}`;
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
header = d[:5] + n.to_bytes(8, "little")
sys.stdout.buffer.write(lzma.decompress(header + d[5:], format=lzma.FORMAT_ALONE))
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

function extractLzsaPdb(archive: Uint8Array, destDir: string): string {
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
  let pdbPath = "";
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
    const outPath = join(destDir, f.name);
    writeFileSync(outPath, raw);
    if (f.name.toLowerCase().endsWith(".pdb") && (f.name.toLowerCase() === "sumatrapdf.pdb" || !pdbPath)) {
      pdbPath = outPath;
    }
  }
  if (!pdbPath) {
    throw new Error("lzsa had no .pdb");
  }
  return pdbPath;
}

async function ensurePdb(row: DumpRow, destDir: string): Promise<string> {
  const cached = join(destDir, "SumatraPDF.pdb");
  if (existsSync(cached)) {
    return cached;
  }
  for (const p of localPdbCandidates(row.version)) {
    if (existsSync(p)) {
      copyFileSync(p, cached);
      console.log(`pdb: copied ${p}`);
      return cached;
    }
  }
  const url = pdbUrlForVersion(row.version);
  if (!url) {
    throw new Error(`no pdb source for version '${row.version}'`);
  }
  console.log(`pdb: downloading ${url}`);
  const lzsaPath = join(destDir, "pdb.lzsa");
  writeFileSync(lzsaPath, await fetchBytes(url));
  const extracted = extractLzsaPdb(readFileSync(lzsaPath), destDir);
  if (extracted !== cached && existsSync(extracted)) {
    copyFileSync(extracted, cached);
  }
  return cached;
}

async function analyze(server: string, row: DumpRow): Promise<void> {
  const dir = join(CACHE_DIR, row.id);
  mkdirSync(dir, { recursive: true });
  const dmpPath = join(dir, `${row.id}.dmp`);
  if (!existsSync(dmpPath)) {
    const url = `${server}/minidump/${row.id}`;
    console.log(`dump: downloading ${url}`);
    writeFileSync(dmpPath, await fetchBytes(url));
  }
  const pdb = await ensurePdb(row, dir);
  const cdb = findCdb();
  const analyzePath = join(dir, "analyze.txt");
  if (!cdb) {
    console.log(`dump: ${dmpPath}`);
    console.log(`pdb:  ${pdb}`);
    console.log("cdb.exe not found; install Windows Debugging Tools to run !analyze");
    return;
  }
  const nt = process.env._NT_SYMBOL_PATH?.trim();
  const symParts = [`cache*${join(CACHE_DIR, "sym")}`, dirname(pdb)];
  if (nt) {
    symParts.push(nt);
  }
  const symPath = symParts.join(";");
  console.log(`cdb: ${cdb}`);
  const r = spawnSync(cdb, ["-z", dmpPath, "-y", symPath, "-lines", "-logo", analyzePath, "-c", "!analyze -v; qq"], {
    encoding: "utf8",
    timeout: 180_000,
  });
  if (r.status !== 0 && !existsSync(analyzePath)) {
    writeFileSync(analyzePath, `${r.stdout || ""}\n${r.stderr || ""}`);
  }
  console.log(`dump:    ${dmpPath}`);
  console.log(`pdb:     ${pdb}`);
  console.log(`analyze: ${analyzePath}`);
}

async function main(): Promise<void> {
  const { server, id } = parseArgs(process.argv.slice(2));
  const list = parseList(await fetchText(`${server}/minidumps.txt`));
  if (!id) {
    printRows(list);
    return;
  }
  const row = list.find((r) => r.id === id);
  if (!row) {
    throw new Error(`minidump '${id}' not in ${server}/minidumps.txt`);
  }
  await analyze(server, row);
}

if (import.meta.main) {
  try {
    await main();
  } catch (e) {
    console.error(e instanceof Error ? e.message : e);
    process.exit(1);
  }
}
