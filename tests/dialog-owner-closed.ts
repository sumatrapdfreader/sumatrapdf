// ng: quitting with a dialog open. The dialog's close callback ran from the
// task queue after its main window was deleted, and read it (use-after-free:
// CloseChangeLanguageDialog, CloseCustomZoomDialog). It now runs while the
// window still exists. An ASan build aborts on the read: a non-zero exit.
//
// Run: bun tests/dialog-owner-closed.ts [--no-build]

import { join } from "node:path";
import { cmdId, ROOT, runStandalone, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, ngToolWindow, sendCommand } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const name = "dialog-owner-closed";
const kWaitMs = 8000;

async function waitFor(what: string, fn: () => Promise<boolean>): Promise<void> {
  const deadline = Date.now() + kWaitMs;
  while (!(await fn())) {
    if (Date.now() > deadline) {
      throw new Error(`${name}: ${what}`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log(`SKIP ${name}: ng only (run with -ng)`);
    return;
  }
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdChangeLanguage"));
    await waitFor("Change Language did not open", async () => !!(await ngToolWindow(client, "changelanguage")));
    // quit with it open: the windows are deleted, then the queued work runs
    await client.quit();
    const code = await Promise.race([proc.exited, sleep(kWaitMs).then(() => "timeout")]);
    if (code !== 0) {
      throw new Error(`${name}: quitting with the dialog open: exit ${code}, signal ${proc.signalCode}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
