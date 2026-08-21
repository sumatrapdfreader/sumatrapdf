// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/1189
//
// Search should find words that are split across a line break with a hyphen
// (end-of-line hyphenation / soft hyphen). The fixture PDF has "hyphen-" on
// one line and "ated" on the next; searching "hyphenated" must find it.
//
// Run:  bun tests/issue-1189.ts [--no-build]

import { existsSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone } from "./util.ts";
import { ControlCommand, withControlledSumatra } from "./control.ts";

const PDF = join(import.meta.dir, "issue-1189.pdf");

export async function testit(): Promise<void> {
  if (!existsSync(EXE)) {
    throw new Error(`app not found: ${EXE} (build first)`);
  }
  if (!existsSync(PDF)) {
    throw new Error(`test pdf not found: ${PDF}`);
  }

  await withControlledSumatra(EXE, async (client) => {
    const search = async (needle: string): Promise<string> => {
      const [, rawArg] = await client.request(ControlCommand.TestSearch, [PDF, needle]);
      return String(rawArg).trim();
    };

    const dehyphenated = await search("hyphenated");
    if (!dehyphenated.startsWith("FOUND")) {
      throw new Error(`search "hyphenated" should find line-broken "hyphen-\\nated", got: ${dehyphenated}`);
    }
    console.log(`✅ ${dehyphenated}`);

    // sanity: the first half still matches as a substring
    const partial = await search("hyphen");
    if (!partial.startsWith("FOUND")) {
      throw new Error(`search "hyphen" should still find the first line, got: ${partial}`);
    }
    console.log(`✅ ${partial}`);
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
