// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6424
//
// Convert PDF to Images, Save Selection As Image and Extract Pages into a
// folder that doesn't exist yet must create it.

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

  // each tool writes into its own missing folder
  const cases: { what: string; out: string; cmd: ControlCommand; args: (string | number)[] }[] = [
    {
      what: "convert to images",
      out: join(dir, "images", "sub", "page-1.png"),
      cmd: ControlCommand.TestConvertToImages,
      args: [join(dir, "images", "sub", "page-<N>.png"), "1"],
    },
    {
      what: "save selection",
      out: join(dir, "selection", "sub", "sel.png"),
      cmd: ControlCommand.TestSaveSelectionAsImage,
      args: [join(dir, "selection", "sub", "sel.png"), 72, 1, 10, 10, 50, 50],
    },
    {
      what: "extract pages",
      out: join(dir, "extract", "sub", "pages.pdf"),
      cmd: ControlCommand.TestExtractPages,
      args: [join(dir, "extract", "sub", "pages.pdf"), "1", 0],
    },
  ];

  const failed: string[] = [];
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      for (const c of cases) {
        const res = await client.request(c.cmd, c.args);
        if (res[0] !== 0 || !String(res[1]).startsWith("OK") || !existsSync(c.out)) {
          failed.push(`${c.what}: no ${c.out}: ${JSON.stringify(res)}`);
        }
      }
    },
    [pdf],
  );

  if (failed.length > 0) {
    throw new Error(`issue-6424:\n${failed.join("\n")}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
