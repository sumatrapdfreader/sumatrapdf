import { join } from "node:path";
import { detectVisualStudio2026, runLogged } from "./util";
import { clearDirPreserveSettings } from "./clean";

let clean = false;
let config = "Debug";
let platform = "x64";
let outDir = join("out", "dbg64");

let t = `/t:SumatraPDF`;

for (const arg of process.argv.slice(2)) {
  if (arg === "-clean") {
    clean = true;
  } else if (arg === "-rel-32") {
    config = "Release";
    platform = "Win32";
    outDir = join("out", "rel32");
  } else {
    throw new Error(`unknown argument: ${arg}`);
  }
}

async function main() {
  const timeStart = performance.now();

  console.log(`${config} ${platform} build`);
  if (clean) {
    const dirs = [outDir];
    for (const dir of dirs) {
      clearDirPreserveSettings(dir);
    }
  }

  const { msbuildPath } = detectVisualStudio2026();
  const sln = String.raw`vs2022\SumatraPDF.sln`;
  // const t = `/t:SumatraPDF;test_util`;
  const p = `/p:Configuration=${config};Platform=${platform}`;
  await runLogged(msbuildPath, [sln, t, p, `/m`]);

  // await runLogged(resolve(join(outDir, "test_util.exe")), [], outDir);

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`build took ${elapsed}s`);
}

await main();
