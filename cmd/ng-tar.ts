// Builds the ng release and packs it as a .tar.xz for distributions with no
// package of their own (openSUSE, ...). libstdc++ and libgcc are linked
// statically; build it on the oldest supported glibc (Ubuntu 22.04) so it
// runs on newer ones. The GUI libraries stay shared.
//
//   bun cmd/ng-tar.ts [-rev <n>] [-clean] [-no-build]
//
// Output, in out/<linux out dir>/rel/:
//   sumatrapdf-<ver>-linux-x86_64.tar.xz        sumatrapdf-<ver>/usr/... : run usr/bin/sumatrapdf
//   sumatrapdf-<ver>-linux-x86_64.debug         its symbols, for crash reports

import { mkdirSync, mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import {
  buildRelease,
  pkgName,
  pkgUsage,
  printSizes,
  run,
  runPkgMain,
  stageUsr,
  type PkgOptions,
} from "./helper/ng-linux-pkg";
import { maxRev, ngVersion } from "./helper/ng-version";

async function main(opts: PkgOptions): Promise<void> {
  const { out, exe } = await buildRelease(opts, true);
  const ver = ngVersion(opts.rev).ver;
  const arch = run(["uname", "-m"]).trim();
  const stem = `${pkgName}-${ver}-linux-${arch}`;
  const tar = join(out, `${stem}.tar.xz`);
  const debug = join(out, `${stem}.debug`);

  const tmp = mkdtempSync(join(tmpdir(), "sumatrapdf-tar-"));
  try {
    const top = `${pkgName}-${ver}`;
    const stage = join(tmp, top);
    mkdirSync(stage);
    stageUsr(exe, stage, debug);
    rmSync(tar, { force: true });
    run(["tar", "-cJf", tar, "--owner=0", "--group=0", "-C", tmp, top]);
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }
  printSizes([tar, debug]);
}

const usage = pkgUsage("ng-tar.ts", "Builds the ng release and packs it as a portable .tar.xz.");
await runPkgMain("ng-tar", usage, main, maxRev);
