import { copyFileSync, existsSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { detectVisualStudio2026, runLogged } from "./util";
import { clearDirPreserveSettings } from "./clean";

const asanDllName = "clang_rt.asan_dynamic-x86_64.dll";

function findAsanDll(vsRoot: string): string {
  const candidates = [join(vsRoot, String.raw`VC\Tools\MSVC`), join(vsRoot, String.raw`VC\Tools\Llvm\x64\lib\clang`)];
  for (const base of candidates) {
    if (!existsSync(base)) continue;
    const walk = (dir: string): string | undefined => {
      for (const name of readdirSync(dir, { withFileTypes: true })) {
        const p = join(dir, name.name);
        if (name.isDirectory()) {
          const hit = walk(p);
          if (hit) return hit;
        } else if (name.name === asanDllName) {
          return p;
        }
      }
      return;
    };
    const hit = walk(base);
    if (hit) return hit;
  }
  throw new Error(`could not find ${asanDllName} under ${vsRoot}`);
}

function copyAsanRuntime(vsRoot: string, outDir: string): void {
  const src = findAsanDll(vsRoot);
  const dst = join(outDir, asanDllName);
  copyFileSync(src, dst);
  console.log(`copied ${asanDllName}`);
}

// Build SumatraPDF-static.exe with MSVC AddressSanitizer (x64_asan).
// Output: out/dbg64_asan/SumatraPDF-static.exe or out/rel64_asan/SumatraPDF-static.exe
//
// Usage:
//   bun cmd/build-asan.ts            # Debug ASan build
//   bun cmd/build-asan.ts  -rel  # Release ASan build
//   bun cmd/build-asan.ts -clean

let argv = process.argv;
const isRelease = argv.includes("-release") || argv.includes("-rel");
const clean = argv.includes("--clean");
const config = isRelease ? "Release" : "Debug";
const outDir = isRelease ? join("out", "rel64_asan") : join("out", "dbg64_asan");

async function main() {
  const timeStart = performance.now();
  console.log(`${config} ASan build (SumatraPDF-static.exe, x64_asan)`);

  if (clean) {
    clearDirPreserveSettings(outDir);
  }

  await runLogged(join("bin", "premake5.exe"), ["vs2022"]);

  const { msbuildPath, vsRoot } = detectVisualStudio2026();
  const sln = String.raw`vs2022\SumatraPDF.sln`;
  const t = `/t:SumatraPDF-static`;
  const p = `/p:Configuration=${config};Platform=x64_asan`;
  await runLogged(msbuildPath, [sln, t, p, `/m`]);
  copyAsanRuntime(vsRoot, outDir);

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`build took ${elapsed}s`);
  console.log(`exe: ${join(outDir, "SumatraPDF-static.exe")}`);
}

await main();
