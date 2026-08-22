// Deferred ScheduleSaveSettings posts were dropped on exit: hide/destroy
// could return before the uitask ran, so closing a file then the app lost
// FileHistory, and HomePageViewMode clicks did not persist.
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util";
import { postMessage, sleep, WM_CLOSE } from "./winapi";
import { killAndWait, launchControlled, sendCommand, waitForExit } from "./win-automation";

function readSettings(dir: string): string {
  return readFileSync(join(dir, "SumatraPDF-settings.txt"), "utf8");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("settings-persist");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nRememberOpenedFiles = true\nHomePageViewMode = thumbnails\n",
  );
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf], { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdClose"));
    // homepage has no document; waitForRenderIdle would report no-doc
    await sleep(500);
    postMessage(frame, WM_CLOSE, 0, 0);
    if (!(await waitForExit(proc))) {
      throw new Error("settings-persist: SumatraPDF didn't exit after WM_CLOSE");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  const settings = readSettings(dir);
  if (!settings.includes("zlib.3.pdf")) {
    throw new Error(`settings-persist: closed file missing from FileHistory:\n${settings}`);
  }
  console.log("settings-persist: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
