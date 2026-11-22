// Test for issue #6578: a punctuation shortcut binds the character, not the
// key that types it on a US layout. Typing '*', '+' or '=' (WM_CHAR, as any
// layout delivers it) must run the command bound to it.
//
// Run: bun tests/issue-6578.ts [--no-build] [-ng]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, runStandalone, tmpPath } from "./util.ts";
import { isIconic, postMessage, sleep } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";

const kWaitMs = 5000;

const WM_CHAR = 0x0102;
const WM_SYSCOMMAND = 0x0112;
const SC_RESTORE = 0xf120;

const kChars = ["*", "+", "="];

const kSettings = `RestoreSession = false
CheckForUpdates = false
Shortcuts [
${kChars.map((c) => `    [\n        Cmd = CmdMinimizeWindow\n        Key = ${c}\n    ]`).join("\n")}
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
  throw new Error(`issue-6578: ${what}: minimized=${!want}, want ${want}`);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6578");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), kSettings);
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    for (const c of kChars) {
      postMessage(canvas, WM_CHAR, c.charCodeAt(0), 1);
      await waitForIconic(frame, true, `typed '${c}'`);
      postMessage(frame, WM_SYSCOMMAND, SC_RESTORE, 0);
      await waitForIconic(frame, false, "restore");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
