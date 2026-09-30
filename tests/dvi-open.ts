// Open a DVI file: convert once to a cached PDF, reuse that PDF while the
// DVI is unchanged, convert again after its modification time changes.
//
// Needs dvipdfmx (ships with pdflatex / xelatex / lualatex) or dvips plus
// Ghostscript. Skips when neither is installed.
//
// Run: bun tests/dvi-open.ts [--no-build]

import { existsSync, mkdirSync, readFileSync, rmSync, statSync, unlinkSync, utimesSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { findGhostscript, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

function whereExe(name: string): string {
  const r = Bun.spawnSync(["where.exe", name], { stdout: "pipe", stderr: "ignore" });
  if (r.exitCode !== 0) {
    return "";
  }
  const line = r.stdout
    .toString()
    .split(/\r?\n/)
    .map((s) => s.trim())
    .find((s) => s.length > 0);
  return line ?? "";
}

function exeBeside(bin: string, name: string): string {
  const dir = dirname(bin);
  const path = join(dir, name);
  return existsSync(path) ? path : "";
}

// Same tools EngineDvi looks for: dvipdfmx / xdvipdfmx on PATH or beside a
// TeX engine, otherwise dvips plus Ghostscript.
function dviConverterAvailable(): boolean {
  if (whereExe("dvipdfmx.exe") || whereExe("xdvipdfmx.exe")) {
    return true;
  }
  const bins = ["pdflatex.exe", "xelatex.exe", "lualatex.exe", "latex.exe"];
  for (const binName of bins) {
    const bin = whereExe(binName);
    if (!bin) {
      continue;
    }
    if (exeBeside(bin, "dvipdfmx.exe") || exeBeside(bin, "xdvipdfmx.exe")) {
      return true;
    }
  }
  const dvips = whereExe("dvips.exe");
  const gs = whereExe("gswin64c.exe") || whereExe("gswin32c.exe") || findGhostscript();
  if (!dvips) {
    for (const binName of bins) {
      const bin = whereExe(binName);
      if (bin && exeBeside(bin, "dvips.exe") && gs) {
        return true;
      }
    }
  }
  return !!(dvips && gs);
}

function u32be(n: number): Buffer {
  const b = Buffer.alloc(4);
  b.writeInt32BE(n, 0);
  return b;
}

function u16be(n: number): Buffer {
  const b = Buffer.alloc(2);
  b.writeUInt16BE(n, 0);
  return b;
}

// One empty page. No font is used, so dvipdfmx / dvips do not need a TFM.
function makeBlankDvi(): Buffer {
  const num = 25400000;
  const den = 473628672;
  const mag = 1000;
  const height = 43725786;
  const width = 30785863;
  const pre = Buffer.concat([Buffer.from([247, 2]), u32be(num), u32be(den), u32be(mag), Buffer.from([0])]);
  const counters = Buffer.alloc(40);
  counters.writeInt32BE(1, 0);
  const bop = Buffer.concat([Buffer.from([139]), counters, u32be(-1)]);
  const eop = Buffer.from([140]);
  const bopAt = pre.length;
  const postAt = bopAt + bop.length + eop.length;
  const post = Buffer.concat([
    Buffer.from([248]),
    u32be(bopAt),
    u32be(num),
    u32be(den),
    u32be(mag),
    u32be(height),
    u32be(width),
    u16be(0),
    u16be(1),
  ]);
  const postPost = Buffer.concat([Buffer.from([249]), u32be(postAt), Buffer.from([2, 223, 223, 223, 223])]);
  return Buffer.concat([pre, bop, eop, post, postPost]);
}

function readLog(logPath: string): string {
  return existsSync(logPath) ? readFileSync(logPath, "latin1") : "";
}

function quotedPaths(log: string, key: string): string[] {
  const re = new RegExp(key + "='([^']*)'", "g");
  const out: string[] = [];
  for (const m of log.matchAll(re)) {
    if (m[1] && !out.includes(m[1])) {
      out.push(m[1]);
    }
  }
  return out;
}

async function openDvi(path: string, logPath: string): Promise<void> {
  const { proc, client } = await launchControlled(["-log-to-file", logPath, path]);
  try {
    await client.waitForRenderIdle(120000);
    const info = await client.chapterInfo();
    if (info.pageCount !== 1) {
      throw new Error(`dvi-open: pageCount=${info.pageCount}, want 1`);
    }
    await client.quit();
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
  await proc.exited;
}

export async function testit(): Promise<void> {
  if (!dviConverterAvailable()) {
    console.log("dvi-open: skipped, no dvipdfmx and no dvips+Ghostscript");
    return;
  }

  const dir = tmpPath("dvi-open");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const dvi = join(dir, "blank.dvi");
  writeFileSync(dvi, makeBlankDvi());
  const logPath = join(dir, "sumatra.log");

  try {
    await openDvi(dvi, logPath);
    await openDvi(dvi, logPath);

    const st = statSync(dvi);
    const next = new Date(st.mtimeMs + 5000);
    utimesSync(dvi, next, next);
    await openDvi(dvi, logPath);

    const log = readLog(logPath);
    const converts = log.split("\n").filter((l) => l.includes("dvi convert ")).length;
    const hits = log.split("\n").filter((l) => l.includes("dvi cache hit ")).length;
    if (converts !== 2 || hits !== 1) {
      throw new Error(`dvi-open: converts=${converts} hits=${hits}, want 2 and 1`);
    }
  } finally {
    const log = readLog(logPath);
    for (const path of [...quotedPaths(log, "pdf"), ...quotedPaths(log, "meta")]) {
      if (existsSync(path)) {
        unlinkSync(path);
      }
    }
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
