// Build and run the release x64 (rel64) build of the native logview tool
// (src/tools/logview). Output binary: out/rel64/logview.exe.
import { join, resolve } from "node:path";
import { detectVisualStudio2026, runLogged } from "./util";
import { launchDetached } from "../tests/winapi";

const config = "Release";
const platform = "x64";
const outDir = join("out", "rel64");
const exe = resolve(join(outDir, "logview.exe"));

async function main(): Promise<void> {
  const { msbuildPath } = detectVisualStudio2026();
  const proj = String.raw`vs2022\logview.vcxproj`;
  const p = `/p:Configuration=${config};Platform=${platform}`;
  await runLogged(msbuildPath, [proj, p, "/m"]);

  // launch fully detached (CreateProcessW) and return immediately: the GUI keeps
  // running after this script exits (a Bun.spawn child would be killed on exit).
  const pid = launchDetached(exe);
  console.log(`launched ${exe} (pid ${pid})`);
}

if (import.meta.main) {
  await main();
}
