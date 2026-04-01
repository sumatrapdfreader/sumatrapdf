import { join, resolve } from "node:path";
import { copyFileSync, unlinkSync } from "node:fs";
import { detectVisualStudio2026, runLogged } from "./util";

function tryDelete(path: string) {
  try { unlinkSync(path); } catch {}
}

async function main() {
  const args = process.argv.slice(2);
  if (args.length === 0) {
    console.log("Usage: bun cmd/test-plugin.ts <path-to-pdf-file>");
    process.exit(1);
  }
  const pdfFile = resolve(args[0]);

  const timeStart = performance.now();
  console.log("debug build");

  const { msbuildPath } = detectVisualStudio2026();
  const sln = String.raw`vs2026\SumatraPDF.slnx`;
  const t = `/t:SumatraPDF`;
  const p = `/p:Configuration=Debug;Platform=x64`;
  await runLogged(msbuildPath, [sln, t, p, `/m`]);

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`build took ${elapsed}s`);

  const outDir = join("out", "dbg64");
  const sumatraExe = resolve(join(outDir, "SumatraPDF.exe"));
  const testPluginExe = resolve(join(outDir, "test-plugin.exe"));

  // clean up old log files
  tryDelete("test-plugin.log.txt");
  tryDelete("plugin.log.txt");

  // copy SumatraPDF.exe to test-plugin.exe so they are different executables
  copyFileSync(sumatraExe, testPluginExe);
  console.log(`copied ${sumatraExe} -> ${testPluginExe}`);

  console.log(`running: ${testPluginExe} -test-plugin ${sumatraExe} "${pdfFile}"`);
  await runLogged(testPluginExe, ["-test-plugin", sumatraExe, pdfFile]);
}

await main();
