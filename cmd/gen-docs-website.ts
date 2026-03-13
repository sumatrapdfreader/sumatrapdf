import {
  existsSync,
  rmSync,
  readdirSync,
  statSync,
  readFileSync,
  writeFileSync,
  mkdirSync,
  copyFileSync,
} from "node:fs";
import { join, resolve, extname } from "node:path";
import { $ } from "bun";
import { isGitClean } from "./util";

function getWebsiteDir(): string {
  return resolve(join("..", "hack", "webapps", "sumatra-website"));
}

async function runGitInDir(dir: string, ...args: string[]): Promise<string> {
  const proc = Bun.spawn(["git", ...args], {
    cwd: dir,
    stdout: "pipe",
    stderr: "inherit",
  });
  const exitCode = await proc.exited;
  const out = await new Response(proc.stdout).text();
  if (exitCode !== 0) {
    throw new Error(`git ${args.join(" ")} failed with exit code ${exitCode}`);
  }
  return out;
}

function getCurrentBranch(dir: string): string {
  const proc = Bun.spawnSync(["git", "branch"], {
    cwd: dir,
    stdout: "pipe",
    stderr: "inherit",
  });
  const s = new TextDecoder().decode(proc.stdout);
  if (s.includes("(HEAD detached")) {
    return "master";
  }
  for (const line of s.split("\n")) {
    const l = line.trim();
    if (l.startsWith("* ")) {
      return l.slice(2);
    }
  }
  return "";
}

async function updateSumatraWebsite(): Promise<string> {
  const dir = getWebsiteDir();
  console.log(`sumatra website dir: '${dir}'`);
  if (!existsSync(dir)) {
    throw new Error(`directory for sumatra website '${dir}' doesn't exist`);
  }
  if (!(await isGitClean(dir))) {
    throw new Error(`github repository '${dir}' must be clean`);
  }
  if (getCurrentBranch(dir) !== "master") {
    throw new Error(`github repository '${dir}' must be on master branch`);
  }
  // git pull
  const pullOut = await runGitInDir(dir, "pull");
  if (pullOut.trim()) {
    console.log(pullOut.trim());
  }
  const wwwDir = join(dir, "www");
  if (!existsSync(wwwDir)) {
    throw new Error(`directory for sumatra website '${wwwDir}' doesn't exist`);
  }
  return wwwDir;
}

function normalizeNewlines(data: string): string {
  return data.replace(/\r\n/g, "\n").replace(/\r/g, "\n");
}

const extsToNormalizeNL = new Set([".md", ".css"]);

function shouldCopyFile(name: string, isDir: boolean): boolean {
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

function copyFileNormalized(dst: string, src: string): void {
  const dstDir = join(dst, "..");
  mkdirSync(dstDir, { recursive: true });
  const ext = extname(dst).toLowerCase();
  if (extsToNormalizeNL.has(ext)) {
    const data = readFileSync(src, "utf-8");
    writeFileSync(dst, normalizeNewlines(data));
  } else {
    copyFileSync(src, dst);
  }
}

function copyFilesRecur(dstDir: string, srcDir: string): void {
  const entries = readdirSync(srcDir, { withFileTypes: true });
  for (const entry of entries) {
    if (!shouldCopyFile(entry.name, entry.isDirectory())) continue;
    const srcPath = join(srcDir, entry.name);
    const dstPath = join(dstDir, entry.name);
    if (entry.isDirectory()) {
      copyFilesRecur(dstPath, srcPath);
      continue;
    }
    copyFileNormalized(dstPath, srcPath);
  }
}

async function main() {
  console.log("genHTMLDocsForWebsite starting");
  const timeStart = performance.now();

  const wwwDir = await updateSumatraWebsite();
  const currBranch = getCurrentBranch(wwwDir);
  if (currBranch !== "master") {
    throw new Error(`expected master branch, got '${currBranch}'`);
  }

  // copy docs/md to website/www/docs-md
  const srcDir = join("docs", "md");
  const websiteDir = getWebsiteDir();
  const dstDir = join(websiteDir, "www", "docs-md");
  rmSync(dstDir, { recursive: true, force: true });
  copyFilesRecur(dstDir, srcDir);

  {
    const dst = join(dstDir, "SumatraPDF-documentation.md");
    const src = join(dstDir, "SumatraPDF-documentation-website.md");
    copyFileNormalized(dst, src);
    rmSync(src);
  }

  // copy CSS and JS files
  const cssJsFiles = ["notion.css", "sumatra.css", "gen_toc.js"];
  for (const name of cssJsFiles) {
    const srcPath = join("docs", "www", name);
    const dstPath = join(websiteDir, "www", name);
    copyFileNormalized(dstPath, srcPath);
  }

  // copy search files
  const searchFiles = ["gen_docs.search.js", "gen_docs.search.html"];
  for (const name of searchFiles) {
    const srcPath = join("cmd", "html-helpers", name);
    const dstPath = join(websiteDir, "www", name);
    copyFileNormalized(dstPath, srcPath);
  }

  // remove .obsidian directory
  const obsidianDir = join(dstDir, ".obsidian");
  rmSync(obsidianDir, { recursive: true, force: true });

  // show git status
  const statusOut = await runGitInDir(websiteDir, "status");
  console.log(`\n${statusOut}`);

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`genHTMLDocsForWebsite finished in ${elapsed}s`);
}

if (import.meta.main) {
  await main();
}
