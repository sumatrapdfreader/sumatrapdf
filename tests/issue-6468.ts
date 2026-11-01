// CmdDebugSaveSettings writes the settings file on demand, e.g. from
// `SumatraPDF.exe -reuse-instance -dde "[CmdDebugSaveSettings]"`.
//
// Uses a scratch -appdata dir and no -for-testing, because -for-testing doesn't save.

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, pollUntil, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { sendCopyDataW } from "./winapi.ts";

const kCopyDataDdeW = 0x44646557;
const PDF = resolve("ext/a-zlib/zlib.3.pdf");
const APPDATA = tmpPath("issue-6468-appdata");
const SETTINGS = join(APPDATA, "SumatraPDF-settings.txt");
const kPageSaved = "PageNo = 2";

async function currentPage(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  const raw = String(res[1] ?? "");
  const m = /page=(\d+)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`issue-6468: could not read current tab: ${raw.trim()}`);
  }
  return Number(m[1]);
}

export async function testit(): Promise<void> {
  rmSync(APPDATA, { recursive: true, force: true });
  mkdirSync(APPDATA, { recursive: true });
  writeFileSync(SETTINGS, "UiLanguage = en\nCheckForUpdates = false\nReuseInstance = false\n", "utf8");

  const { proc, client, frame } = await launchControlled(["-appdata", APPDATA, PDF], { saveSettings: true });
  try {
    await client.waitForRenderIdle(30000);

    // changing the page doesn't save, so only the command can put it in the file
    sendCommand(frame, cmdId("CmdGoToNextPage"));
    await pollUntil(
      () => currentPage(client),
      (page) => page === 2,
      { timeoutMs: 5000 * SLOW_BUILD_FACTOR, error: (page) => `issue-6468: on page ${page}, want 2` },
    );
    await client.waitForRenderIdle(30000);
    if (readFileSync(SETTINGS, "utf8").includes(kPageSaved)) {
      throw new Error("issue-6468: page was saved before the command ran, so the test proves nothing");
    }

    sendCopyDataW(frame, kCopyDataDdeW, "[CmdDebugSaveSettings]");
    await pollUntil(
      () => readFileSync(SETTINGS, "utf8").includes(kPageSaved),
      (saved) => saved,
      { timeoutMs: 5000 * SLOW_BUILD_FACTOR, error: `issue-6468: settings file has no '${kPageSaved}'` },
    );
  } finally {
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
