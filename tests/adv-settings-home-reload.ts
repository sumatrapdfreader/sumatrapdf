// Saving Advanced Settings while the home page is showing used to crash:
// ReloadSettings destroyed homeRoot after LoadSettings had already rebuilt
// the layout cache, so DrawHomePage painted a null VirtRoot.
//
// Run: bun tests/adv-settings-home-reload.ts [--no-build]

import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import { ControlCommand, type ControlClient } from "./control.ts";

async function adv(client: ControlClient, action: string, arg = 0): Promise<string> {
  const deadline = Date.now() + 8000;
  while (Date.now() < deadline) {
    const res = await client.request(ControlCommand.TestAdvSettingsRows, [action, arg]);
    const exitCode = res[0] as number;
    const out = String(res[1] ?? "");
    if (exitCode === 0) {
      return out;
    }
    if (exitCode !== 2) {
      throw new Error(`adv-settings-home-reload: TestAdvSettingsRows(${action}) failed: ${out.trim()}`);
    }
    await sleep(50);
  }
  throw new Error(`adv-settings-home-reload: Advanced Settings never ready for ${action}`);
}

export async function testit(): Promise<void> {
  const appdata = tmpPath("adv-settings-home-reload");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  const pdf = join(appdata, "doc.pdf");
  copyFileSync(join(ROOT, "ext", "a-zlib", "zlib.3.pdf"), pdf);
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nRememberOpenedFiles = true\nShowStartPage = true\nFileStates [\n\t[\n\t\tFilePath = ${pdf}\n\t\tOpenCount = 1\n\t]\n]\n`,
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata]);
  try {
    const homeDeadline = Date.now() + 8000;
    let home = await client.homeSelection();
    while (!home.ready && Date.now() < homeDeadline) {
      await sleep(50);
      home = await client.homeSelection();
    }
    if (!home.ready) {
      throw new Error(`adv-settings-home-reload: home page never ready (${home.raw})`);
    }

    sendCommandSync(frame, cmdId("CmdAdvancedSettings"));
    await adv(client, "toggle", 0);
    const saved = await adv(client, "save");
    if (!saved.includes("saved=1")) {
      throw new Error(`adv-settings-home-reload: save failed: ${saved.trim()}`);
    }

    const ping = await client.request(ControlCommand.Ping);
    if (ping[0] !== "pong") {
      throw new Error(`adv-settings-home-reload: ping after save: ${JSON.stringify(ping)}`);
    }

    const after = await client.homeSelection();
    if (!after.ready) {
      throw new Error(`adv-settings-home-reload: home page not ready after save (${after.raw})`);
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
