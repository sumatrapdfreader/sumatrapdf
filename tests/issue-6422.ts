// #6422: Convert PDF to Images can write GIF, next to PNG / JPEG / BMP.
//
// Run: bun tests/issue-6422.ts [--no-build]

import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { convert, makeTwoPagePdf } from "./issue-5991.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
RememberStatePerDocument = false
`;

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6422");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);
  const pdf = join(dir, "pages.pdf");
  writeFileSync(pdf, makeTwoPagePdf());

  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      await convert(client, join(dir, "p-<N>.gif"), "all");
      for (const pageNo of [1, 2]) {
        const path = join(dir, `p-${pageNo}.gif`);
        if (!existsSync(path)) {
          throw new Error(`issue-6422: no ${path}`);
        }
        const buf = readFileSync(path);
        if (buf.toString("latin1", 0, 4) !== "GIF8") {
          throw new Error(`issue-6422: ${path} is not a GIF (${buf.length} bytes)`);
        }
      }
    },
    ["-appdata", dir, pdf],
  );

  console.log("issue-6422: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
