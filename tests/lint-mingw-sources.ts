// Lint: cmd/helper/mingw-build.ts must compile the same src/*.cpp files as premake.
//
// The Linux/Wine CI job cross-compiles with mingw-w64 using its own copy of the
// source list in cmd/helper/mingw-build.ts, which "replicates build logic from
// premake5.lua / premake5.files.lua". Adding a new src file to premake (and the
// .vcxproj) but not to that list builds fine on Windows and then fails the CI
// job at link time with undefined references -- which is how ScreenshotCapture,
// ImageEditHostSumatra and ShortcutParse each broke master in turn.
//
// This compares the src patterns of premake's sumatrapdf_files() against the
// mingw list and fails on any src/*.cpp premake compiles that mingw doesn't.
//
// Run standalone: bun tests/lint-mingw-sources.ts  (also runs in run-almost-all.ts)

import { readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";

// files the Windows build compiles that mingw deliberately leaves out. Keep this
// list tiny and say why: each entry is a file the cross-compile cannot build.
const MINGW_EXCLUDED: Record<string, string> = {
  "TextToSpeech.cpp": "WinRT (windows.media.speechsynthesis.h) / SAPI, not in mingw-w64; stubbed",
  "TestPlugin.cpp": "doesn't build with mingw GDI+; stubbed",
  "TestPreview.cpp": "doesn't build with mingw GDI+; stubbed",
  "BasePch.cpp": "MSVC precompiled header source",
  "MuPDF_Exports.cpp": "the libmupdf DLL's export table, not part of the exe",
};

// the dirs the vcxproj check covers: where SumatraPDF's own UI/app code lives.
// src/gui/win and src/uia are already wildcarded in the mingw list.
const VCXPROJ_DIRS = ["src", "src/gui"];

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

// the patterns cmd/helper/mingw-build.ts compiles for `dir`
function mingwPatterns(root: string, dir: string): string[] {
  const ts = readFileSync(join(root, "cmd", "helper", "mingw-build.ts"), "utf-8");
  const out: string[] = [];
  const re = new RegExp(`dir:\\s*"${dir}",\\s*patterns:\\s*\\[([^\\]]*)\\]`, "g");
  for (const group of ts.matchAll(re)) {
    for (const q of group[1].matchAll(/"([^"]+)"/g)) {
      out.push(q[1]);
    }
  }
  if (out.length === 0) {
    throw new Error(`lint-mingw-sources: no dir: "${dir}" pattern groups found in cmd/helper/mingw-build.ts`);
  }
  return out;
}

// the .cpp files SumatraPDF.vcxproj compiles that live directly in `dir`. That
// project is the source of truth for what has to link: premake is regenerated
// from it by hand and drifts (it was missing DarkMode_win.cpp when that file
// broke the Linux job), so checking only premake -> mingw can pass while the
// cross-compile is missing a file.
function vcxprojSources(root: string, dir: string): string[] {
  const vcx = readFileSync(join(root, "vs2022", "SumatraPDF.vcxproj"), "utf-8");
  const out: string[] = [];
  for (const m of vcx.matchAll(/<ClCompile Include="\.\.\\([^"]+)"/g)) {
    const rel = m[1].replace(/\\/g, "/");
    if (!rel.startsWith(`${dir}/`) || !rel.endsWith(".cpp")) {
      continue;
    }
    const name = rel.slice(dir.length + 1);
    if (!name.includes("/")) {
      out.push(name);
    }
  }
  if (out.length === 0) {
    throw new Error(`lint-mingw-sources: no ClCompile entries for ${dir} in vs2022/SumatraPDF.vcxproj`);
  }
  return out;
}

export async function testit(): Promise<void> {
  const root = join(import.meta.dir, "..");
  const premake = premakeSrcPatterns(root);
  const mingwSrc = mingwPatterns(root, "src").map(patternToRe);
  const sources = readdirSync(join(root, "src")).filter((f) => f.endsWith(".cpp"));

  const missing: string[] = [];
  for (const pattern of premake) {
    const re = patternToRe(pattern);
    for (const f of sources) {
      if (!re.test(f) || f in MINGW_EXCLUDED) {
        continue;
      }
      if (!mingwSrc.some((r) => r.test(f))) {
        missing.push(`  src/${f}  (premake pattern "${pattern}")`);
      }
    }
  }

  // and the same against the vcxproj, which is what actually has to link
  let nChecked = 0;
  for (const dir of VCXPROJ_DIRS) {
    const mingw = mingwPatterns(root, dir).map(patternToRe);
    for (const f of vcxprojSources(root, dir)) {
      nChecked++;
      if (f in MINGW_EXCLUDED || mingw.some((r) => r.test(f))) {
        continue;
      }
      missing.push(`  ${dir}/${f}  (in vs2022/SumatraPDF.vcxproj)`);
    }
  }

  if (missing.length > 0) {
    throw new Error(
      "these files are compiled on Windows but not by cmd/helper/mingw-build.ts,\n" +
        "so the Linux/Wine CI job will fail to link. Add matching patterns to the\n" +
        "corresponding dir group in cmd/helper/mingw-build.ts (and to premake5.files.lua),\n" +
        "or to MINGW_EXCLUDED in this file if the cross-compile genuinely can't build them:\n" +
        missing.join("\n"),
    );
  }
  console.log(`PASS: mingw source list covers ${premake.length} premake src patterns and ${nChecked} vcxproj sources`);
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
