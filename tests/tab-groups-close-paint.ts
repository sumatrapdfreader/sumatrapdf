// The tab groups dialog drops its model when it closes, but the window lives
// until its scheduled delete. A paint in that gap (something pumping messages,
// as SetActiveWindow did for Merge PDF) drew the list from the null model.
//
// orig only: ng's dialog is drawn by gpui.
// Run: bun tests/tab-groups-close-paint.ts [--no-build]

import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { cmdId, ROOT, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

export async function testit(): Promise<void> {
  if (USE_NG) {
    console.log("SKIP tab-groups-close-paint: orig only");
    return;
  }
  const dir = tmpPath("tab-groups-close-paint");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  copyFileSync(join(ROOT, "ext", "a-zlib", "zlib.3.pdf"), pdf);
  // one group, so the list has a row to draw
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `RestoreSession = false\nCheckForUpdates = false\nTabGroups [\n\t[\n\t\tName = one\n\t\tTabFiles [\n\t\t\t[\n\t\t\t\tPath = ${pdf}\n\t\t\t]\n\t\t]\n\t]\n]\n`,
  );

  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle(30000);
    sendCommand(frame, cmdId("CmdTabGroupRestore"));
    const deadline = Date.now() + 8000;
    let res: unknown[] = [];
    for (;;) {
      res = await client.request(ControlCommand.TestTabGroups, ["closepaint"]);
      const out = String(res[1] ?? "");
      if (!out.startsWith("NOTREADY")) {
        break;
      }
      if (Date.now() > deadline) {
        throw new Error("tab-groups-close-paint: the dialog never opened");
      }
      await sleep(100);
    }
    if (res[0] !== 0) {
      throw new Error(`tab-groups-close-paint: ${String(res[1] ?? "").trim()}`);
    }
    // still alive after painting the closed dialog
    await client.request(ControlCommand.Ping, []);
    if (proc.exitCode !== null) {
      throw new Error(`tab-groups-close-paint: app exited (${proc.exitCode})`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log("tab-groups-close-paint: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
