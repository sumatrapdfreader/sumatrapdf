// build-codeql.ts - replaces Go "-build-codeql" flag
// Just a static 64-bit release build for CodeQL analysis
import { join } from "node:path";
import { detectVisualStudio, runLogged } from "./util";

function buildConfigPath(): string {
  return join("src", "base", "BuildConfig.h");
}

async function revertBuildConfig(): Promise<void> {
  const proc = Bun.spawn(["git", "checkout", buildConfigPath()], {
    stdout: "inherit",
    stderr: "inherit",
  });
  await proc.exited;
}

async function main() {
  const timeStart = performance.now();
  console.log("build-codeql: static 64-bit release build for CodeQL analysis");

  const { msbuildPath } = detectVisualStudio();
  const slnPath = join("vs2022", "SumatraPDF.sln");

  await runLogged(msbuildPath, [slnPath, `/t:SumatraPDF-static:Rebuild`, `/p:Configuration=Release;Platform=x64`, `/m`]);

  await revertBuildConfig();

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`build-codeql finished in ${elapsed}s`);
}

if (import.meta.main) {
  await main();
}
