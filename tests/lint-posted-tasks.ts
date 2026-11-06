// Lint: a task posted with a raw MainWindow* or WindowTab* must go through
// PostForWindow / PostForTab (src/shared/MainWindowCommon.cpp).
//
// A posted task runs later, after the window or tab may have been closed and
// freed. Each such task used to re-check validity itself, and the ones that
// forgot (or wrote to the tab before checking) were use-after-frees.
//
// Run standalone: bun tests/lint-posted-tasks.ts  (also runs in run-almost-all.ts)

import { readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";

// uitask::Post(MkFunc0(Fn, win), ...)
const INLINE_POST = /uitask::Post\(\s*MkFunc0(<\s*(MainWindow|WindowTab)\s*>)?\(\s*\w+\s*,\s*(win|tab)\s*\)/;
// auto fn = MkFunc0<WindowTab>(Fn, tab); ... uitask::Post(fn, ...)
const TYPED_FUNC = /\b(\w+)\s*=\s*MkFunc0<\s*(MainWindow|WindowTab)\s*>/;

function* cppFiles(dir: string): Generator<string> {
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) {
      yield* cppFiles(p);
    } else if (name.endsWith(".cpp")) {
      yield p;
    }
  }
}

export async function testit(): Promise<void> {
  const srcDir = join(import.meta.dir, "..", "src");
  const violations: string[] = [];
  for (const file of cppFiles(srcDir)) {
    const lines = readFileSync(file, "utf-8").split("\n");
    lines.forEach((line, i) => {
      if (INLINE_POST.test(line)) {
        violations.push(`${file}:${i + 1}: ${line.trim()}`);
        return;
      }
      const m = TYPED_FUNC.exec(line);
      if (!m) {
        return;
      }
      const posted = new RegExp(String.raw`uitask::Post\(\s*${m[1]}\b`);
      if (lines.slice(i + 1, i + 4).some((l) => posted.test(l))) {
        violations.push(`${file}:${i + 1}: ${line.trim()}`);
      }
    });
  }
  if (violations.length > 0) {
    throw new Error(
      "tasks posted with a raw MainWindow* / WindowTab* - use PostForWindow / PostForTab instead:\n" +
        violations.join("\n"),
    );
  }
  console.log("PASS: no raw window / tab pointers in posted tasks");
}

if (import.meta.main) {
  // pure static lint - no app build needed
  try {
    await testit();
  } catch (e) {
    console.error(`❌ ${(e as Error)?.message ?? e}`);
    process.exit(1);
  }
}
