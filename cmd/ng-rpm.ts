// Builds the ng release and packs it as an .rpm for the Fedora it runs on.
// The ng-linux-release workflow runs it in a container per release.
//
//   bun cmd/ng-rpm.ts [-rev <n>] [-clean] [-no-build]
//
// Output, in out/<linux out dir>/rel/:
//   sumatrapdf-<ver>-1.fc45.x86_64.rpm          the package
//   sumatrapdf-<ver>-1.fc45.x86_64.debug        its symbols, for crash reports
//
// The install tree is described in cmd/helper/ng-linux-pkg.ts. rpmbuild finds
// the library dependencies itself.

import { copyFileSync, mkdirSync, mkdtempSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import {
  buildRelease,
  description,
  homepage,
  maintainer,
  pkgName,
  pkgUsage,
  printSizes,
  run,
  runPkgMain,
  stageUsr,
  summary,
  type PkgOptions,
} from "./helper/ng-linux-pkg";
import { maxRev, ngVersion } from "./helper/ng-version";

// installs the already staged, already stripped tree: no debuginfo
// subpackage and no post-install strip
function spec(ver: string, stage: string, files: string[]): string {
  return [
    "%global debug_package %{nil}",
    "%global __os_install_post %{nil}",
    "%global _build_id_links none",
    `Name: ${pkgName}`,
    `Version: ${ver}`,
    "Release: 1%{?dist}",
    `Summary: ${summary}`,
    "License: GPL-3.0-or-later",
    `URL: ${homepage}`,
    `Packager: ${maintainer}`,
    "Recommends: xdg-utils",
    "",
    "%description",
    ...description,
    "",
    "%install",
    `cp -a ${stage}/. %{buildroot}/`,
    "",
    "%files",
    ...files.map((f) => `/${f}`),
    "",
  ].join("\n");
}

async function main(opts: PkgOptions): Promise<void> {
  const { out, exe } = await buildRelease(opts);
  const ver = ngVersion(opts.rev).ver;

  const tmp = mkdtempSync(join(tmpdir(), "sumatrapdf-rpm-"));
  try {
    const stage = join(tmp, "stage");
    mkdirSync(stage);
    const debugTmp = join(tmp, "sumatrapdf.debug");
    const files = stageUsr(exe, stage, debugTmp);
    const top = join(tmp, "rpmbuild");
    const specPath = join(tmp, `${pkgName}.spec`);
    writeFileSync(specPath, spec(ver, stage, files));
    run(["rpmbuild", "-bb", "--define", `_topdir ${top}`, specPath]);

    // RPMS/<arch>/sumatrapdf-<ver>-1.fc45.<arch>.rpm
    const rpmsDir = join(top, "RPMS");
    const rpms = readdirSync(rpmsDir, { recursive: true, encoding: "utf8" }).filter((f) => f.endsWith(".rpm"));
    if (rpms.length !== 1) throw new Error(`expected one .rpm in ${rpmsDir}, found: ${rpms.join(", ")}`);
    const name = rpms[0]!.split("/").at(-1)!;
    const rpm = join(out, name);
    const debug = join(out, name.replace(/\.rpm$/, ".debug"));
    copyFileSync(join(rpmsDir, rpms[0]!), rpm);
    copyFileSync(debugTmp, debug);
    console.log(run(["rpm", "-qpi", "--requires", rpm]));
    printSizes([rpm, debug]);
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }
}

const usage = pkgUsage("ng-rpm.ts", "Builds the ng release and packs it as an .rpm for this Fedora.");
await runPkgMain("ng-rpm", usage, main, maxRev);
