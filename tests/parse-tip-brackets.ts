// Regression: ParseTip must not hang on '[' in plain text (e.g. loading
// notifications for files like "Apocalypse Bringer Mynoghra_01 [CIW].pdf").

import { ControlCommand, runControlCommand } from "../cmd/control.ts";
import { EXE, runStandalone } from "./util.ts";

const LOADING =
  "Loading Apocalypse Bringer Mynoghra_01 [CIW].pdf ...";

export async function testit(): Promise<void> {
  const [exitCode, raw] = await runControlCommand(EXE, ControlCommand.TestParseTip, [LOADING]);
  if (exitCode !== 0) {
    throw new Error(`parse-tip-brackets: ParseTip failed: ${(raw ?? "").trim()}`);
  }
  const line = (raw ?? "").trim();
  console.log(`parse-tip-brackets: ${line}`);
  if (!line.includes("words=") || line.includes("words=0")) {
    throw new Error(`parse-tip-brackets: expected words > 0, got ${line}`);
  }
  if (!line.includes("[CIW]")) {
    throw new Error(`parse-tip-brackets: bracket text missing from: ${line}`);
  }
  if (!line.includes("links=0")) {
    throw new Error(`parse-tip-brackets: expected no links, got ${line}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}