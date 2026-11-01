// Online revocation checks on a real certificate chain: asks the CA, then
// expects the answer to be found offline. Needs the network, so it's not in the
// suites.

import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, runStandalone, writeAppdata } from "./util.ts";

const SAMPLE = join(ROOT, "tests", "issue-5581-data", "test_sign_PAdES_B-LTA.pdf");
const DOC_TIMESTAMP = /Signature 2 \(document timestamp\):[\s\S]*?Revocation: ([^\n]*)/;

export async function testit(): Promise<void> {
  const appdata = writeAppdata("ad-hoc-sig-revocation", "RestoreSession = false\nCheckForUpdates = false\n");
  const raw = await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      const before = String((await client.request(ControlCommand.TestDocumentSignatures))[1] ?? "");
      console.log(`before: ${DOC_TIMESTAMP.exec(before)?.[1]}`);
      await client.request(ControlCommand.TestFetchRevocation);
      return String((await client.request(ControlCommand.TestDocumentSignatures))[1] ?? "");
    },
    ["-appdata", appdata, SAMPLE],
  );
  const after = DOC_TIMESTAMP.exec(raw)?.[1] ?? "";
  console.log(`after: ${after}`);
  if (!after.includes("wasn't revoked")) {
    throw new Error(`ad-hoc-sig-revocation: no answer from the CA:\n${raw}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
