import { writeFileSync, statSync } from "node:fs";
import { join } from "node:path";
import { $ } from "bun";
import {
  getGitLinearVersion,
  extractSumatraVersion,
  runLogged,
  isGitClean,
  getGitSha1,
  detectVisualStudio2026,
} from "./util";

//const { msbuildPath } = detectVisualStudio();
//const slnPath = join("vs2022", "SumatraPDF.sln");

const { msbuildPath } = detectVisualStudio2026();
const slnPath = join("vs2022", "SumatraPDF.sln");

const platforms = [
  { vsplatform: "ARM64", suffix: "arm64", outDir: join("out", "arm64") },
  { vsplatform: "Win32", suffix: "32", outDir: join("out", "rel32") },
  { vsplatform: "x64", suffix: "64", outDir: join("out", "rel64") },
];

async function isGithubMyMasterBranch(): Promise<boolean> {
  const repo = process.env["GITHUB_REPOSITORY"] ?? "";
  if (!repo) {
    const branch = (await $`git branch --show-current`.text()).trim();
    return branch === "master";
  }
  if (repo !== "sumatrapdfreader/sumatrapdf") return false;
  const ref = process.env["GITHUB_REF"] ?? "";
  if (ref !== "refs/heads/master") {
    console.log(`GITHUB_REF: '${ref}'`);
    return false;
  }
  const event = process.env["GITHUB_EVENT_NAME"] ?? "";
  return event === "push" || event === "repository_dispatch";
}

function buildConfigPath(): string {
  return join("src", "base", "BuildConfig.h");
}

function setBuildConfigPreRelease(sha1: string, preRelVer: string): void {
  const todayDate = new Date().toISOString().slice(0, 10);
  let s = `#define GIT_COMMIT_ID ${sha1}\n`;
  s += `#define BUILT_ON ${todayDate}\n`;
  s += `#define PRE_RELEASE_VER ${preRelVer}\n`;
  writeFileSync(buildConfigPath(), s, "utf-8");
}

async function revertBuildConfig(): Promise<void> {
  await $`git checkout ${buildConfigPath()}`;
}

function ensureManualIsBuilt(): void {
  const path = join("docs", "manual.dat");
  let size = 0;
  try {
    size = statSync(path).size;
  } catch {
    // file doesn't exist
  }
  if (size < 2 * 2024) {
    throw new Error(
      `size of '${path}' is ${size} which indicates we didn't build it`,
    );
  }
}

async function main() {
  if (!(await isGithubMyMasterBranch())) {
    console.log("buildCiDaily: skipping build because not on master branch");
    return;
  }
  if (!(await isGitClean("."))) {
    console.log("buildCiDaily: skipping build because git is not clean");
    return;
  }

  const preRelVer = String(await getGitLinearVersion());
  const sha1 = await getGitSha1();
  const sumatraVer = extractSumatraVersion();
  console.log(`preReleaseVer: '${preRelVer}'`);
  console.log(`gitSha1: '${sha1}'`);
  console.log(`sumatraVersion: '${sumatraVer}'`);
  console.log(`building unsigned pre-release version ${preRelVer}`);

  // generate HTML docs
  const { main: genDocs } = await import("./gen-docs");
  await genDocs();
  ensureManualIsBuilt();

  setBuildConfigPreRelease(sha1, preRelVer);

  const allStart = performance.now();
  try {
    for (const plat of platforms) {
      const platStart = performance.now();
      console.log(
        `buidling pre-release ${plat.vsplatform} version ${preRelVer}`,
      );
      const p = `/p:Configuration=Release;Platform=${plat.vsplatform}`;
      const t = `/t:SumatraPDF;SumatraPDF-static`;
      await runLogged(msbuildPath, [slnPath, t, p, `/m`]);
      const platElapsed = ((performance.now() - platStart) / 1000).toFixed(1);
      console.log(
        `buidling pre-release ${plat.vsplatform} version ${preRelVer} took ${platElapsed}s`,
      );
    }
  } finally {
    await revertBuildConfig();
  }
  const allElapsed = ((performance.now() - allStart) / 1000).toFixed(1);
  console.log(`all builds took ${allElapsed}s`);
}

await main();
