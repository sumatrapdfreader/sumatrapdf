// src/shared/mupdf/fonts_map.c must list every font in ext/mupdf/resources/fonts
// at the URL cmd/upload-fonts.ts gives it. A mupdf update that adds or changes a
// font otherwise leaves the app unable to download it, and nothing says so.
// Offline: `bun cmd/upload-fonts.ts -check` also checks that each URL answers.
//
// Run: bun tests/font-map-current.ts   (or via tests/run-almost-all.ts)

import { fontMapProblems } from "../cmd/upload-fonts.ts";

export async function testit(): Promise<void> {
  const problems = fontMapProblems();
  if (problems.length > 0) {
    throw new Error(
      `fonts_map.c is out of date, run bun cmd/upload-fonts.ts:\n${problems.slice(0, 10).join("\n")}` +
        (problems.length > 10 ? `\n... and ${problems.length - 10} more` : ""),
    );
  }
  console.log("PASS font-map-current: fonts_map.c lists every built-in font");
}

if (import.meta.main) {
  // static check - no app build needed
  try {
    await testit();
  } catch (e) {
    console.error(`❌ ${(e as Error)?.message ?? e}`);
    process.exit(1);
  }
}
