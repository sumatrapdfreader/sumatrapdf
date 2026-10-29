// #6435: a PDF opened from Outlook (its Content.Outlook temp folder) is loaded
// from a private copy in our open-cache (#4705). The copy must keep the
// attachment's name, so Save As suggests it, instead of "1.pdf". Closing the
// tab deletes the copy and the folder it was put in.
//
// Run: bun tests/issue-6435.ts [--no-build]
import { existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { basename, dirname, join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, makeMinimalPdf, runStandalone, tmpPath, USE_NG } from "./util";
import { sleep } from "./winapi";

const NAME = "Report Q3.pdf";

async function currentTabPath(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 10000;
  for (;;) {
    const res = await client.request(ControlCommand.TestCurrentTab, []);
    const m = /^path=(.*) page=\d+$/.exec(String(res[1] ?? "").trim());
    if (res[0] === 0 && m) {
      return m[1]!;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6435: no document loaded: ${res[1]}`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  if (USE_NG) {
    console.log("SKIP issue-6435: ng opens host temp files in place, without the #4705 copy");
    return;
  }
  const root = tmpPath("issue-6435");
  rmSync(root, { recursive: true, force: true });
  const outlookDir = join(root, "Content.Outlook", "AB12CD");
  mkdirSync(outlookDir, { recursive: true });
  const attachment = join(outlookDir, NAME);
  writeFileSync(attachment, makeMinimalPdf("attachment"));

  await withControlledSumatra(
    EXE,
    async (client) => {
      const path = await currentTabPath(client);
      if (!/[\\/]open-cache[\\/]/i.test(path)) {
        throw new Error(`issue-6435: the attachment should load from the open-cache copy, got ${path}`);
      }
      if (basename(path) !== NAME) {
        throw new Error(`issue-6435: the copy should be named '${NAME}', got '${basename(path)}' (${path})`);
      }
      if (!existsSync(path)) {
        throw new Error(`issue-6435: the copy doesn't exist: ${path}`);
      }

      await client.request(ControlCommand.TestInvokeCommand, ["CmdClose"]);
      const deadline = Date.now() + 5000;
      while ((existsSync(path) || existsSync(dirname(path))) && Date.now() < deadline) {
        await sleep(50);
      }
      if (existsSync(path)) {
        throw new Error(`issue-6435: closing the tab should delete the copy ${path}`);
      }
      if (existsSync(dirname(path))) {
        throw new Error(`issue-6435: closing the tab should delete the copy's folder ${dirname(path)}`);
      }
    },
    [attachment],
  );
  console.log("issue-6435: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
