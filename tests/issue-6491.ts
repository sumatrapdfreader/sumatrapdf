// Test for issue #6491: CmdMinimizeWindow minimizes the window. It has no
// default shortcut; Shift + Esc bound in Shortcuts must run it.
//
// Run: bun tests/issue-6491.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, ROOT, runStandalone, tmpPath } from "./util.ts";
import { isIconic, postKeyWithShift, postMessage, sleep, VK_ESCAPE, WM_KEYDOWN } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const kWaitMs = 5000;

const WM_SYSCOMMAND = 0x0112;
const SC_RESTORE = 0xf120;

const kSettings = `RestoreSession = false
CheckForUpdates = false
Shortcuts [
    [
        Cmd = CmdMinimizeWindow
        Key = Shift + Esc
    ]
]
`;

async function waitForIconic(frame: number, want: boolean, what: string): Promise<void> {
  const deadline = Date.now() + kWaitMs;
  while (Date.now() < deadline) {
    if (isIconic(frame) === want) {
      return;
    }
    await sleep(50);
  }
  throw new Error(`issue-6491: ${what}: minimized=${!want}, want ${want}`);
}

async function restore(frame: number): Promise<void> {
  postMessage(frame, WM_SYSCOMMAND, SC_RESTORE, 0);
  await waitForIconic(frame, false, "restore");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6491");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), kSettings);
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);

    sendCommand(frame, cmdId("CmdMinimizeWindow"));
    await waitForIconic(frame, true, "CmdMinimizeWindow");
    await restore(frame);

    // Esc alone isn't bound: it must not minimize
    postMessage(canvas, WM_KEYDOWN, VK_ESCAPE, 1);
    await sleep(300);
    if (isIconic(frame)) {
      throw new Error("issue-6491: Esc without Shift minimized the window");
    }

    const release = postKeyWithShift(canvas, VK_ESCAPE);
    try {
      await waitForIconic(frame, true, "Shift + Esc");
    } finally {
      release();
    }
    await restore(frame);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
