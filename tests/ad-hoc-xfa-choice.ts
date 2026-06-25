// Ad-hoc: verify XFA dropDownList (choice) field serialize and save round-trip.
//
// Fixture: tests/ad-hoc-xfa-data/ad-hoc-xfa-choice.pdf (regen via ad-hoc-xfa-choice.gen.py)
//
// Run:  bun tests/ad-hoc-xfa-choice.ts [--no-build]

import { unlinkSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { ControlCommand, runControlCommand } from "../cmd/control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const here = dirname(fileURLToPath(import.meta.url));
const PDF = join(here, "ad-hoc-xfa-data", "ad-hoc-xfa-choice.pdf");
const FIELD = "filingStatus";
const VALUE = "Married filing jointly";

async function runWithRetry<T>(label: string, fn: () => Promise<T>, attempts = 15): Promise<T> {
  let lastErr: unknown;
  for (let attempt = 1; attempt <= attempts; attempt++) {
    try {
      return await fn();
    } catch (e) {
      lastErr = e;
    }
  }
  throw new Error(`${label} failed after ${attempts} attempts: ${lastErr}`);
}

export async function testit(): Promise<void> {
  const outPdf = tmpPath("ad-hoc-xfa-choice-roundtrip.pdf");
  try {
    const [probeExit, probeXml] = await runWithRetry("TestXfaSerializeData", () =>
      runControlCommand(EXE, ControlCommand.TestXfaSerializeData, [PDF]),
    );
    const probe = String(probeXml ?? "");
    if (probeExit !== 0 || !probe.includes("<datasets") || !probe.includes("<filingStatus>Single</filingStatus>")) {
      throw new Error(`expected valid XFA datasets, got: ${probe.trim()}`);
    }

    const [kindExit, kindRaw] = await runWithRetry("TestXfaFieldKind", () =>
      runControlCommand(EXE, ControlCommand.TestXfaFieldKind, [PDF, FIELD]),
    );
    const kindLine = String(kindRaw ?? "").trim();
    if (kindExit !== 0) {
      throw new Error(`TestXfaFieldKind failed: ${kindLine}`);
    }
    if (!kindLine.includes("kind=4") || !kindLine.includes("choice_count=3")) {
      throw new Error(`expected kind=4 choice_count=3, got ${kindLine}`);
    }

    const [setExit, xml] = await runWithRetry("TestXfaSetFieldSerializeData", () =>
      runControlCommand(EXE, ControlCommand.TestXfaSetFieldSerializeData, [PDF, FIELD, VALUE]),
    );
    const data = String(xml ?? "");
    if (setExit !== 0) {
      throw new Error(`TestXfaSetFieldSerializeData failed: ${data.trim()}`);
    }
    if (!data.includes(`<filingStatus>${VALUE}</filingStatus>`)) {
      throw new Error(`serialized datasets missing <filingStatus>${VALUE}</filingStatus>`);
    }

    const [saveExit, reloaded] = await runWithRetry("TestXfaSaveFieldRoundTrip", () =>
      runControlCommand(EXE, ControlCommand.TestXfaSaveFieldRoundTrip, [PDF, FIELD, VALUE, outPdf]),
    );
    const value = String(reloaded ?? "").trim();
    if (saveExit !== 0) {
      throw new Error(`TestXfaSaveFieldRoundTrip failed: ${value}`);
    }
    if (value !== VALUE) {
      throw new Error(`expected reloaded ${FIELD}=${VALUE}, got ${value}`);
    }

    console.log(`ad-hoc-xfa-choice: OK field=${FIELD} value=${VALUE}`);
  } finally {
    try {
      unlinkSync(outPdf);
    } catch {
      /* ignore */
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}