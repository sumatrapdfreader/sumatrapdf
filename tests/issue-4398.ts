// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/4398
//
// CmdTogglePageGrid overlays graph paper on paginated documents (default
// ¼ inch minor / 1 inch major, color 128,128,255, dots). Showing it is
// session-only. CmdConfigurePageGrid sets spacing, origin, color and style
// (saved in FixedPageUI.PageGrid).
//
// The fixture is tests/issue-4398.pdf (opaque white page + text). Regenerated
// by this file when run as main.
//
// Run: bun tests/issue-4398.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, runStandalone, tmpPath, assemblePdf } from "./util.ts";

const PDF = join(ROOT, "tests", "issue-4398.pdf");

// Opaque white page so the overlay is on paper, not in a hole.
export function makePageGridPdf(): string {
  const stream = ["1 1 1 rg", "0 0 400 300 re f", "0 0 0 rg", "BT /F1 18 Tf 40 220 Td (Page grid) Tj ET"].join("\n");
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 300] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>",
    `<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`,
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
  ];
  return assemblePdf(objs);
}

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    writeFileSync(PDF, makePageGridPdf());
  }

  const appdata = tmpPath("issue-4398-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });

  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle(30000);
      const off = String((await client.request(ControlCommand.TestCanvasFlags, []))[1] ?? "");
      if (!/show=0/.test(off)) {
        throw new Error(`issue-4398: grid should start off: ${off}`);
      }

      await client.request(ControlCommand.TestInvokeCommand, ["CmdTogglePageGrid"]);
      const on = String((await client.request(ControlCommand.TestCanvasFlags, []))[1] ?? "");
      if (!/show=1/.test(on)) {
        throw new Error(`issue-4398: CmdTogglePageGrid did not enable the overlay: ${on}`);
      }

      await client.request(ControlCommand.TestInvokeCommand, ["CmdTogglePageGrid"]);
      await client.request(ControlCommand.TestCanvasFlags, ["set-grid", 1]);
      const fromSet = String((await client.request(ControlCommand.TestCanvasFlags, []))[1] ?? "");
      if (!/show=1/.test(fromSet)) {
        throw new Error(`issue-4398: set-grid did not enable the overlay: ${fromSet}`);
      }

      const reset = String((await client.request(ControlCommand.TestCanvasFlags, ["reset-grid"]))[1] ?? "");
      if (!/show=1/.test(reset)) {
        throw new Error(`issue-4398: reset-grid turned off Show Grid: ${reset}`);
      }
      if (!/width=72/.test(reset) || !/subdiv=4/.test(reset) || !/ox=0/.test(reset)) {
        throw new Error(`issue-4398: reset-grid did not restore defaults: ${reset}`);
      }
    },
    ["-appdata", appdata, PDF],
  );
}

if (import.meta.main) {
  writeFileSync(PDF, makePageGridPdf());
  const bugs = "C:\\Users\\kjk\\OneDrive\\!sumatra\\bugs\\bug-4398.pdf";
  try {
    writeFileSync(bugs, makePageGridPdf());
    console.log(`wrote ${PDF} and ${bugs}`);
  } catch {
    console.log(`wrote ${PDF}`);
  }
  await runStandalone(testit);
}
