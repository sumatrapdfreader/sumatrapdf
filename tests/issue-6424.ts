// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6424
//
// Convert PDF to Images into a folder that doesn't exist yet must create it.

import { existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, makeBlankPdf, runStandalone, tmpPath } from "./util.ts";

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6424");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "blank.pdf");
  writeFileSync(pdf, makeBlankPdf(), "latin1");

  const destDir = join(dir, "missing", "sub");
  let res: unknown[] = [];
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      res = await client.request(ControlCommand.TestConvertToImages, [join(destDir, "page-<N>.png"), "1"]);
    },
    [pdf],
  );

  const out = join(destDir, "page-1.png");
  if (res[0] !== 0 || !String(res[1]).startsWith("OK") || !existsSync(out)) {
    throw new Error(`issue-6424: no ${out}: ${JSON.stringify(res)}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
