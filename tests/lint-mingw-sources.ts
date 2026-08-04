// Lint: cmd/build-with-mingw.ts must compile the same src/*.cpp files as premake.
//
// The Linux/Wine CI job cross-compiles with mingw-w64 using its own copy of the
// source list in cmd/build-with-mingw.ts, which "replicates build logic from
// premake5.lua / premake5.files.lua". Adding a new src file to premake (and the
// .vcxproj) but not to that list builds fine on Windows and then fails the CI
// job at link time with undefined references -- which is how ScreenshotCapture,
// ImageEditHostSumatra and ShortcutParse each broke master in turn.
//
// This compares the src patterns of premake's sumatrapdf_files() against the
// mingw list and fails on any src/*.cpp premake compiles that mingw doesn't.
//
// Run standalone: bun tests/lint-mingw-sources.ts  (also runs in all.ts)

import { readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";

// files premake compiles that mingw deliberately leaves out. Keep this list
// tiny and say why: each entry is a file the cross-compile cannot build.
const MINGW_EXCLUDED: Record<string, string> = {
  "TextToSpeech.cpp": "WinRT (windows.media.speechsynthesis.h) / SAPI, not in mingw-w64",
};

function patternToRe(p: string): RegExp {
  const escaped = p.replace(/[.+^${}()|[\]\\]/g, "\\$&").replace(/\*/g, ".*");
  return new RegExp(`^${escaped}$`, "i");
}

// the src patterns premake's sumatrapdf_files() compiles
function premakeSrcPatterns(root: string): string[] {
  const lua = readFileSync(join(root, "premake5.files.lua"), "utf-8");
  const start = lua.indexOf("function sumatrapdf_files()");
  if (start < 0) {
    throw new Error("lint-mingw-sources: sumatrapdf_files() not found in premake5.files.lua");
  }
  const body = lua.slice(start, lua.indexOf("\nend", start));
  const m = /files_in_dir\("src",\s*\{([\s\S]*?)\}\)/.exec(body);
  if (!m) {
    throw new Error(`lint-mingw-sources: files_in_dir("src", ...) not found in sumatrapdf_files()`);
  }
  return [...m[1].matchAll(/"([^"]+)"/g)].map((x) => x[1]);
}

// the src patterns cmd/build-with-mingw.ts compiles
function mingwSrcPatterns(root: string): string[] {
  const ts = readFileSync(join(root, "cmd", "build-with-mingw.ts"), "utf-8");
  const out: string[] = [];
  for (const group of ts.matchAll(/dir:\s*"src",\s*patterns:\s*\[([^\]]*)\]/g)) {
    for (const q of group[1].matchAll(/"([^"]+)"/g)) {
      out.push(q[1]);
    }
  }
  if (out.length === 0) {
    throw new Error(`lint-mingw-sources: no dir: "src" pattern groups found in cmd/build-with-mingw.ts`);
  }
  return out;
}

export async function testit(): Promise<void> {
  const root = join(import.meta.dir, "..");
  const premake = premakeSrcPatterns(root);
  const mingw = mingwSrcPatterns(root).map(patternToRe);
  const sources = readdirSync(join(root, "src")).filter((f) => f.endsWith(".cpp"));

  const missing: string[] = [];
  for (const pattern of premake) {
    const re = patternToRe(pattern);
    for (const f of sources) {
      if (!re.test(f) || f in MINGW_EXCLUDED) {
        continue;
      }
      if (!mingw.some((r) => r.test(f))) {
        missing.push(`  src/${f}  (premake pattern "${pattern}")`);
      }
    }
  }
  if (missing.length > 0) {
    throw new Error(
      "these src files are compiled by premake but not by cmd/build-with-mingw.ts,\n" +
        "so the Linux/Wine CI job will fail to link. Add matching patterns to the\n" +
        `dir: "src" group in cmd/build-with-mingw.ts:\n` +
        missing.join("\n"),
    );
  }
  console.log(`PASS: mingw source list covers all ${premake.length} premake src patterns`);
}

if (import.meta.main) {
  try {
    await testit();
  } catch (e) {
    console.error(`\n${(e as Error)?.message ?? e}`);
    process.exit(1);
  }
  process.exit(0);
}
