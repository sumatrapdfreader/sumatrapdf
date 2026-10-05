// Vendor gpui-kit-cpp-dist into ext/gpui.
//
//   bun cmd/ng-update-gpui.ts              # latest main -> ext/gpui, records the commit
//   bun cmd/ng-update-gpui.ts -commit <sha>
//   bun cmd/ng-update-gpui.ts -examples    # also keep the clone (examples, assets) in out/gpui-dist
//
// The clone lands in out/gpui-dist (gitignored). Only the files a build needs
// are copied into ext/gpui: gpui.h, gpui.cpp, quickjs/, web/shell.html,
// mac-window-place.m, readme.md, plus VERSION.txt with the dist commit
// (not "VERSION": ext/gpui is an include dir and libc++ has a <version>).

import { cpSync, existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";

const repoUrl = "https://github.com/kjk/gpui-kit-cpp-dist";
const root = resolve(import.meta.dir, "..");
const cloneDir = join(root, "out", "gpui-dist");
const extDir = join(root, "ext", "gpui");

const vendoredFiles = ["gpui.h", "gpui.cpp", "mac-window-place.m", "readme.md"];
const vendoredDirs = ["quickjs", "web"];

function run(cmd: string[], cwd?: string): string {
  const r = Bun.spawnSync(cmd, { cwd, stdout: "pipe", stderr: "pipe" });
  if (r.exitCode !== 0) {
    console.error(`failed: ${cmd.join(" ")}\n${r.stderr.toString()}`);
    process.exit(1);
  }
  return r.stdout.toString().trim();
}

function parseArgs(args: string[]): { commit: string | null; keepExamples: boolean } {
  let commit: string | null = null;
  let keepExamples = false;
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a === "-commit") {
      commit = args[++i] ?? null;
      if (!commit) {
        console.error("-commit needs a sha");
        process.exit(1);
      }
    } else if (a === "-examples") {
      keepExamples = true;
    } else {
      console.error(`unknown option: ${a}\nUsage: bun cmd/ng-update-gpui.ts [-commit <sha>] [-examples]`);
      process.exit(1);
    }
  }
  return { commit, keepExamples };
}

function clone(commit: string | null): void {
  rmSync(cloneDir, { recursive: true, force: true });
  mkdirSync(join(root, "out"), { recursive: true });
  if (commit) {
    run(["git", "clone", "-q", "--filter=blob:none", repoUrl, cloneDir]);
    run(["git", "checkout", "-q", commit], cloneDir);
    return;
  }
  run(["git", "clone", "-q", "--depth", "1", repoUrl, cloneDir]);
}

function vendor(): string {
  const sha = run(["git", "rev-parse", "HEAD"], cloneDir);
  const date = run(["git", "log", "-1", "--format=%cd", "--date=short"], cloneDir);
  rmSync(extDir, { recursive: true, force: true });
  mkdirSync(extDir, { recursive: true });
  for (const f of vendoredFiles) {
    cpSync(join(cloneDir, f), join(extDir, f));
  }
  for (const d of vendoredDirs) {
    cpSync(join(cloneDir, d), join(extDir, d), { recursive: true });
  }
  const version = [
    "gpui-kit-cpp-dist",
    `commit ${sha}`,
    `date ${date}`,
    `${repoUrl}/commit/${sha}`,
    "vendored by cmd/ng-update-gpui.ts; do not edit files here by hand",
    "",
  ].join("\n");
  writeFileSync(join(extDir, "VERSION.txt"), version);
  return sha;
}

function main(): void {
  const { commit, keepExamples } = parseArgs(process.argv.slice(2));
  console.log(`cloning ${repoUrl}${commit ? ` at ${commit}` : ""}`);
  clone(commit);
  const sha = vendor();
  console.log(`vendored ext/gpui at ${sha}`);
  if (!keepExamples) {
    rmSync(cloneDir, { recursive: true, force: true });
    return;
  }
  console.log(`examples and assets kept in ${cloneDir}`);
  if (!existsSync(join(cloneDir, "examples"))) {
    console.error("clone has no examples directory");
  }
}

main();
