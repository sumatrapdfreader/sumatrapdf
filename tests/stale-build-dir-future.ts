// A build dir in SumatraPDF-data whose mtime is in the future (clock skew, a
// copy from another machine) is not stale: the debug build must keep it.
import { existsSync, mkdirSync, readFileSync, rmSync, utimesSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import { sleep } from "./winapi.ts";

// a build id no real build has
const kDirName = "fffff0";
const kYearSec = 365 * 24 * 60 * 60;

export async function testit(): Promise<void> {
  const localAppData = process.env.LOCALAPPDATA;
  if (USE_NG || !localAppData) {
    console.log("stale-build-dir-future: orig on Windows only, skipping");
    return;
  }
  const dir = join(localAppData, "SumatraPDF-data", kDirName);
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "marker.txt"), "x");
  const future = Date.now() / 1000 + kYearSec;
  utimesSync(join(dir, "marker.txt"), future, future);
  utimesSync(dir, future, future);

  const log = tmpPath("stale-build-dir-future.log");
  rmSync(log, { force: true });
  try {
    await withControlledSumatra(EXE, async () => {
      const deadline = Date.now() + 15_000 * SLOW_BUILD_FACTOR;
      for (;;) {
        const s = existsSync(log) ? readFileSync(log, "utf8") : "";
        if (s.includes(kDirName)) break;
        if (Date.now() > deadline) throw new Error("stale-build-dir-future: the stale dir pass never logged our dir");
        await sleep(100);
      }
      // the pass logs before it deletes
      await sleep(500);
    }, ["-log-to-file", log]);
    if (!existsSync(dir)) {
      throw new Error("stale-build-dir-future: a build dir with a future mtime was deleted as stale");
    }
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
