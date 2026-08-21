import { existsSync, rmSync, readdirSync } from "node:fs";
import { join, resolve } from "node:path";
import { copyFileNormalized } from "./util";

const kCopiedWebsiteFiles = ["sumatra.css", "gen_toc.js", "gen_code_copy.js", "favicon.ico"];

function getWebsiteDir(): string {
  return resolve(join("..", "hack", "webapps", "sumatra-website"));
}

function die(msg: string): never {
  console.error(msg);
  process.exit(1);
}

async function runInDir(dir: string, cmd: string, args: string[], opts?: { pipeStdout?: boolean }): Promise<string> {
  const proc = Bun.spawn([cmd, ...args], {
    cwd: dir,
    stdout: opts?.pipeStdout === false ? "inherit" : "pipe",
    stderr: "inherit",
  });
  let out = "";
  if (opts?.pipeStdout !== false && proc.stdout) {
    out = await new Response(proc.stdout).text();
  }
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    throw new Error(`${cmd} ${args.join(" ")} failed with exit code ${exitCode}`);
  }
  return out;
}

async function runGitInDir(dir: string, ...args: string[]): Promise<string> {
  return runInDir(dir, "git", args);
}

function getCurrentBranch(dir: string): string {
  const proc = Bun.spawnSync(["git", "rev-parse", "--abbrev-ref", "HEAD"], {
    cwd: dir,
    stdout: "pipe",
    stderr: "pipe",
  });
  if (proc.exitCode !== 0) {
    const err = new TextDecoder().decode(proc.stderr).trim();
    die(`git rev-parse failed in '${dir}': ${err || `exit ${proc.exitCode}`}`);
  }
  return new TextDecoder().decode(proc.stdout).trim();
}

function copiedGitPaths(): string[] {
  return ["www/docs-md", ...kCopiedWebsiteFiles.map((n) => `www/${n}`)];
}

function shouldCopyFile(name: string): boolean {
  const bannedSuffixes = [".go", ".bat"];
  for (const s of bannedSuffixes) {
    if (name.endsWith(s)) return false;
  }
  const bannedPrefixes = ["yarn", "go."];
  for (const s of bannedPrefixes) {
    if (name.startsWith(s)) return false;
  }
  const doNotCopy = ["tests"];
  return !doNotCopy.includes(name);
}

function copyFilesRecur(dstDir: string, srcDir: string): void {
  const entries = readdirSync(srcDir, { withFileTypes: true });
  for (const entry of entries) {
    if (!shouldCopyFile(entry.name)) continue;
    const srcPath = join(srcDir, entry.name);
    const dstPath = join(dstDir, entry.name);
    if (entry.isDirectory()) {
      copyFilesRecur(dstPath, srcPath);
      continue;
    }
    copyFileNormalized(dstPath, srcPath);
  }
}

async function copyDocsToWebsite(websiteDir: string): Promise<void> {
  const srcDir = join("docs", "md");
  const dstDir = join(websiteDir, "www", "docs-md");
  rmSync(dstDir, { recursive: true, force: true });
  copyFilesRecur(dstDir, srcDir);
  for (const name of kCopiedWebsiteFiles) {
    copyFileNormalized(join(websiteDir, "www", name), join("docs", name));
  }
  rmSync(join(dstDir, ".obsidian"), { recursive: true, force: true });
}

async function main() {
  console.log("genHTMLDocsForWebsite starting");
  const timeStart = performance.now();

  const websiteDir = getWebsiteDir();
  console.log(`sumatra website dir: '${websiteDir}'`);
  if (!existsSync(websiteDir)) {
    die(`directory for sumatra website '${websiteDir}' doesn't exist`);
  }

  const branch = getCurrentBranch(websiteDir);
  if (branch !== "master") {
    die(`sumatra-website must be on master (currently '${branch}')`);
  }

  const dirty = (await runGitInDir(websiteDir, "status", "--porcelain", "--", ".")).trim();
  if (dirty) {
    die(`sumatra-website must be clean (no uncommitted changes):\n${dirty}`);
  }

  try {
    const pullOut = await runGitInDir(websiteDir, "pull");
    if (pullOut.trim()) {
      console.log(pullOut.trim());
    }
  } catch (e) {
    const msg = e instanceof Error ? e.message : String(e);
    die(`git pull failed: ${msg}`);
  }

  const wwwDir = join(websiteDir, "www");
  if (!existsSync(wwwDir)) {
    die(`directory for sumatra website '${wwwDir}' doesn't exist`);
  }

  await copyDocsToWebsite(websiteDir);

  try {
    await runInDir(websiteDir, "go", ["run", ".", "-check-docs"], { pipeStdout: false });
  } catch (e) {
    const msg = e instanceof Error ? e.message : String(e);
    die(`docs check failed: ${msg}`);
  }

  const paths = copiedGitPaths();
  await runGitInDir(websiteDir, "add", "-A", "--", ...paths);
  const porcelain = (await runGitInDir(websiteDir, "status", "--porcelain", "--", ...paths)).trim();
  if (!porcelain) {
    console.log("no doc changes to commit");
  } else {
    console.log(porcelain);
    await runGitInDir(websiteDir, "commit", "-m", "sumatra-website: update docs");
    await runGitInDir(websiteDir, "push");
    console.log("committed and pushed sumatra-website docs");
  }

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`genHTMLDocsForWebsite finished in ${elapsed}s`);
}

if (import.meta.main) {
  try {
    await main();
  } catch (e) {
    const msg = e instanceof Error ? e.message : String(e);
    die(msg);
  }
}
