// Lint: forbid hardcoded command ids in tests.
//
// Command ids (CmdFoo) are auto-numbered in src/Commands.h and shift whenever
// commands are added or removed, so a hardcoded numeric id silently starts
// sending a *different* command. This broke issue-5780 (sent CmdCommandPalette
// instead of CmdOpenNextFileInFolder) and lurked in several ad-hoc tests.
//
// Tests must resolve ids by name with cmdId("CmdName") from tests/util.ts.
// This lint scans the other test files and fails if it finds either:
//   - a `const Cmd... = <number>` constant, or
//   - an inline numeric id passed to sendCommand(win, <number>).
//
// Run standalone: bun tests/lint-command-ids.ts  (also runs first in all.ts)

import { readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";

const HARDCODED_CONST = /\bconst\s+Cmd\w+\s*=\s*\d+/;
const INLINE_SEND = /\bsendCommand\s*\([^,)]+,\s*\d+\s*\)/;

export async function testit(): Promise<void> {
  const dir = import.meta.dir;
  const violations: string[] = [];
  for (const name of readdirSync(dir)) {
    if (!name.endsWith(".ts") || name === "lint-command-ids.ts") {
      continue;
    }
    const text = readFileSync(join(dir, name), "utf-8");
    text.split("\n").forEach((line, i) => {
      if (HARDCODED_CONST.test(line) || INLINE_SEND.test(line)) {
        violations.push(`${name}:${i + 1}: ${line.trim()}`);
      }
    });
  }
  if (violations.length > 0) {
    throw new Error(
      "hardcoded command ids found - use cmdId(\"CmdName\") from tests/util.ts instead:\n" + violations.join("\n"),
    );
  }
  console.log("PASS: no hardcoded command ids in tests");
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
