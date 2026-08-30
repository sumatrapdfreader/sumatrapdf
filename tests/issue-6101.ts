// #6101: a hyperlink drawn through an OCG that is off on screen (PrintState ON)
// vanished when printed. RenderTarget::Print must still paint that layer.
//
// Fixture tests/issue-6101.pdf is the report's template.pdf (LuaLaTeX + ocgx2).
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, runStandalone } from "./util.ts";

const PDF = join(ROOT, "tests", "issue-6101.pdf");

function parseCounts(raw: string): { view: number; print: number } {
  const m = /viewNonwhite=(\d+) printNonwhite=(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-6101: could not parse: ${raw}`);
  }
  return { view: +m[1]!, print: +m[2]! };
}

export async function testit(): Promise<void> {
  await withControlledSumatra(EXE, async (client) => {
    const res = await client.request(ControlCommand.TestRenderViewPrint, [PDF]);
    const raw = String(res[1] ?? "");
    if (res[0] !== 0) {
      throw new Error(`issue-6101: ${raw.trim()}`);
    }
    const c = parseCounts(raw);
    if (c.view < 500) {
      throw new Error(`issue-6101: view render looks empty (viewNonwhite=${c.view}):\n${raw}`);
    }
    // Print-only OCG is the same clipped glyphs as the on-screen purple.
    // Without PrintState ON, the link is dropped (~8% less ink on this fixture).
    // A lost clip floods the page.
    if (c.print < Math.floor((c.view * 95) / 100) || c.print > Math.floor((c.view * 120) / 100)) {
      throw new Error(`issue-6101: print OCG ink mismatch (viewNonwhite=${c.view} printNonwhite=${c.print}):\n${raw}`);
    }
    console.log(`issue-6101: viewNonwhite=${c.view} printNonwhite=${c.print}`);
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
