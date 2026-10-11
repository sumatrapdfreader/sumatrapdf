// Builds the ng release and packs it as a .deb for the Ubuntu / Debian it
// runs on. build-ng-linux.ts (in ../hack) runs it inside a container per
// Ubuntu release; it also works on an Ubuntu host or in WSL.
//
//   bun cmd/ng-deb.ts [-rev <n>] [-clean] [-no-build]
//
// Output, in out/<linux out dir>/rel/:
//   sumatrapdf_<ver>~ubuntu24.04_amd64.deb      the package
//   sumatrapdf_<ver>~ubuntu24.04_amd64.debug    its symbols, for crash reports
//
// The package installs:
//   /usr/bin/sumatrapdf                         the app, /usr/bin/SumatraPDF links to it
//   /usr/share/applications/sumatrapdf.desktop  launcher entry and "Open With"
//   /usr/share/mime/packages/sumatrapdf.xml     file types the system doesn't know
//   /usr/share/icons/hicolor/<n>x<n>/apps/      icons from src/ng/linux/icons
//   /usr/share/doc/sumatrapdf/copyright

import {
  chmodSync,
  copyFileSync,
  existsSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  rmSync,
  statSync,
  symlinkSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { basename, join } from "node:path";
import { outDir, root, type BuildFlags } from "./helper/ng-compile";
import { defaultTarget } from "./helper/ng-targets";
import { maxRev, ngVersion } from "./helper/ng-version";
import { LINUX_ICON_SIZES, linuxIconName } from "./update-linux-icons";

const pkgName = "sumatrapdf";
const binName = "sumatrapdf";
// the name the mac and Windows builds use; keeps `SumatraPDF` working in a shell
const binAlias = defaultTarget;
const maintainer = "Krzysztof Kowalczyk <kkowalczyk@gmail.com>";
const homepage = "https://www.sumatrapdfreader.org/";
const linuxDir = join(root, "src", "ng", "linux");

type Options = { rev: number; clean: boolean; build: boolean };

class CliError extends Error {}

function usage(): string {
  return [
    "Usage: bun cmd/ng-deb.ts [-rev <n>] [-clean] [-no-build]",
    "",
    "Builds the ng release and packs it as a .deb for this Ubuntu / Debian.",
    "",
    `  -rev <n>    n-th further build of the day: version yy.mm.dd.n (1-${maxRev})`,
    "  -clean      delete the output directory first",
    "  -no-build   pack what out/ already holds",
    "  -h | -help  this text",
  ].join("\n");
}

function parseArgs(args: string[]): Options {
  const opts: Options = { rev: 0, clean: false, build: true };
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

function run(cmd: string[], cwd?: string): string {
  console.log(`> ${cmd.join(" ")}`);
  const p = Bun.spawnSync(cmd, { cwd, stdout: "pipe", stderr: "inherit" });
  if (p.exitCode !== 0) throw new Error(`${cmd[0]} failed with exit code ${p.exitCode}`);
  return p.stdout.toString();
}

async function runInherit(cmd: string[], env?: Record<string, string>): Promise<void> {
  console.log(`> ${cmd.join(" ")}`);
  const p = Bun.spawn(cmd, { cwd: root, stdout: "inherit", stderr: "inherit", env: { ...process.env, ...env } });
  if ((await p.exited) !== 0) throw new Error(`${cmd.join(" ")} failed`);
}

// "ubuntu24.04" from /etc/os-release; Debian's VERSION_ID is "13"
function distroTag(): string {
  const rel = readFileSync("/etc/os-release", "utf8");
  const id = /^ID=(.*)$/m.exec(rel)?.[1]?.replaceAll('"', "");
  const ver = /^VERSION_ID=(.*)$/m.exec(rel)?.[1]?.replaceAll('"', "");
  if (!id || !ver) throw new Error("can't read ID / VERSION_ID from /etc/os-release");
  return `${id}${ver}`;
}

// dpkg-shlibdeps names the packages providing each linked library, with the
// minimal versions (libssl3 on 22.04, libssl3t64 on 24.04). It insists on a
// debian/control in the working directory.
function shlibDepends(exe: string): string {
  const dir = mkdtempSync(join(tmpdir(), "shlibdeps-"));
  try {
    mkdirSync(join(dir, "debian"));
    writeFileSync(join(dir, "debian", "control"), `Source: ${pkgName}\n\nPackage: ${pkgName}\nArchitecture: any\n`);
    const out = run(["dpkg-shlibdeps", "-O", "-e", exe], dir);
    const m = /^shlibs:Depends=(.*)$/m.exec(out);
    if (!m) throw new Error(`no shlibs:Depends in dpkg-shlibdeps output:\n${out}`);
    return m[1]!.trim();
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
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

function control(ver: string, arch: string, depends: string, installedKb: number): string {
  return [
    `Package: ${pkgName}`,
    `Version: ${ver}`,
    "Section: text",
    "Priority: optional",
    `Architecture: ${arch}`,
    `Maintainer: ${maintainer}`,
    `Installed-Size: ${installedKb}`,
    `Depends: ${depends}`,
    "Recommends: xdg-utils",
    `Homepage: ${homepage}`,
    "Description: PDF, ePub, MOBI, CHM, XPS, DjVu, CBZ and CBR viewer",
    " SumatraPDF is a small, fast document viewer. It opens PDF, XPS, DjVu,",
    " ePub, MOBI, FictionBook, CHM, comic book archives and images.",
    "",
  ].join("\n");
}

function dirSizeKb(dir: string): number {
  let bytes = 0;
  for (const e of new Bun.Glob("**/*").scanSync({ cwd: dir, onlyFiles: true, dot: true })) {
    bytes += statSync(join(dir, e)).size;
  }
  return Math.ceil(bytes / 1024);
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

async function main(): Promise<void> {
  let opts: Options;
  try {
    opts = parseArgs(process.argv.slice(2));
  } catch (e) {
    if (!(e instanceof CliError)) throw e;
    if (e.message) console.error(`${e.message}\n`);
    console.log(usage());
    process.exit(e.message ? 1 : 0);
  }
  if (process.platform !== "linux") throw new Error("a .deb is built on Linux");

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
    await runInherit(["bun", ...args]);
  }
  if (!existsSync(exe)) throw new Error(`missing ${exe}`);

  const ver = `${ngVersion(opts.rev).ver}~${distroTag()}`;
  const arch = run(["dpkg", "--print-architecture"]).trim();
  const stem = `${pkgName}_${ver}_${arch}`;
  const deb = join(out, `${stem}.deb`);
  const debug = join(out, `${stem}.debug`);

  // staged on the local disk: a mounted checkout may not keep modes or symlinks
  const stage = mkdtempSync(join(tmpdir(), "sumatrapdf-deb-"));
  try {
    // mkdtemp makes it 0700; the package root must be world-readable
    chmodSync(stage, 0o755);
    const usr = join(stage, "usr");
    stageExe(exe, join(usr, "bin", binName), debug);
    symlinkSync(binName, join(usr, "bin", binAlias));
    install(join(linuxDir, "sumatrapdf.desktop"), join(usr, "share", "applications", "sumatrapdf.desktop"), 0o644);
    install(join(linuxDir, "sumatrapdf.xml"), join(usr, "share", "mime", "packages", "sumatrapdf.xml"), 0o644);
    for (const px of LINUX_ICON_SIZES) {
      const dst = join(usr, "share", "icons", "hicolor", `${px}x${px}`, "apps", `${binName}.png`);
      install(join(linuxDir, "icons", linuxIconName(px)), dst, 0o644);
    }
    const docDir = join(usr, "share", "doc", pkgName);
    mkdirSync(docDir, { recursive: true });
    writeFileSync(join(docDir, "copyright"), copyright());

    const depends = shlibDepends(join(usr, "bin", binName));
    console.log(`Depends: ${depends}`);
    mkdirSync(join(stage, "DEBIAN"));
    writeFileSync(join(stage, "DEBIAN", "control"), control(ver, arch, depends, dirSizeKb(usr)));

    rmSync(deb, { force: true });
    run(["dpkg-deb", "--build", "--root-owner-group", "-Zxz", stage, deb]);
  } finally {
    rmSync(stage, { recursive: true, force: true });
  }

  console.log(run(["dpkg-deb", "--info", deb]));
  for (const p of [deb, debug]) {
    console.log(`${basename(p)}: ${statSync(p).size} bytes`);
  }
}

try {
  await main();
} catch (err) {
  console.error(`ng-deb failed: ${err instanceof Error ? err.message : err}`);
  process.exit(1);
}
