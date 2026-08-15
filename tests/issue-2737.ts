// Regression test for issue #2737: ChmUI.FontName must override fonts embedded
// in a CHM when the fixed-page view is used.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const CHM = join(import.meta.dir, "issue-2737.chm");

async function waitForFonts(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const [codeArg, outArg] = await client.request(ControlCommand.TestDocumentFontList);
    const code = Number(codeArg);
    const out = String(outArg ?? "").trim();
    if (code === 0) return out;
    if (code !== 2) throw new Error(`issue-2737: font-list probe failed: ${out}`);
    if (Date.now() > deadline) throw new Error(`issue-2737: timed out waiting for CHM: ${out}`);
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
}

export async function testit(): Promise<void> {
  const appData = tmpPath("issue-2737-appdata");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
ChmUI [
    UseFixedPageUI = true
    FontName = Arial
]
`,
  );

  await withControlledSumatra(
    EXE,
    async (client) => {
      const out = await waitForFonts(client);
      if (!out.includes("Arial")) {
        throw new Error(`issue-2737: expected ChmUI.FontName Arial, got: ${out}`);
      }
      if (out.includes("Courier New")) {
        throw new Error(`issue-2737: document font was not overridden: ${out}`);
      }
    },
    ["-appdata", appData, CHM],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
