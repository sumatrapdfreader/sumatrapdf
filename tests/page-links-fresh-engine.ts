// TestPageLinks opens its own engine. On an EPUB a page past the end, or in a
// chapter not laid out yet, tripped a debug report (!loc.IsValid()) instead
// of laying the chapter out or failing cleanly.
//
// Run: bun tests/page-links-fresh-engine.ts [--no-build]

import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, runStandalone } from "./util.ts";

const EPUB = join(ROOT, "docs", "test", "test.epub");
const PDF = join(ROOT, "docs", "test", "links.pdf");

export async function testit(): Promise<void> {
  await withControlledSumatra(EXE, async (client) => {
    const ok = await client.request(ControlCommand.TestPageLinks, [PDF, 1]);
    const okText = String(ok[1] ?? "");
    if (ok[0] !== 0 || okText.includes("ERROR")) {
      throw new Error(`TestPageLinks(links.pdf, 1) failed: ${okText}`);
    }
    const past = await client.request(ControlCommand.TestPageLinks, [EPUB, 9999]);
    const pastText = String(past[1] ?? "");
    if (past[0] === 0 || !pastText.includes("page-load-failed")) {
      throw new Error(`TestPageLinks(9999) should fail cleanly: ${pastText}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
