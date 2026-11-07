// Issue #6502: the overlay toolbar had a wider margin right of its buttons
// than left of them.
// https://github.com/sumatrapdfreader/sumatrapdf/issues/6502

import { writeFileSync } from "node:fs";
import { cmdId, makeTextPdf, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { getClientRect, sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const TOOLBAR_CLASS = "SUMATRA_VIRT_TOOLBAR";
const READY_MS = 10_000;

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6502.pdf");
  writeFileSync(pdf, makeTextPdf("overlay toolbar margins"));
  const settings = "Toolbar = overlay\nRestoreSession = false\nCheckForUpdates = false\n";
  const appdata = writeAppdata("issue-6502-appdata", settings);

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();

    // Go To Page reveals the overlay toolbar (#5916)
    sendCommandSync(frame, cmdId("CmdGoToPage"));
    const deadline = Date.now() + READY_MS;
    let items = await client.virtTree(TOOLBAR_CLASS);
    while (items.length === 0) {
      if (Date.now() > deadline) {
        throw new Error("issue-6502: the overlay toolbar didn't show");
      }
      await sleep(50);
      items = await client.virtTree(TOOLBAR_CLASS);
    }

    const host = getClientRect(items[0]!.hwnd);
    const left = Math.min(...items.map((c) => c.rect.x));
    const right = Math.max(...items.map((c) => c.rect.x + c.rect.dx));
    const leftMargin = left - host.left;
    const rightMargin = host.right - right;
    if (leftMargin !== rightMargin) {
      throw new Error(`issue-6502: left margin ${leftMargin}px, right margin ${rightMargin}px`);
    }
  } finally {
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
