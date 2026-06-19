// Runs all regular tests (tests/all.ts) plus ad-hoc / pre-release checks.
//
// Run:  bun tests/before-release.ts [--no-build]
//
// Use before a release to re-verify EXIF parsing and other occasional checks
// that are too slow or need external repos and are not in tests/all.ts.

import { buildApp } from "./util.ts";
import { testit as allTests } from "./all.ts";
import { testit as adHocExif } from "./ad-hoc-exif.ts";
import { testit as issueChmLzx } from "./issue-chm-lzx.ts";

const adHocTests: [string, () => void | Promise<void>][] = [
  ["ad-hoc-exif", adHocExif],
  ["issue-chm-lzx", issueChmLzx],
];

export async function testit(): Promise<void> {
  await allTests();

  for (const [name, fn] of adHocTests) {
    console.log(`\n========== ${name} ==========`);
    try {
      await fn();
    } catch (e) {
      throw new Error(`${name} failed: ${(e as Error)?.message ?? e}`);
    }
  }
  console.log(`\n✅ before-release checks passed (all.ts + ${adHocTests.length} ad-hoc test(s))`);
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