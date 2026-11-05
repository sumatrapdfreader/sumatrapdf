// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6488
//
// Drag-selecting Arabic text from the right edge of a line's first letter
// started the selection at the second letter. The glyph hit test took "past
// the middle of the glyph" along +x, but right-to-left text runs along -x.
// Mid-line, where letter and space boxes overlap, it also took the space
// before the word.
//
// Fixture: tests/issue-6483.pdf (the same LibreOffice export as #6483).
//
// Run:  bun tests/issue-6488.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone } from "./util.ts";

const PDF = join(import.meta.dir, "issue-6483.pdf");

// the first line's first word, and one from its middle
const WORDS = ["ليت", "شعري"];

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    console.log(`SKIP issue-6488: fixture not found: ${PDF}`);
    return;
  }

  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      for (const word of WORDS) {
        const [code, raw] = await client.request(ControlCommand.TestSelectWordEdges, [word]);
        const res = String(raw ?? "").trim();
        if (code !== 0) {
          throw new Error(`issue-6488: ${word}: ${res}`);
        }
        const line = res.split("\n").find((l) => l.startsWith("text=")) ?? "";
        const selected = line.slice("text=".length);
        if (selected !== word) {
          throw new Error(`issue-6488: dragging across '${word}' selected '${selected}'\n${res}`);
        }
      }
    },
    [PDF],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
