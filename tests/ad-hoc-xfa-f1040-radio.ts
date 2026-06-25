// Ad-hoc: verify hybrid IRS f1040 XFA radio groups (same-named c1_01 widgets).
//
// Run:  bun tests/ad-hoc-xfa-f1040-radio.ts [--no-build]

import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { ControlCommand, runControlCommand } from "../cmd/control.ts";
import { EXE, runStandalone } from "./util.ts";

const here = dirname(fileURLToPath(import.meta.url));
const PDF = join(here, "ad-hoc-xfa-data", "ad-hoc-f1040.pdf");

// Second filing-status radio on page 1 (from TestXfaFieldRects).
const C1_01_MFJ = ["136.80", "714.00", "148.80", "726.00"] as const;

export async function testit(): Promise<void> {
  const [exitCode, xml] = await runControlCommand(EXE, ControlCommand.TestXfaSelectRadioSerializeData, [
    PDF,
    "c1_01",
    1,
    ...C1_01_MFJ,
  ]);
  const data = String(xml ?? "");
  if (exitCode !== 0) {
    throw new Error(`TestXfaSelectRadioSerializeData failed: ${data.trim()}`);
  }
  if (!data.includes("<c1_01>1</c1_01>")) {
    throw new Error("serialized datasets missing marker: <c1_01>1</c1_01>");
  }
  if (data.includes("<c1_01>0</c1_01>")) {
    throw new Error("serialized datasets still contains stale marker: <c1_01>0</c1_01>");
  }

  console.log("ad-hoc-f1040-radio: OK field=c1_01 group-select=MFJ");
}

if (import.meta.main) {
  await runStandalone(testit);
}