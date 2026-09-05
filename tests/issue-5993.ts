// View Certificate and Update EU Trusted List are only on Document Properties
// when the PDF has signature certificates (issue #5993, #3997).

import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { cmdId, pollUntil, ROOT, runStandalone, SLOW_BUILD_FACTOR } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const UNSIGNED_PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
const SIGNED_PDF = join(ROOT, "tests", "issue-5581-data", "test_sign_PAdES_B-T.pdf");
const CBZ = join(ROOT, "tests", "issue-1201.cbz");

type Buttons = { copy: number; viewCert: number; updateEutl: number; raw: string };

function parseButtons(raw: string): Buttons | null {
  const m = /copy=(\d+) viewCert=(\d+) updateEutl=(\d+)/.exec(raw);
  if (!m) {
    return null;
  }
  return { copy: +m[1]!, viewCert: +m[2]!, updateEutl: +m[3]!, raw };
}

async function propertiesButtons(path: string): Promise<Buttons> {
  const { proc, client, frame } = await launchControlled([path]);
  try {
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId("CmdProperties"));
    const buttons = await pollUntil(
      async () => {
        const res = await client.request(ControlCommand.TestDocumentProperties, ["buttons"]);
        return parseButtons(String(res[1] ?? ""));
      },
      (b) => b !== null,
      {
        timeoutMs: 8000 * SLOW_BUILD_FACTOR,
        error: (b) => `issue-5993: properties buttons not ready (${b?.raw ?? "empty"})`,
      },
    );
    if (!buttons) {
      throw new Error("issue-5993: properties buttons not ready");
    }
    return buttons;
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

function expectButtons(got: Buttons, want: { copy: number; viewCert: number; updateEutl: number }, what: string): void {
  if (got.copy !== want.copy || got.viewCert !== want.viewCert || got.updateEutl !== want.updateEutl) {
    throw new Error(
      `issue-5993: ${what}: got ${got.raw}, want copy=${want.copy} viewCert=${want.viewCert} updateEutl=${want.updateEutl}`,
    );
  }
}

export async function testit(): Promise<void> {
  expectButtons(await propertiesButtons(UNSIGNED_PDF), { copy: 1, viewCert: 0, updateEutl: 0 }, "unsigned PDF");
  expectButtons(await propertiesButtons(CBZ), { copy: 1, viewCert: 0, updateEutl: 0 }, "comic book");
  expectButtons(await propertiesButtons(SIGNED_PDF), { copy: 1, viewCert: 1, updateEutl: 1 }, "signed PDF");
  console.log("PASS: PDF certification actions only when the PDF has certificates (issue #5993)");
}

if (import.meta.main) {
  await runStandalone(testit);
}
