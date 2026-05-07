import { $ } from "bun";
import { existsSync, mkdirSync, readdirSync, statSync, writeFileSync, copyFileSync, rmSync } from "node:fs";
import { join, basename, dirname } from "node:path";
import { tmpdir } from "node:os";
import { spawn } from "node:child_process";

const winMergePaths = [
  String.raw`C:\Program Files\WinMerge\WinMergeU.exe`,
  String.raw`C:\Users\kjk\AppData\Local\Programs\WinMerge\WinMergeU.exe`,
];

function detectWinMerge(): string {
  for (const p of winMergePaths) {
    if (existsSync(p)) {
      return p;
    }
  }
  return "WinMergeU.exe";
}

interface GitChange {
  type: "modified" | "added" | "deleted";
  path: string;
  name: string;
}

function parseGitStatusLine(line: string): GitChange | null {
  const trimmed = line.trim();
  if (!trimmed) return null;
  const spaceIdx = trimmed.indexOf(" ");
  if (spaceIdx === -1) return null;
  const status = trimmed.substring(0, spaceIdx);
  const path = trimmed.substring(spaceIdx + 1).trim();
  const name = basename(path);
  switch (status) {
    case "M":
      return { type: "modified", path, name };
    case "A":
      return { type: "added", path, name };
    case "D":
      return { type: "deleted", path, name };
    case "RM":
      return null;
    default:
      return null;
  }
}

async function gitStatus(): Promise<GitChange[]> {
  const result = await $`git status --porcelain`.text();
  const lines = result.split("\n");
  const changes: GitChange[] = [];
  for (const line of lines) {
    const c = parseGitStatusLine(line);
    if (c) {
      changes.push(c);
    }
  }
  return changes;
}

async function gitGetFileContentHead(path: string): Promise<Buffer> {
  return Buffer.from(await $`git show ${"HEAD:" + path}`.arrayBuffer());
}

function createEmptyFile(path: string): void {
  writeFileSync(path, "");
}

async function copyFileChange(dirBefore: string, dirAfter: string, change: GitChange): Promise<void> {
  switch (change.type) {
    case "added": {
      createEmptyFile(join(dirBefore, change.name));
      copyFileSync(change.path, join(dirAfter, change.name));
      break;
    }
    case "deleted": {
      createEmptyFile(join(dirAfter, change.name));
      const content = await gitGetFileContentHead(change.path);
      writeFileSync(join(dirBefore, change.name), content);
      break;
    }
    case "modified": {
      copyFileSync(change.path, join(dirAfter, change.name));
      const content = await gitGetFileContentHead(change.path);
      writeFileSync(join(dirBefore, change.name), content);
      break;
    }
  }
}

function deleteOldDirs(tempBase: string): void {
  if (!existsSync(tempBase)) return;
  const oneDayMs = 24 * 60 * 60 * 1000;
  const now = Date.now();
  for (const entry of readdirSync(tempBase, { withFileTypes: true })) {
    if (!entry.isDirectory()) continue;
    const fullPath = join(tempBase, entry.name);
    const stat = statSync(fullPath);
    const age = now - stat.mtimeMs;
    if (age > oneDayMs) {
      console.log(`Deleting ${fullPath} because older than 1 day`);
      rmSync(fullPath, { recursive: true, force: true });
    } else {
      console.log(`Not deleting ${fullPath} because younger than 1 day`);
    }
  }
}

function cdToGitRoot(): void {
  let dir = process.cwd();
  while (true) {
    if (existsSync(join(dir, ".git"))) {
      if (dir !== process.cwd()) {
        console.log(`Changed current dir to: '${dir}'`);
        process.chdir(dir);
      }
      return;
    }
    const parent = dirname(dir);
    if (parent === dir) {
      throw new Error("Not in a git repository");
    }
    dir = parent;
  }
}

async function main() {
  const winMergePath = detectWinMerge();
  console.log(`WinMerge: ${winMergePath}`);

  const tempBase = join(tmpdir(), "sum-diff-preview");
  mkdirSync(tempBase, { recursive: true });
  console.log(`temp dir: ${tempBase}`);
  deleteOldDirs(tempBase);

  cdToGitRoot();
  const changes = await gitStatus();
  if (changes.length === 0) {
    console.log("No changes to preview!");
    process.exit(0);
  }
  console.log(`${changes.length} change(s)`);

  const now = new Date();
  const pad = (n: number) => n.toString().padStart(2, "0");
  const subDir = `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())}_${pad(now.getHours())}_${pad(now.getMinutes())}_${pad(now.getSeconds())}`;
  const dir = join(tempBase, subDir);
  const dirBefore = join(dir, "before");
  const dirAfter = join(dir, "after");
  mkdirSync(dirBefore, { recursive: true });
  mkdirSync(dirAfter, { recursive: true });

  for (const change of changes) {
    await copyFileChange(dirBefore, dirAfter, change);
  }
  console.log("launching WinMerge:", winMergePath);
  const proc = spawn(winMergePath, ["/u", "/wl", "/wr", dirBefore, dirAfter], {
    detached: true,
    stdio: "ignore",
  });
  proc.on("error", (err) => {
    console.error(`failed to launch WinMerge: ${err.message}`);
    process.exit(1);
  });
  proc.unref();
}

await main();
