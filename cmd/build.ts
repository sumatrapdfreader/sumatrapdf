import { join } from "node:path";
import { detectVisualStudio2026, runLogged } from "./util";
import { clearDirPreserveSettings } from "./clean";

let clean = false;

let t = `/t:SumatraPDF`;
t = `/t:SumatraPDF-dll`;

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

  // const outDir = join("out", "dbg64");
  // await runLogged(resolve(join(outDir, "test_util.exe")), [], outDir);

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`build took ${elapsed}s`);
}

await main();
