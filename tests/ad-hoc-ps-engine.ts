// Opening a PostScript file (issue #6200) must run Ghostscript exactly once.
// The thumbnail used to re-open the file, converting it a second time, and
// EnginePs::Clone() could not clone (no file path, no re-readable stream), so
// printing and palette thumbnails saw a 1-page document.
//
// Ad-hoc: needs Ghostscript installed, so it is not in any suite. It skips when
// Ghostscript is missing.
//
// Run: bun tests/ad-hoc-ps-engine.ts [--no-build]

import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ROOT, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const PS_PAGE_COUNT = 2;

function findGhostscript(): string {
  for (const base of ["C:\\Program Files\\gs", "C:\\Program Files (x86)\\gs"]) {
    if (!existsSync(base)) {
      continue;
    }
    for (const ver of readdirSync(base)) {
      for (const exe of ["gswin64c.exe", "gswin32c.exe"]) {
        const path = join(base, ver, "bin", exe);
        if (existsSync(path)) {
          return path;
        }
      }
    }
  }
  return "";
}

// a 2-page PDF from the repo, converted to PostScript
async function makePsFile(gs: string, dir: string): Promise<string> {
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const dst = join(dir, "multipage.ps");
  const proc = Bun.spawn([gs, "-q", "-dNOPAUSE", "-dBATCH", "-sDEVICE=ps2write", `-sOutputFile=${dst}`, src], {
    stdout: "ignore",
    stderr: "ignore",
  });
  const code = await proc.exited;
  if (code !== 0 || !existsSync(dst)) {
    throw new Error(`ad-hoc-ps-engine: ghostscript could not make ${dst} (exit ${code})`);
  }
  return dst;
}

function readLog(logPath: string): string {
  return existsSync(logPath) ? readFileSync(logPath, "latin1") : "";
}

export async function testit(): Promise<void> {
  const gs = findGhostscript();
  if (!gs) {
    console.log("ad-hoc-ps-engine: skipped, Ghostscript is not installed (get it from ghostscript.com)");
    return;
  }

  const dir = tmpPath("ad-hoc-ps-engine");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const ps = await makePsFile(gs, dir);
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });

  const logPath = join(dir, "sumatra.log");

  // no -for-testing: thumbnails (which used to re-convert) need file history
  const { proc, client } = await launchControlled(["-appdata", appdata, "-log-to-file", logPath, ps], {
    saveSettings: true,
  });
  try {
    await client.waitForRenderIdle(30000);

    const info = await client.chapterInfo();
    if (info.pageCount !== PS_PAGE_COUNT) {
      throw new Error(`ad-hoc-ps-engine: pageCount=${info.pageCount}, want ${PS_PAGE_COUNT}`);
    }

    // wait for the thumbnail, the step that used to convert a second time
    const deadline = Date.now() + 20000 * SLOW_BUILD_FACTOR;
    while (Date.now() < deadline && !/SetThumbnailFromFile/.test(readLog(logPath))) {
      await sleep(200);
    }
    await client.quit();
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
  await proc.exited;

  const log = readLog(logPath);
  if (log.length === 0) {
    throw new Error(`ad-hoc-ps-engine: no log at ${logPath} (needs a debug build)`);
  }
  const runs = log.split("\n").filter((l) => /EnginePs\.cpp:\d+: using /.test(l)).length;
  if (runs !== 1) {
    throw new Error(`ad-hoc-ps-engine: ghostscript ran ${runs} times, want 1`);
  }
  rmSync(dir, { recursive: true, force: true });
}

if (import.meta.main) {
  await runStandalone(testit);
}
