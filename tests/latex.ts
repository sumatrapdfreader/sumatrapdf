// Runs LaTeX / SyncTeX integration tests that need external toolchains
// (MiKTeX pdflatex/lualatex, Tectonic, and/or WSL).
//
// Each test skips gracefully when its dependencies are not installed.
//
// Run:  bun tests/latex.ts [--no-build]

import { buildApp, formatDuration, runTest } from "./util.ts";
import { testit as issue5633 } from "./issue-5633.ts";
import { testit as adHocSynctexChinese } from "./ad-hoc-synctex-chinese.ts";
import { testit as adHocSynctexWsl } from "./ad-hoc-synctex-wsl.ts";

const tests: [string, () => void | Promise<void>][] = [
  ["issue-5633", issue5633],
  ["ad-hoc-synctex-chinese", adHocSynctexChinese],
  ["ad-hoc-synctex-wsl", adHocSynctexWsl],
];

export async function testit(): Promise<void> {
  const t0 = performance.now();
  for (const [name, fn] of tests) {
    console.log(`\n========== ${name} ==========`);
    await runTest(name, fn);
  }
  console.log(
    `\n✅ all ${tests.length} LaTeX test(s) passed in ${formatDuration(performance.now() - t0)}`,
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