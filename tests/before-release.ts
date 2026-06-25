// Runs all regular tests (tests/all.ts) plus ad-hoc / pre-release checks.
//
// Run:  bun tests/before-release.ts [--no-build]
//
// Use before a release to re-verify EXIF parsing and other occasional checks
// that are too slow or need external repos and are not in tests/all.ts.

import { buildApp, formatDuration, runTest } from "./util.ts";
import { testit as allTests } from "./all.ts";
import { testit as adHocExif } from "./ad-hoc-exif.ts";
import { testit as adHocSelectionTranslate } from "./ad-hoc-selection-translate.ts";
import { testit as adHocTripleClickLine } from "./ad-hoc-triple-click-line.ts";
import { testit as adHocTocPaletteSync } from "./ad-hoc-toc-palette-sync.ts";
import { testit as adHocJpegDecode } from "./ad-hoc-jpeg-decode.ts";
import { testit as adHocPasteImageAnnot } from "./ad-hoc-paste-image-annot.ts";
import { testit as adHocXfa } from "./ad-hoc-xfa.ts";
import { testit as adHocXfaCorpus } from "./ad-hoc-xfa-corpus.ts";
import { testit as adHocXfaF1040Visual } from "./ad-hoc-xfa-f1040-visual.ts";
import { testit as latexTests } from "./latex.ts";
import { testit as issueChmLzx } from "./issue-chm-lzx.ts";

const adHocTests: [string, () => void | Promise<void>][] = [
  ["ad-hoc-exif", adHocExif],
  ["ad-hoc-selection-translate", adHocSelectionTranslate],
  ["ad-hoc-triple-click-line", adHocTripleClickLine],
  ["ad-hoc-toc-palette-sync", adHocTocPaletteSync],
  ["ad-hoc-jpeg-decode", adHocJpegDecode],
  ["ad-hoc-paste-image-annot", adHocPasteImageAnnot],
  ["ad-hoc-xfa", adHocXfa],
  ["ad-hoc-xfa-corpus", adHocXfaCorpus],
  ["ad-hoc-xfa-f1040-visual", adHocXfaF1040Visual],
  ["issue-chm-lzx", issueChmLzx],
];

export async function testit(): Promise<void> {
  const t0 = performance.now();
  await allTests();

  console.log("\n========== latex ==========");
  await runTest("latex", latexTests);

  for (const [name, fn] of adHocTests) {
    console.log(`\n========== ${name} ==========`);
    await runTest(name, fn);
  }
  console.log(
    `\n✅ before-release checks passed (all.ts + latex.ts + ${adHocTests.length} ad-hoc test(s)) in ${formatDuration(performance.now() - t0)}`,
  );
}

if (import.meta.main) {
  if (!process.argv.includes("--no-build")) {
    buildApp();
  }
  try {
    await testit();
  } catch (e) {
    console.error(`\n❌ ${(e as Error)?.message ?? e}`);
    process.exit(1);
  }
  process.exit(0);
}