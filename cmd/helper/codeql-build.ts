// CodeQL implementation used by cmd/build.ts -codeql.
// Just a static 64-bit release build for CodeQL analysis
import { join } from "node:path";
import { detectVisualStudio, runLogged } from "../util";

export async function buildCodeql() {
  const timeStart = performance.now();
  console.log("build-codeql: static 64-bit release build for CodeQL analysis");

  // the prebuild packs .work/docs into IDR_EMBEDDED_PAK; generate the manual first (same as build-ci).
  const { main: genDocs } = await import("../gen-docs");
  await genDocs();

  const { msbuildPath } = detectVisualStudio();
  const slnPath = join("vs2022", "SumatraPDF.sln");

  await runLogged(msbuildPath, [
    slnPath,
    `/t:SumatraPDF-static:Rebuild`,
    `/p:Configuration=Release;Platform=x64`,
    `/m`,
  ]);

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`build-codeql finished in ${elapsed}s`);
}
