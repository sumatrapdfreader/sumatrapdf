// #6250: with EscToExit on, Esc in presentation / fullscreen mode leaves that
// mode first; only the next Esc closes the window.

import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { pollUntil, ROOT, runStandalone, tmpPath } from "./util";
import { getFocusedHwnd, isWindowVisible, postMessage, WM_KEYDOWN } from "./winapi";
import { killAndWait, launchControlled } from "./win-automation";

const VK_ESCAPE = 0x1b;
const SRC_PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

type Mode = { presentation: boolean; fullscreen: boolean };

// action "presentation" / "fullscreen" toggles that mode, "get" only reports
async function displayMode(client: ControlClient, action: string): Promise<Mode> {
  const res = await client.request(ControlCommand.TestDisplayMode, [action]);
  const raw = String(res[1] ?? "");
  const m = /presentation=(\d) fullscreen=(\d)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`issue-6250: TestDisplayMode ${action} failed: ${raw}`);
  }
  return { presentation: m[1] === "1", fullscreen: m[2] === "1" };
}

// only WM_KEYDOWN: the app's message loop makes the WM_CHAR
function pressEsc(frame: number): void {
  postMessage(getFocusedHwnd(frame), WM_KEYDOWN, VK_ESCAPE, 0);
}

async function escLeavesMode(client: ControlClient, frame: number, action: string): Promise<void> {
  const on = await displayMode(client, action);
  if (!on.presentation && !on.fullscreen) {
    throw new Error(`issue-6250: could not enter ${action} mode`);
  }
  pressEsc(frame);
  if (!isWindowVisible(frame)) {
    throw new Error(`issue-6250: Esc in ${action} mode closed the window`);
  }
  const off = await pollUntil(
    () => displayMode(client, "get"),
    (mode) => !mode.presentation && !mode.fullscreen,
    { error: `issue-6250: Esc did not leave ${action} mode` },
  );
  if (off.presentation || off.fullscreen) {
    throw new Error(`issue-6250: Esc did not leave ${action} mode`);
  }
}

export async function testit(): Promise<void> {
  const appData = tmpPath("issue-6250");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  const pdf = join(appData, "doc.pdf");
  copyFileSync(SRC_PDF, pdf);
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    ["UiLanguage = en", "CheckForUpdates = false", "RestoreSession = false", "EscToExit = true", ""].join("\n"),
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appData, pdf], { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    await escLeavesMode(client, frame, "presentation");
    await escLeavesMode(client, frame, "fullscreen");

    // out of both modes, EscToExit still closes
    pressEsc(frame);
    await pollUntil(
      () => isWindowVisible(frame),
      (visible) => !visible,
      {
        error: "issue-6250: EscToExit stopped working outside presentation / fullscreen",
      },
    );
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
