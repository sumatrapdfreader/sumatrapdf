// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5846
//
// NormalizeURLTemp must collapse consecutive ../../ parent segments so EPUB
// images like ../../cover.jpg from OEBPS/html/ resolve to cover.jpg (ZIP root).
// Covered by EbookDoc_UnitTestNormalizeURL(), run via -unit-tests (debug builds).
//
// Fixture: tests/issue-5846.epub (hand-built repro from the issue: OPF at ZIP
// root, page at OEBPS/html/page.xhtml, images via ../../root.jpg and ../ok.jpg).

import { existsSync } from "node:fs";
import { join } from "node:path";
import { ROOT, runAppUnitTests, runStandalone } from "./util.ts";

const FIXTURE = join(ROOT, "tests", "issue-5846.epub");

export async function testit(): Promise<void> {
  if (!existsSync(FIXTURE)) {
    throw new Error(`issue-5846: missing fixture ${FIXTURE}`);
  }

  await runAppUnitTests();
}

if (import.meta.main) {
  await runStandalone(testit);
}
