// Regression test for issue #1678.
//
// Additional related file extensions should map to the correct viewer kinds.

import { ControlCommand, runControlCommand } from "../cmd/control.ts";
import { EXE, runStandalone } from "./util.ts";

const CASES: [string, string][] = [
  ["sample.ai", "filePDF"],
  ["archive.ora", "fileCbz"],
  ["doc.xod", "fileXPS"],
  ["scan.djv", "fileDjVu"],
  ["drawing.dwfx", "fileXPS"],
];

export async function testit(): Promise<void> {
  for (const [path, kind] of CASES) {
    const [exitCode, raw] = await runControlCommand(EXE, ControlCommand.TestFileKind, [path, kind]);
    if (exitCode !== 0) {
      throw new Error(`issue-1678: ${path} => ${kind} failed: ${(raw ?? "").trim()}`);
    }
    console.log(`issue-1678: ${(raw ?? "").trim()}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}