// Copy (Ctrl+C) of a rectangular selection puts its text and an image of the
// rectangle on the clipboard, so it pastes into both a text editor and Paint.
// ng used to copy the text only.
//
// Run: bun tests/copy-rect-selection-image.ts [-ng] [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { cmdId, makeTextPdf, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import { clipboardHasImage, emptyClipboard, getClipboardText } from "./winapi.ts";

const WORD = "copyme";

export async function testit(): Promise<void> {
  if (!IS_WIN) {
    console.log("SKIP copy-rect-selection-image: gpui's clipboard is text only off Windows");
    return;
  }
  const pdf = tmpPath("copy-rect-selection-image.pdf");
  writeFileSync(pdf, makeTextPdf(WORD));
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await pollUntil(
      async () => String((await client.request(ControlCommand.TestDrawRectSelection, [WORD]))[1] ?? ""),
      (raw) => !raw.includes("NOTREADY"),
      { error: (raw) => `copy-rect-selection-image: no rectangular selection: ${raw}` },
    );
    emptyClipboard();
    sendCommandSync(frame, cmdId("CmdCopySelection"));
    await pollUntil(
      () => clipboardHasImage(),
      (has) => has,
      { timeoutMs: 3000, error: "copy-rect-selection-image: no image on the clipboard" },
    );
    const text = await getClipboardText();
    if (!text.includes(WORD)) {
      throw new Error(`copy-rect-selection-image: clipboard text is ${JSON.stringify(text)}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
