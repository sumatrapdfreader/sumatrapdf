// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5780
//
// Ctrl+Shift+Right / Ctrl+Shift+Left (CmdOpenNextFileInFolder /
// CmdOpenPrevFileInFolder) stopped working: DirIter built its FindFirstFileW
// pattern with a plain string join ("C:\dir*" instead of "C:\dir\*"), so it
// enumerated zero files and folder navigation silently did nothing.
//
// Run: bun tests/issue-5780.ts [--no-build]

import { mkdirSync, copyFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const SRC_PDF = join(import.meta.dir, "issue-3219.pdf");

async function tabPath(client: { request: Function }): Promise<string> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`issue-5780: current tab: ${raw.trim()}`);
  }
  const m = /^path=(.+?) page=/.exec(raw.trim());
  if (!m) {
    throw new Error(`issue-5780: could not parse: ${raw}`);
  }
  return m[1]!.replaceAll("/", "\\").toLowerCase();
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5780");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  copyFileSync(SRC_PDF, join(dir, "aaa.pdf"));
  copyFileSync(SRC_PDF, join(dir, "bbb.pdf"));
  copyFileSync(SRC_PDF, join(dir, "ccc.pdf"));

  const { proc, client } = await launchControlled([join(dir, "aaa.pdf")]);
  try {
    await client.waitForRenderIdle();
    if (!(await tabPath(client)).endsWith("aaa.pdf")) {
      throw new Error(`issue-5780: did not open aaa.pdf`);
    }

    const waitPath = async (suffix: string) => {
      const deadline = Date.now() + 8000;
      let got = "";
      while (Date.now() < deadline) {
        try {
          got = await tabPath(client);
          if (got.endsWith(suffix)) {
            return;
          }
        } catch {
          // loading
        }
        await new Promise((r) => setTimeout(r, 50));
      }
      throw new Error(`issue-5780: timed out waiting for ${suffix} (last=${got})`);
    };

    await client.request(ControlCommand.TestInvokeCommand, ["CmdOpenNextFileInFolder"]);
    await waitPath("bbb.pdf");

    await client.request(ControlCommand.TestInvokeCommand, ["CmdOpenNextFileInFolder"]);
    await waitPath("ccc.pdf");

    await client.request(ControlCommand.TestInvokeCommand, ["CmdOpenPrevFileInFolder"]);
    await waitPath("bbb.pdf");
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
