// Ad-hoc: verify hybrid IRS f1040 XFA field edits persist across save/reload.
//
// Run:  bun tests/ad-hoc-xfa-f1040-save.ts [--no-build]

import { unlinkSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { ControlCommand, runControlCommand } from "../cmd/control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const here = dirname(fileURLToPath(import.meta.url));
const PDF = join(here, "ad-hoc-xfa-data", "ad-hoc-f1040.pdf");

// Second filing-status widget on page 1 (from TestXfaFieldRects).
const C1_01_MFJ = ["136.80", "714.00", "148.80", "726.00"] as const;

export async function testit(): Promise<void> {
  const outPdf = tmpPath("ad-hoc-f1040-save-roundtrip.pdf");
  const outPdfC101 = tmpPath("ad-hoc-f1040-save-c1_01-roundtrip.pdf");
  try {
    const [exitCode, raw] = await runControlCommand(EXE, ControlCommand.TestXfaSaveFieldRoundTrip, [
      PDF,
      "c1_11",
      "1",
      outPdf,
    ]);
    const value = String(raw ?? "").trim();
    if (exitCode !== 0) {
      throw new Error(`TestXfaSaveFieldRoundTrip failed: ${value}`);
    }
    if (value !== "1") {
      throw new Error(`expected reloaded c1_11=1, got ${value}`);
    }

    const [exitCode0, raw0] = await runControlCommand(EXE, ControlCommand.TestXfaSaveFieldRoundTrip, [
      PDF,
      "c1_11",
      "0",
      tmpPath("ad-hoc-f1040-save-roundtrip-0.pdf"),
    ]);
    const value0 = String(raw0 ?? "").trim();
    if (exitCode0 !== 0 || value0 !== "0") {
      throw new Error(`expected c1_11 round-trip back to 0, got exit=${exitCode0} value=${value0}`);
    }

    const [exitCodeC101, rawC101] = await runControlCommand(
      EXE,
      ControlCommand.TestXfaSelectRadioSaveRoundTrip,
      [PDF, "c1_01", 1, ...C1_01_MFJ, outPdfC101],
    );
    const valueC101 = String(rawC101 ?? "").trim();
    if (exitCodeC101 !== 0) {
      throw new Error(`TestXfaSelectRadioSaveRoundTrip failed: ${valueC101}`);
    }
    if (valueC101 !== "1") {
      throw new Error(`expected reloaded c1_01=1, got ${valueC101}`);
    }

    console.log("ad-hoc-f1040-save: OK field=c1_11 values=1,0 field=c1_01 value=1");
  } finally {
    for (const path of [outPdf, outPdfC101]) {
      try {
        unlinkSync(path);
      } catch {
        /* ignore */
      }
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}