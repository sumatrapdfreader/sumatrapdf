// Regression test for issue #5274.
//
// Image file types registered for Open With must not use the PDF document icon.

import { ControlCommand, runControlCommand } from "../cmd/control.ts";
import { EXE, runStandalone } from "./util.ts";

const IMAGE_EXTS = [".jpg", ".png", ".tiff", ".webp"];

export async function testit(): Promise<void> {
  for (const ext of IMAGE_EXTS) {
    const [exitCode, raw] = await runControlCommand(EXE, ControlCommand.TestIconPathForExt, [ext]);
    if (exitCode !== 0) {
      throw new Error(`issue-5274: ${ext} uses PDF icon: ${(raw ?? "").trim()}`);
    }
    console.log(`issue-5274: ${(raw ?? "").trim()}`);
  }
  const [pdfCode, pdfRaw] = await runControlCommand(EXE, ControlCommand.TestIconPathForExt, [".pdf"]);
  if (pdfCode !== 0) {
    throw new Error(`issue-5274: .pdf icon path unexpected: ${(pdfRaw ?? "").trim()}`);
  }
  console.log(`issue-5274: ${(pdfRaw ?? "").trim()}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}