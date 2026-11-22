// Closing a main window while a dialog is drawn inside it (tool windows off,
// as on wasm and in plugin mode). The dialog stayed marked visible after its
// window was gone, so every other window lost its keyboard shortcuts.
//
// ng only: orig's dialogs are win32 windows.
// Run: bun tests/close-window-with-frame-dialog.ts [--no-build] -ng

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { makePdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const kAccelsAll = 0;
const kAccelsNone = 2;

async function windowCount(client: ControlClient): Promise<number> {
  const s = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
  const m = /\bwindows=(\d+)/.exec(s);
  return m ? +m[1]! : -1;
}

// accelerator table of the first window
async function dialogAccels(client: ControlClient): Promise<number> {
  const s = String((await client.request(ControlCommand.TestToolWindow, ["dialog-accels"]))[1] ?? "");
  const m = /\baccels=(-?\d+)/.exec(s);
  return m ? +m[1]! : -99;
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("SKIP close-window-with-frame-dialog: ng only (run with -ng)");
    return;
  }
  const dir = tmpPath("close-window-with-frame-dialog");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "plain.pdf");
  writeFileSync(pdf, makePdf(1, 400, 0, 0), "latin1");

  const { proc, client } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle(30000);
    await client.request(ControlCommand.TestToolWindow, ["off"]);

    // the custom zoom dialog, drawn inside the first window
    await client.request(ControlCommand.TestInvokeCommand, ["CmdZoomCustom"]);
    await pollUntil(
      async () => dialogAccels(client),
      (a) => a === kAccelsNone,
      { error: (a) => `close-window-with-frame-dialog: accels=${a} with the dialog up, want ${kAccelsNone}` },
    );

    // a second window keeps the app running once the first one closes
    await client.request(ControlCommand.TestInvokeCommand, ["CmdNewWindow"]);
    await pollUntil(
      async () => windowCount(client),
      (n) => n === 2,
      { error: (n) => `close-window-with-frame-dialog: ${n} windows, want 2` },
    );

    await client.request(ControlCommand.TestInvokeCommand, ["WM_CLOSE"]);
    await pollUntil(
      async () => windowCount(client),
      (n) => n === 1,
      { error: (n) => `close-window-with-frame-dialog: ${n} windows after closing one, want 1` },
    );

    const accels = await dialogAccels(client);
    if (accels !== kAccelsAll) {
      throw new Error(`close-window-with-frame-dialog: remaining window has accels=${accels}, want ${kAccelsAll}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log("close-window-with-frame-dialog: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
