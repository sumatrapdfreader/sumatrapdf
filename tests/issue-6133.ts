// #6133: global shortcuts in settings, window MRU targeting, and fallback.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util";
import { FRAME_CLASS, killAndWait, launchControlled, sendCommandSync } from "./win-automation";
import { enumWindows, getClassName, getWindowPid, isWindowVisible, postMessage, sleep, WM_CLOSE } from "./winapi";

const WM_ACTIVATE = 0x0006;
const WA_ACTIVE = 1;
const WM_HOTKEY = 0x0312;
const kGlobalHotkeyBaseId = 0x6000;

function settings(): string {
  return `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
Shortcuts [
\t[
\t\tCmd = CmdGoToNextPage
\t\tKey = Global PageDown
\t]
]
`;
}

function getFrames(pid: number): number[] {
  const res: number[] = [];
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && getClassName(hwnd) === FRAME_CLASS && isWindowVisible(hwnd)) {
      res.push(hwnd);
    }
    return true;
  });
  return res;
}

export async function testit(): Promise<void> {
  const pdf = join(ROOT, "ext", "brotli", "docs", "brotli-comparison-study-2015-09-22.pdf");
  const dir = tmpPath("issue-6133");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), settings());

  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();
    const initialInfo = await client.chapterInfo();
    if (initialInfo.page !== 1) {
      throw new Error(`expected initial page 1, got ${initialInfo.page}`);
    }

    // Trigger global hotkey
    postMessage(frame, WM_HOTKEY, kGlobalHotkeyBaseId, 0);
    await sleep(300);

    const afterInfo = await client.chapterInfo();
    if (afterInfo.page !== 2) {
      throw new Error(`expected page 2 after global hotkey, got ${afterInfo.page}`);
    }

    // Duplicate window to create a second window
    sendCommandSync(frame, cmdId("CmdDuplicateInNewWindow"));
    await sleep(600);

    const frames = getFrames(proc.pid!);
    if (frames.length !== 2) {
      throw new Error(`expected 2 frames after duplicate, got ${frames.length}`);
    }
    const frame2 = frames.find((f) => f !== frame)!;

    // Activate window 2
    postMessage(frame2, WM_ACTIVATE, WA_ACTIVE, 0);
    await sleep(200);

    // Global hotkey should target window 2 (most recently activated)
    postMessage(frame, WM_HOTKEY, kGlobalHotkeyBaseId, 0);
    await sleep(300);

    // Window 1 should still be at page 2
    const w1Info = await client.chapterInfo();
    if (w1Info.page !== 2) {
      throw new Error(`expected window 1 to stay at page 2, got ${w1Info.page}`);
    }

    // Close window 2
    sendCommandSync(frame2, cmdId("CmdClose"));
    const deadline = Date.now() + 3000;
    while (getFrames(proc.pid!).length > 1 && Date.now() < deadline) {
      await sleep(50);
    }

    // Global hotkey should fall back to window 1
    postMessage(frame, WM_HOTKEY, kGlobalHotkeyBaseId, 0);
    await sleep(300);

    const finalInfo = await client.chapterInfo();
    if (finalInfo.page !== 3) {
      throw new Error(`expected page 3 on window 1 after fallback, got ${finalInfo.page}`);
    }

    sendCommandSync(frame, cmdId("CmdExit"));
    await proc.exited;
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
