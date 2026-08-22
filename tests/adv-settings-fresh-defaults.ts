// Fresh settings (empty -appdata) must not mark any Advanced Settings value
// as different from default. Toolbar / Fullscreen.Toolbar / Theme used to
// start empty in the metadata and get filled in at load ("show" / "hide" /
// "Light"), so they showed bold even on a first run.
import { mkdirSync, rmSync } from "node:fs";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { ControlCommand } from "./control.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

export async function testit(): Promise<void> {
  const appdata = tmpPath("adv-settings-fresh-defaults");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });

  const { proc, client, frame } = await launchControlled(["-appdata", appdata]);
  try {
    sendCommandSync(frame, cmdId("CmdAdvancedSettings"));
    const deadline = Date.now() + 8000;
    let out = "";
    while (Date.now() < deadline) {
      const res = await client.request(ControlCommand.TestAdvSettingsRows, ["nondefault"]);
      const exitCode = res[0] as number;
      out = String(res[1] ?? "");
      if (exitCode === 0) {
        break;
      }
      if (exitCode !== 2) {
        throw new Error(`adv-settings-fresh-defaults: ${out.trim()}`);
      }
      await sleep(50);
    }
    if (!/count=\d+/.test(out)) {
      throw new Error(`adv-settings-fresh-defaults: no dump: ${out.trim()}`);
    }
    const count = parseInt(/count=(\d+)/.exec(out)?.[1] ?? "-1", 10);
    if (count !== 0) {
      throw new Error(`adv-settings-fresh-defaults: ${count} values differ from default:\n${out.trim()}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(appdata, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
