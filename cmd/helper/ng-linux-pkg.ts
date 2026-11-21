// What every Linux package of ng shares: building the release, the install
// tree under usr/ and the split-off symbols. cmd/ng-deb.ts, cmd/ng-rpm.ts,
// cmd/ng-arch.ts and cmd/ng-tar.ts wrap it in their format.
//
//   usr/bin/sumatrapdf                         the app, usr/bin/SumatraPDF links to it
//   usr/share/applications/sumatrapdf.desktop  launcher entry and "Open With"
//   usr/share/mime/packages/sumatrapdf.xml     file types the system doesn't know
//   usr/share/icons/hicolor/<n>x<n>/apps/      icons from src/ng/linux/icons
//   usr/share/doc/sumatrapdf/copyright

import {
  chmodSync,
  copyFileSync,
  existsSync,
  mkdirSync,
  readFileSync,
  statSync,
  symlinkSync,
  writeFileSync,
} from "node:fs";
import { join } from "node:path";
import { LINUX_ICON_SIZES, linuxIconName } from "../update-linux-icons";
import { outDir, root, type BuildFlags } from "./ng-compile";
import { defaultTarget } from "./ng-targets";

export const pkgName = "sumatrapdf";
export const binName = "sumatrapdf";
// the name the mac and Windows builds use; keeps `SumatraPDF` working in a shell
const binAlias = defaultTarget;
export const maintainer = "Krzysztof Kowalczyk <kkowalczyk@gmail.com>";
export const homepage = "https://www.sumatrapdfreader.org/";
export const summary = "PDF, ePub, MOBI, CHM, XPS, DjVu, CBZ and CBR viewer";
export const description = [
  "SumatraPDF is a small, fast document viewer. It opens PDF, XPS, DjVu,",
  "ePub, MOBI, FictionBook, CHM, comic book archives and images.",
];
const linuxDir = join(root, "src", "ng", "linux");

export type PkgOptions = { rev: number; clean: boolean; build: boolean };

export class CliError extends Error {}

export function pkgUsage(script: string, what: string): string {
  return [
    `Usage: bun cmd/${script} [-rev <n>] [-clean] [-no-build]`,
    "",
    what,
    "",
    "  -rev <n>    n-th further build of the day: version yy.mm.dd.n",
    "  -clean      delete the output directory first",
    "  -no-build   pack what out/ already holds",
    "  -h | -help  this text",
  ].join("\n");
}

export function parsePkgArgs(args: string[], maxRev: number): PkgOptions {
  const opts: PkgOptions = { rev: 0, clean: false, build: true };
  for (let i = 0; i < args.length; i++) {
    const a = args[i]!;
    if (a === "-rev") {
      opts.rev = Number(args[++i]);
      if (!Number.isInteger(opts.rev) || opts.rev < 1 || opts.rev > maxRev) {
        throw new CliError(`-rev needs a number from 1 to ${maxRev}`);
      }
    } else if (a === "-clean") opts.clean = true;
    else if (a === "-no-build") opts.build = false;
    else if (a === "-h" || a === "-help" || a === "--help") throw new CliError("");
    else throw new CliError(`unknown option: ${a}`);
  }
  return opts;
}

// runs a package script's main: usage on a CliError, exit 1 on any other error
export async function runPkgMain(
  script: string,
  usage: string,
  main: (opts: PkgOptions) => Promise<void>,
  maxRev: number,
) {
  let opts: PkgOptions;
  try {
    opts = parsePkgArgs(process.argv.slice(2), maxRev);
  } catch (e) {
    if (!(e instanceof CliError)) throw e;
    if (e.message) console.error(`${e.message}\n`);
    console.log(usage);
    process.exit(e.message ? 1 : 0);
  }
  try {
    if (process.platform !== "linux") throw new Error("Linux packages are built on Linux");
    await main(opts);
  } catch (err) {
    console.error(`${script} failed: ${err instanceof Error ? err.message : err}`);
    process.exit(1);
  }
}

export function run(cmd: string[], cwd?: string): string {
  console.log(`> ${cmd.join(" ")}`);
  const p = Bun.spawnSync(cmd, { cwd, stdout: "pipe", stderr: "inherit" });
  if (p.exitCode !== 0) throw new Error(`${cmd[0]} failed with exit code ${p.exitCode}`);
  return p.stdout.toString();
}

async function runInherit(cmd: string[]): Promise<void> {
  console.log(`> ${cmd.join(" ")}`);
  const p = Bun.spawn(cmd, { cwd: root, stdout: "inherit", stderr: "inherit" });
  if ((await p.exited) !== 0) throw new Error(`${cmd.join(" ")} failed`);
}

// "ubuntu24.04", "debian13", "fedora45" from /etc/os-release
export function distroTag(): string {
  const rel = readFileSync("/etc/os-release", "utf8");
  const id = /^ID=(.*)$/m.exec(rel)?.[1]?.replaceAll('"', "");
  const ver = /^VERSION_ID=(.*)$/m.exec(rel)?.[1]?.replaceAll('"', "");
  if (!id || !ver) throw new Error("can't read ID / VERSION_ID from /etc/os-release");
  return `${id}${ver}`;
}

// builds the release unless opts.build is false; the path of the app
export async function buildRelease(opts: PkgOptions, staticRuntime = false): Promise<{ out: string; exe: string }> {
  const flags: BuildFlags = {
    debug: false,
    asan: false,
    clang: false,
    clean: opts.clean,
    verbose: false,
    rev: opts.rev,
  };
  const out = outDir("linux", flags);
  const exe = join(out, defaultTarget);
  if (opts.build) {
    const args = ["cmd/ng-build.ts", "-rel", defaultTarget];
    if (opts.clean) args.push("-clean");
    if (opts.rev > 0) args.push("-rev", String(opts.rev));
    if (staticRuntime) args.push("-static");
    await runInherit(["bun", ...args]);
  }
  if (!existsSync(exe)) throw new Error(`missing ${exe}`);
  sharedLibs(exe);
  return { out, exe };
}

// the shared libraries exe loads here, name => path, from ldd. Throws on one
// the loader can't find: the package would not start on this distro
export function sharedLibs(exe: string): Map<string, string> {
  const libs = new Map<string, string>();
  const missing: string[] = [];
  for (const line of run(["ldd", exe]).split("\n")) {
    // libX11.so.6 => /usr/lib/libX11.so.6 (0x...)  or  libfoo.so.1 => not found
    const m = /^\s*(\S+) => (.+?)(?: \(0x[0-9a-f]+\))?$/.exec(line);
    if (!m) continue;
    if (m[2] === "not found") missing.push(m[1]!);
    else libs.set(m[1]!, m[2]!);
  }
  if (missing.length > 0) throw new Error(`${exe} needs libraries not found here: ${missing.join(", ")}`);
  return libs;
}

function copyright(): string {
  return [
    "Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/",
    "Upstream-Name: SumatraPDF",
    `Source: ${homepage}`,
    "",
    "Files: *",
    "Copyright: 2006-2026 Krzysztof Kowalczyk and contributors",
    "License: GPL-3+",
    " SumatraPDF is free software: you can redistribute it and/or modify it under",
    " the terms of the GNU General Public License as published by the Free Software",
    " Foundation, either version 3 of the License, or (at your option) any later",
    " version. See /usr/share/common-licenses/GPL-3. Bundled third-party",
    " libraries carry their own licenses: https://github.com/sumatrapdfreader/sumatrapdf/tree/master/ext",
    "",
  ].join("\n");
}

function install(src: string, dst: string, mode: number): void {
  mkdirSync(join(dst, ".."), { recursive: true });
  copyFileSync(src, dst);
  chmodSync(dst, mode);
}

// separates the symbols into <exe>.debug and strips the staged copy, which
// keeps a debuglink to them: addr2line -e <exe>.debug finds a crash report's offsets
function stageExe(exe: string, staged: string, debug: string): void {
  mkdirSync(join(staged, ".."), { recursive: true });
  run(["objcopy", "--only-keep-debug", exe, debug]);
  run(["objcopy", "--strip-unneeded", exe, staged]);
  run(["objcopy", `--add-gnu-debuglink=${debug}`, staged]);
  chmodSync(staged, 0o755);
  chmodSync(debug, 0o644);
}

// fills <stage>/usr with the install tree and writes the symbols to debug.
// Stage on the local disk: a mounted checkout may not keep modes or symlinks
export function stageUsr(exe: string, stage: string, debug: string): string[] {
  // mkdtemp makes it 0700; the package root must be world-readable
  chmodSync(stage, 0o755);
  const usr = join(stage, "usr");
  const files: string[] = [];
  const add = (src: string, rel: string, mode: number) => {
    install(src, join(stage, rel), mode);
    files.push(rel);
  };
  stageExe(exe, join(usr, "bin", binName), debug);
  files.push(`usr/bin/${binName}`);
  symlinkSync(binName, join(usr, "bin", binAlias));
  files.push(`usr/bin/${binAlias}`);
  add(join(linuxDir, "sumatrapdf.desktop"), "usr/share/applications/sumatrapdf.desktop", 0o644);
  add(join(linuxDir, "sumatrapdf.xml"), "usr/share/mime/packages/sumatrapdf.xml", 0o644);
  for (const px of LINUX_ICON_SIZES) {
    add(join(linuxDir, "icons", linuxIconName(px)), `usr/share/icons/hicolor/${px}x${px}/apps/${binName}.png`, 0o644);
  }
  const docDir = join(usr, "share", "doc", pkgName);
  mkdirSync(docDir, { recursive: true });
  writeFileSync(join(docDir, "copyright"), copyright());
  files.push(`usr/share/doc/${pkgName}/copyright`);
  return files;
}

export function dirSizeKb(dir: string): number {
  let bytes = 0;
  for (const e of new Bun.Glob("**/*").scanSync({ cwd: dir, onlyFiles: true, dot: true })) {
    bytes += statSync(join(dir, e)).size;
  }
  return Math.ceil(bytes / 1024);
}

export function printSizes(paths: string[]): void {
  for (const p of paths) {
    console.log(`${p}: ${statSync(p).size} bytes`);
  }
}
