// Prefs writes are posted as a uitask except on exit, which saves immediately.
// Closing a file then the window used to drop the deferred save (FileHistory).
// Homepage list vs thumbnails is the same: click schedules a save, caption X
// flushes it, and a watcher echo of our own bytes must not reload.
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util";
import { postMessage, sleep, SC_CLOSE, WM_CLOSE, WM_SYSCOMMAND } from "./winapi";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommand, waitForExit } from "./win-automation";

function readSettings(dir: string): string {
  return readFileSync(join(dir, "SumatraPDF-settings.txt"), "utf8");
}

function viewMode(settings: string): string {
  const m = /^HomePageViewMode\s*=\s*(\S+)/m.exec(settings);
  return m?.[1] ?? "";
}

async function waitHome(client: { homeSelection: () => Promise<{ ready: boolean }> }): Promise<void> {
  const deadline = Date.now() + 8_000;
  for (;;) {
    const st = await client.homeSelection();
    if (st.ready) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error("settings-persist: home page did not layout");
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("settings-persist");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nReuseInstance = false\nRememberOpenedFiles = true\nHomePageViewMode = thumbnails\n",
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

  // home page list vs thumbnails: click the list icon, then the caption X
  // (WM_SYSCOMMAND SC_CLOSE), which is what the window close button posts.
  const dir2 = tmpPath("settings-persist-homeview");
  rmSync(dir2, { recursive: true, force: true });
  mkdirSync(dir2, { recursive: true });
  writeFileSync(
    join(dir2, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nReuseInstance = false\nRememberOpenedFiles = true\nHomePageViewMode = thumbnails\n" +
      `FileStates [\n[\nFilePath = ${pdf}\n]\n]\n`,
  );
  const second = await launchControlled(["-appdata", dir2], { saveSettings: true });
  try {
    await waitHome(second.client);
    const before = await second.client.homeSelection();
    if (before.listView) {
      throw new Error(`settings-persist: expected thumbnails at start: ${before.raw}`);
    }
    const [ix, iy, idx, idy] = before.listIcon;
    if (idx < 8 || idy < 8) {
      throw new Error(`settings-persist: list icon missing: ${before.raw}`);
    }
    const canvas = findCanvas(second.frame);
    if (!canvas) {
      throw new Error("settings-persist: no canvas");
    }
    await clickAt(canvas, ix + Math.floor(idx / 2), iy + Math.floor(idy / 2), 400);
    const afterClick = await second.client.homeSelection();
    if (!afterClick.listView) {
      throw new Error(`settings-persist: click did not switch to list: ${afterClick.raw}`);
    }
    // caption X, like the user: don't wait for the scheduled uitask; exit saves now
    postMessage(second.frame, WM_SYSCOMMAND, SC_CLOSE, 0);
    if (!(await waitForExit(second.proc))) {
      throw new Error("settings-persist: did not exit after homepage view change + caption close");
    }
  } finally {
    second.client.close();
    await killAndWait(second.proc);
  }
  const mode = viewMode(readSettings(dir2));
  if (mode !== "list") {
    throw new Error(`settings-persist: HomePageViewMode after close is '${mode}', want list:\n${readSettings(dir2)}`);
  }

  const third = await launchControlled(["-appdata", dir2], { saveSettings: true });
  try {
    await waitHome(third.client);
    const restored = await third.client.homeSelection();
    if (!restored.listView) {
      throw new Error(`settings-persist: relaunch did not restore list view: ${restored.raw}`);
    }
    postMessage(third.frame, WM_SYSCOMMAND, SC_CLOSE, 0);
    await waitForExit(third.proc);
  } finally {
    third.client.close();
    await killAndWait(third.proc);
  }

  console.log("settings-persist: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
