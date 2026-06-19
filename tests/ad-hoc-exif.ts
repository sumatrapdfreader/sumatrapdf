// Ad-hoc EXIF corpus test against https://github.com/ianare/exif-py
//
// Clones or updates ../exif-py (sibling directory), runs -dump-exif on each
// file listed in exif-py/tests/resources/dump.txt, and compares output.
//
// Tier A (strict): Image, EXIF, GPS, Interoperability, Thumbnail
// Tier B (strict): MakerNote Tag 0xXXXX and Canon TIFF makernote tags
// Other MakerNote proprietary names (Apple, Olympus, ...) are skipped.
//
// NOT registered in tests/all.ts — run occasionally:
//   bun tests/ad-hoc-exif.ts [--no-build]
// or as part of: bun tests/before-release.ts
//
// Requires git and network access on first run (to clone exif-py).

import { existsSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT, runStandalone } from "./util.ts";

const EXIF_PY = join(ROOT, "..", "exif-py");
const RESOURCES = join(EXIF_PY, "tests", "resources");
const DUMP_TXT = join(RESOURCES, "dump.txt");

const CANON_MAKERNOTE_RE =
  /\/(jpg|tiff)\/(Canon_|canon_)/i;

function fail(msg: string): never {
  throw new Error(msg);
}

function runGit(cmd: string[], cwd: string): void {
  const p = Bun.spawnSync({ cmd, cwd, stdout: "pipe", stderr: "pipe" });
  if (p.exitCode !== 0) {
    const err = p.stderr.toString().trim() || p.stdout.toString().trim();
    fail(`git ${cmd.join(" ")} failed in ${cwd}: ${err}`);
  }
}

function ensureExifPy(): void {
  if (existsSync(join(EXIF_PY, ".git"))) {
    console.log(`• updating ${EXIF_PY} ...`);
    runGit(["git", "reset", "--hard"], EXIF_PY);
    runGit(["git", "pull"], EXIF_PY);
    return;
  }
  console.log(`• cloning exif-py into ${EXIF_PY} ...`);
  runGit(["git", "clone", "https://github.com/ianare/exif-py.git", EXIF_PY], ROOT);
}

type FileBlock = {
  relPath: string;
  lines: string[];
};

function parseDumpTxt(text: string): FileBlock[] {
  const blocks: FileBlock[] = [];
  let current: FileBlock | null = null;

  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trimEnd();
    const m = line.match(/^Opening: tests\/resources\/(.+)$/);
    if (m) {
      if (current) {
        blocks.push(current);
      }
      current = { relPath: m[1], lines: [] };
      continue;
    }
    if (!current) {
      continue;
    }
    if (line.length > 0) {
      current.lines.push(line);
    }
  }
  if (current) {
    blocks.push(current);
  }
  return blocks;
}

function compareLine(relPath: string, line: string): boolean {
  if (line === "File has JPEG thumbnail") {
    return true;
  }
  if (line.startsWith("No EXIF information found")) {
    return true;
  }
  if (/^(Image|EXIF|GPS|Interoperability|Thumbnail) /.test(line)) {
    return true;
  }
  if (line.startsWith("MakerNote Tag 0x")) {
    return true;
  }
  if (line.startsWith("MakerNote ") && CANON_MAKERNOTE_RE.test(relPath)) {
    return true;
  }
  return false;
}

function filterComparable(relPath: string, lines: string[]): string[] {
  return lines.filter((l) => compareLine(relPath, l));
}

const SPAWN_TIMEOUT_MS = 120_000;

async function runDumpExif(absPath: string): Promise<string[]> {
  // stderr must not be piped without being read — SumatraPDF logs there and the
  // child will block once the pipe buffer fills (issue #5677-style GUI/pipe issue).
  const proc = Bun.spawn({
    cmd: [EXE, "-for-testing", "-dump-exif", absPath],
    stdout: "pipe",
    stderr: "ignore",
  });
  let timedOut = false;
  const timer = setTimeout(() => {
    timedOut = true;
    proc.kill();
  }, SPAWN_TIMEOUT_MS);
  const out = await new Response(proc.stdout).text();
  const exitCode = await proc.exited;
  clearTimeout(timer);
  if (timedOut) {
    fail(`-dump-exif timed out after ${SPAWN_TIMEOUT_MS / 1000}s for ${absPath}`);
  }
  if (exitCode !== 0) {
    fail(`-dump-exif failed for ${absPath}: ${out.trim()}`);
  }
  const lines: string[] = [];
  let pastOpening = false;
  for (const raw of out.split(/\r?\n/)) {
    const line = raw.trimEnd();
    if (!pastOpening) {
      if (line.startsWith("Opening: ")) {
        pastOpening = true;
      }
      continue;
    }
    if (line.length > 0) {
      lines.push(line);
    }
  }
  return lines;
}

function diffLines(expected: string[], actual: string[]): { missing: string[]; extra: string[] } {
  const expSet = new Set(expected);
  const actSet = new Set(actual);
  const missing = expected.filter((l) => !actSet.has(l));
  const extra = actual.filter((l) => !expSet.has(l));
  return { missing, extra };
}

export async function testit(): Promise<void> {
  ensureExifPy();
  if (!existsSync(DUMP_TXT)) {
    fail(`missing ${DUMP_TXT} after cloning exif-py`);
  }

  const dumpText = await Bun.file(DUMP_TXT).text();
  const blocks = parseDumpTxt(dumpText);
  if (blocks.length === 0) {
    fail(`no file blocks parsed from ${DUMP_TXT}`);
  }

  let checked = 0;
  const failures: string[] = [];

  for (const block of blocks) {
    const absPath = join(RESOURCES, block.relPath);
    if (!existsSync(absPath)) {
      failures.push(`${block.relPath}: file missing in exif-py checkout`);
      continue;
    }

    const expected = filterComparable(block.relPath, block.lines);
    const actual = filterComparable(block.relPath, await runDumpExif(absPath));
    const { missing, extra } = diffLines(expected, actual);

    if (missing.length > 0 || extra.length > 0) {
      const parts: string[] = [`${block.relPath}:`];
      if (missing.length > 0) {
        parts.push(`  missing (${missing.length}):`);
        for (const l of missing.slice(0, 20)) {
          parts.push(`    - ${l}`);
        }
        if (missing.length > 20) {
          parts.push(`    ... and ${missing.length - 20} more`);
        }
      }
      if (extra.length > 0) {
        parts.push(`  extra (${extra.length}):`);
        for (const l of extra.slice(0, 20)) {
          parts.push(`    + ${l}`);
        }
        if (extra.length > 20) {
          parts.push(`    ... and ${extra.length - 20} more`);
        }
      }
      failures.push(parts.join("\n"));
    }
    checked++;
  }

  if (failures.length > 0) {
    fail(`${failures.length} of ${checked} files differ:\n\n${failures.join("\n\n")}`);
  }
  console.log(`✅ EXIF dump matches exif-py dump.txt for ${checked} files (Tier A + B)`);
}

if (import.meta.main) {
  await runStandalone(testit);
}