// Merge PDF: the legend under the pages names every added file, however many
// there are and however long their names: it wraps instead of being cut off.
//
// Run: bun tests/issue-6397.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, makePdf, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const kFiles = 7;

async function merge(client: ControlClient, action = "", arg = "", n = 0): Promise<string> {
  const res = await client.request(ControlCommand.TestMergePdf, [action, arg, n]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0 || !raw.startsWith("OK")) {
    throw new Error(`issue-6397: TestMergePdf ${action}: ${raw}`);
  }
  return raw;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6397");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const paths: string[] = [];
  for (let i = 0; i < kFiles; i++) {
    const path = join(dir, `a-rather-long-file-name-for-the-merge-legend-${i}.pdf`);
    writeFileSync(path, makePdf(1, 601 + i, 0, 0), "latin1");
    paths.push(path);
  }

  const { proc, client, frame } = await launchControlled([paths[0]!]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommand(frame, cmdId("CmdMergePDF"));
    const deadline = Date.now() + 5000;
    while (!(await client.request(ControlCommand.TestMergePdf, ["", "", 0]))[1]?.toString().startsWith("OK")) {
      if (Date.now() > deadline) {
        throw new Error("issue-6397: the dialog didn't open");
      }
      await sleep(100);
    }
    for (const path of paths.slice(1)) {
      await merge(client, "add", path, -1);
    }

    // the layout follows the added files on the next paint
    let raw = "";
    let m: RegExpExecArray | null = null;
    const until = Date.now() + 5000;
    while (Date.now() < until) {
      raw = await merge(client);
      m = /legend=(\d+)\/(\d+)/.exec(raw);
      if (m && m[1] === m[2] && +m[2]! === kFiles) {
        break;
      }
      await sleep(100);
    }
    if (!m || +m[2]! !== kFiles || m[1] !== m[2]) {
      throw new Error(`issue-6397: legend shows ${m?.[1]} of ${m?.[2]} files, want ${kFiles}: ${raw}`);
    }
    // the pages keep room above the legend
    if (!/rects=\d+:/.test(raw)) {
      throw new Error(`issue-6397: no page is visible: ${raw}`);
    }
    await merge(client, "close");
    console.log("issue-6397: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
