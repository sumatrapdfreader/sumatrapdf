import { spawn } from "node:child_process";
import { join } from "node:path";
import { detectVisualStudio2026, runLogged } from "./util";
import { clearDirPreserveSettings } from "./clean";

let clean = false;

let t = `/t:SumatraPDF`;

async function main() {
  const timeStart = performance.now();

  console.log("debug build");
  if (clean) {
    const dirs = [join("out", "dbg64")];
    for (const dir of dirs) {
      clearDirPreserveSettings(dir);
    }
  }

  const { msbuildPath } = detectVisualStudio2026();
  const sln = String.raw`vs2022\SumatraPDF.sln`;
  // const t = `/t:SumatraPDF;test_util`;
  const p = `/p:Configuration=Debug;Platform=x64`;
  await runLogged(msbuildPath, [sln, t, p, `/m`]);
  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`build took ${elapsed}s`);

  const path = join("out", "dbg64", "SumatraPDF.exe");
  // launch fully detached so SumatraPDF keeps running after this script exits.
  // Bun.spawn isn't enough here: it kills its children when the parent exits, so
  // unref() alone would let the script return but immediately kill SumatraPDF.
  // node:child_process with { detached: true } + unref() truly detaches it.
  const proc = spawn(path, [], { cwd: ".", detached: true, stdio: "ignore" });
  proc.unref();
}

await main();
