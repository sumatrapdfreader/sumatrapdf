// An error thrown while form JavaScript runs (here: the console callback) used
// to be rethrown on the JS engine's fz_context. On a render thread that context
// has no fz_try, so the process aborted. Checked with ng's test_mupdf, since
// the app's own console callback never throws.
import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { root as ROOT } from "../cmd/helper/ng-compile";
import { findToolchain, hostPlatform } from "../cmd/helper/ng-toolchain";
import { makePdf } from "./form-js-no-stderr.ts";
import { tmpPath } from "./util.ts";

export async function testit(): Promise<void> {
  const fail = (message: string): never => {
    throw new Error(message);
  };
  const tc = findToolchain(ROOT, hostPlatform(), false, fail);
  const run = (args: string[]) => {
    const proc = Bun.spawnSync(args, { cwd: ROOT, env: { ...process.env, ...tc.env } });
    if (proc.exitCode !== 0) fail(`${args.join(" ")} failed (exit ${proc.exitCode}):\n${proc.stdout}\n${proc.stderr}`);
  };
  run([process.execPath, join(ROOT, "cmd/ng-build.ts"), "-rel", "test_mupdf"]);

  const pdf = tmpPath("form-js-throw.pdf");
  writeFileSync(pdf, makePdf('console.println("x");'));
  const probe = join(ROOT, "out", tc.plat, "rel", tc.msvcStyle ? "test_mupdf.exe" : "test_mupdf");
  run([probe, "-js-throw", pdf]);
}

if (import.meta.main) {
  try {
    await testit();
    console.log("PASS: form-js-throw");
  } catch (error) {
    console.error(error);
    process.exitCode = 1;
  }
}
