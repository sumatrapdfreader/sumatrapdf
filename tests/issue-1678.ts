// Regression test for issue #1678.
//
// Additional related file extensions should map to the correct viewer types
// (compared via the type's canonical extension).

import { ControlCommand, runControlCommand } from "../cmd/control.ts";
import { EXE, runStandalone } from "./util.ts";

const CASES: [string, string][] = [
  ["sample.ai", ".pdf"],
  ["archive.ora", ".cbz"],
  ["doc.xod", ".xps"],
  ["scan.djv", ".djvu"],
  ["drawing.dwfx", ".xps"],
];

export async function testit(): Promise<void> {
  for (const [path, ext] of CASES) {
    const [exitCode, raw] = await runControlCommand(EXE, ControlCommand.TestFileKind, [path, ext]);
    if (exitCode !== 0) {
      throw new Error(`issue-1678: ${path} => ${ext} failed: ${(raw ?? "").trim()}`);
    }
    console.log(`issue-1678: ${(raw ?? "").trim()}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}