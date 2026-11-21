// Builds the ng release and packs it as an Arch Linux package with makepkg.
// The ng-linux-release workflow runs it in an archlinux container.
//
//   bun cmd/ng-arch.ts [-rev <n>] [-clean] [-no-build]
//
// Output, in out/<linux out dir>/rel/:
//   sumatrapdf-<ver>-1-x86_64.pkg.tar.zst       the package
//   sumatrapdf-<ver>-1-x86_64.debug             its symbols, for crash reports
//
// The install tree is described in cmd/helper/ng-linux-pkg.ts. depends= lists
// the packages owning the libraries the app links. makepkg refuses to run as
// root, so in a container it runs as nobody.

import { copyFileSync, mkdirSync, mkdtempSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import {
  buildRelease,
  homepage,
  pkgName,
  pkgUsage,
  printSizes,
  run,
  runPkgMain,
  sharedLibs,
  stageUsr,
  summary,
  type PkgOptions,
} from "./helper/ng-linux-pkg";
import { maxRev, ngVersion } from "./helper/ng-version";

const pkgRel = 1;

// the packages owning the libraries exe links directly (NEEDED), e.g. libx11, cairo
function pacmanDepends(exe: string): string[] {
  const libs = sharedLibs(exe);
  const needed = [...run(["objdump", "-p", exe]).matchAll(/^\s*NEEDED\s+(\S+)$/gm)].map((m) => m[1]!);
  const paths = needed.map((name) => {
    const p = libs.get(name);
    if (!p) throw new Error(`ldd didn't resolve ${name}`);
    return p;
  });
  const owners = run(["pacman", "-Qoq", ...paths])
    .split("\n")
    .filter(Boolean);
  return [...new Set(owners)].sort();
}

// installs the already staged, already stripped tree
function pkgbuild(ver: string, arch: string, depends: string[], stage: string): string {
  return [
    `pkgname=${pkgName}`,
    `pkgver=${ver}`,
    `pkgrel=${pkgRel}`,
    `pkgdesc="${summary}"`,
    `arch=('${arch}')`,
    `url="${homepage}"`,
    "license=('GPL-3.0-or-later')",
    `depends=(${depends.map((d) => `'${d}'`).join(" ")})`,
    "optdepends=('xdg-utils: open links and folders')",
    "options=('!strip' '!debug')",
    "",
    "package() {",
    `  cp -a '${stage}/.' "$pkgdir/"`,
    "}",
    "",
  ].join("\n");
}

async function main(opts: PkgOptions): Promise<void> {
  const { out, exe } = await buildRelease(opts);
  const ver = ngVersion(opts.rev).ver;
  const arch = run(["uname", "-m"]).trim();
  const depends = pacmanDepends(exe);
  console.log(`depends: ${depends.join(" ")}`);

  const tmp = mkdtempSync(join(tmpdir(), "sumatrapdf-arch-"));
  try {
    const stage = join(tmp, "stage");
    mkdirSync(stage);
    const debugTmp = join(tmp, "sumatrapdf.debug");
    stageUsr(exe, stage, debugTmp);
    const build = join(tmp, "build");
    mkdirSync(build);
    writeFileSync(join(build, "PKGBUILD"), pkgbuild(ver, arch, depends, stage));

    // --nodeps: depends= names the libraries' packages, already installed
    let cmd = ["makepkg", "--nodeps", "--noconfirm"];
    if (process.getuid?.() === 0) {
      run(["chown", "-R", "nobody:nobody", tmp]);
      cmd = ["runuser", "-u", "nobody", "--", ...cmd];
    }
    run(cmd, build);

    const pkgs = readdirSync(build).filter((f) => f.endsWith(".pkg.tar.zst"));
    if (pkgs.length !== 1) throw new Error(`expected one package in ${build}, found: ${pkgs.join(", ")}`);
    const name = pkgs[0]!;
    const pkg = join(out, name);
    const debug = join(out, name.replace(/\.pkg\.tar\.zst$/, ".debug"));
    copyFileSync(join(build, name), pkg);
    copyFileSync(debugTmp, debug);
    console.log(run(["pacman", "-Qip", pkg]));
    printSizes([pkg, debug]);
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }
}

const usage = pkgUsage("ng-arch.ts", "Builds the ng release and packs it as an Arch Linux package.");
await runPkgMain("ng-arch", usage, main, maxRev);
