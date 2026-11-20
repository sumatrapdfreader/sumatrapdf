// Builds the ng release and packs it as a .deb for the Ubuntu / Debian it
// runs on. The ng-linux-release workflow runs it in a container per release;
// it also works on an Ubuntu host or in WSL.
//
//   bun cmd/ng-deb.ts [-rev <n>] [-clean] [-no-build]
//
// Output, in out/<linux out dir>/rel/:
//   sumatrapdf_<ver>~ubuntu24.04_amd64.deb      the package
//   sumatrapdf_<ver>~ubuntu24.04_amd64.debug    its symbols, for crash reports
//
// The install tree is described in cmd/helper/ng-linux-pkg.ts.

import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import {
  buildRelease,
  description,
  dirSizeKb,
  distroTag,
  homepage,
  maintainer,
  pkgName,
  pkgUsage,
  printSizes,
  run,
  runPkgMain,
  stageUsr,
  summary,
  binName,
  type PkgOptions,
} from "./helper/ng-linux-pkg";
import { maxRev, ngVersion } from "./helper/ng-version";

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
    `Description: ${summary}`,
    ...description.map((l) => ` ${l}`),
    "",
  ].join("\n");
}

async function main(opts: PkgOptions): Promise<void> {
  const { out, exe } = await buildRelease(opts);

  const ver = `${ngVersion(opts.rev).ver}~${distroTag()}`;
  const arch = run(["dpkg", "--print-architecture"]).trim();
  const stem = `${pkgName}_${ver}_${arch}`;
  const deb = join(out, `${stem}.deb`);
  const debug = join(out, `${stem}.debug`);

  const stage = mkdtempSync(join(tmpdir(), "sumatrapdf-deb-"));
  try {
    stageUsr(exe, stage, debug);
    const usr = join(stage, "usr");
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
  printSizes([deb, debug]);
}

const usage = pkgUsage("ng-deb.ts", "Builds the ng release and packs it as a .deb for this Ubuntu / Debian.");
await runPkgMain("ng-deb", usage, main, maxRev);
