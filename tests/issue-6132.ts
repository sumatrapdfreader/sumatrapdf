// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6132
//
// Document Timestamp signatures (PAdES RFC 3161) without an /M entry in the
// signature dictionary must display the timestamp time and policy ID from
// the TimeStampToken's TSTInfo.
//
// Run: bun tests/issue-6132.ts [--no-build]

import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, runStandalone } from "./util.ts";

async function signaturesOf(pdf: string): Promise<string> {
  return withControlledSumatra(
    EXE,
    async (client) => {
      const deadline = Date.now() + 20_000;
      let raw = "";
      for (;;) {
        const res = await client.request(ControlCommand.TestDocumentSignatures);
        const code = res[0] as number;
        raw = String(res[1] ?? "");
        if (code === 0) {
          return raw;
        }
        if (code !== 2) {
          throw new Error(`issue-6132 signatures: ${raw.trim()}`);
        }
        if (Date.now() > deadline) {
          throw new Error(`issue-6132: signatures never ready: ${raw.trim()}`);
        }
        await new Promise((r) => setTimeout(r, 150));
      }
    },
    [pdf],
  );
}

export async function testit(): Promise<void> {
  const pdf = join(ROOT, "tests", "issue-6132.pdf");
  const raw = await signaturesOf(pdf);

  const sig2Match = raw.match(/Signature 2 \(document timestamp\):[\s\S]*?(?=\n\n|$)/);
  if (!sig2Match) {
    throw new Error(`issue-6132: missing Signature 2 (document timestamp):\n${raw}`);
  }
  const sig2Text = sig2Match[0];
  if (!/Signature time:\s*2026\/09\/04 06:07:41 UTC/.test(sig2Text)) {
    throw new Error(`issue-6132: Signature 2 missing or wrong signature time:\n${sig2Text}`);
  }
  if (!/The time and date displayed is from the secure time & date server\./.test(sig2Text)) {
    throw new Error(`issue-6132: Signature 2 missing secure time server notice:\n${sig2Text}`);
  }
  if (!/Policy ID:\s*0\.4\.0\.2023\.1\.1/.test(sig2Text)) {
    throw new Error(`issue-6132: Signature 2 missing Policy ID:\n${sig2Text}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
