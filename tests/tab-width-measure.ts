// With TabsEqualWidth = false (the default) ng measured every tab's title on
// every frame, slow with pango. The widths are now remembered by text: frames
// that don't change a title must not measure it again.
//
// ng only: it counts the measurements through -dbg-control.
// Run: bun tests/tab-width-measure.ts [--no-build] -ng

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { makePdf, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const kBurst = 20;

async function measures(client: ControlClient): Promise<number> {
  const s = String((await client.request(ControlCommand.TestUiState, ["tabmeasures", 0]))[1] ?? "");
  const m = /\bmeasures=(\d+)/.exec(s);
  if (!m) {
    throw new Error(`tab-width-measure: bad tabmeasures reply: ${s}`);
  }
  return +m[1]!;
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("SKIP tab-width-measure: ng only (run with -ng)");
    return;
  }
  const dir = tmpPath("tab-width-measure");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdfs = ["a.pdf", "b.pdf", "c.pdf"].map((n) => join(dir, n));
  for (const p of pdfs) {
    writeFileSync(p, makePdf(1, 601, 0, 0), "latin1");
  }
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "TabsEqualWidth = false\nRestoreSession = false\nCheckForUpdates = false\n",
  );

  const { proc, client } = await launchControlled(["-appdata", dir, ...pdfs]);
  try {
    await client.waitForRenderIdle();
    await sleep(500);
    const before = await measures(client);
    if (before === 0) {
      throw new Error("tab-width-measure: the tabs were never measured");
    }
    for (let i = 0; i < kBurst; i++) {
      await client.request(ControlCommand.TestInvokeCommand, [i % 2 === 0 ? "CmdZoomIn" : "CmdZoomOut"]);
    }
    await client.waitForRenderIdle();
    const n = (await measures(client)) - before;
    console.log(`tab-width-measure: ${n} tab text measurements for ${kBurst} zoom commands`);
    if (n > 0) {
      throw new Error(`tab-width-measure: ${n} tab text measurements for ${kBurst} zoom commands, want 0`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log("tab-width-measure: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
