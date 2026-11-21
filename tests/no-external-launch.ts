// In test mode (-for-testing) nothing is opened outside the app: a test that
// runs Help > Visit Website or Show in Folder mustn't put a browser or Finder
// in front of whoever uses the computer (gBlockExternalLaunch in
// src/base/Launch.h, gp::SetOpenExternalHook for gpui's own links). The app
// counts what it kept back: TestUiState's launchesBlocked.
//
// ng only: orig doesn't set it.
// Run: bun tests/no-external-launch.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, ROOT, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const name = "no-external-launch";

async function blocked(client: ControlClient): Promise<number> {
  const raw = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
  const m = / launchesBlocked=(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`${name}: no launchesBlocked in TestUiState: ${raw}`);
  }
  return Number(m[1]);
}

async function waitBlocked(client: ControlClient, want: number, what: string): Promise<void> {
  const deadline = Date.now() + 5000;
  let n = 0;
  while (Date.now() < deadline) {
    n = await blocked(client);
    if (n >= want) {
      return;
    }
    await sleep(50);
  }
  throw new Error(`${name}: ${what} wasn't kept back: launchesBlocked=${n}, want ${want}`);
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log(`SKIP ${name}: ng only (run with -ng)`);
    return;
  }
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\n",
  );
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();
    const n = await blocked(client);
    // through gp::OpenUrl
    sendCommand(frame, cmdId("CmdHelpVisitWebsite"));
    await waitBlocked(client, n + 1, "Visit Website");
    sendCommand(frame, cmdId("CmdHelpOpenManualOnWebsite"));
    await waitBlocked(client, n + 2, "the manual on the website");
    // through OpenPathInDefaultFileManager
    sendCommand(frame, cmdId("CmdShowInFolder"));
    await waitBlocked(client, n + 3, "Show in Folder");
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
