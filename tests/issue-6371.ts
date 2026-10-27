// Merge PDF rotates the selected pages: Rotate Right twice is 180, Rotate Left
// of an unrotated page 270. The saved PDF has each page's /Rotate; a page
// turned by 90 is as wide as it was tall (792).
//
// Run: bun tests/issue-6371.ts [--no-build]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, makePdf, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

async function merge(client: ControlClient, action = "", arg = "", n = 0): Promise<string> {
  const res = await client.request(ControlCommand.TestMergePdf, [action, arg, n]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`issue-6371: TestMergePdf ${action}: ${raw}`);
  }
  return raw;
}

function itemsOf(raw: string): string {
  return /items=(\S*)/.exec(raw)?.[1] ?? "";
}

async function waitFor(what: string, f: () => Promise<boolean>) {
  const deadline = Date.now() + 5000;
  while (!(await f())) {
    if (Date.now() > deadline) {
      throw new Error(`issue-6371: ${what}`);
    }
    await sleep(100);
  }
}

async function widths(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestPageInfo, []);
  return /widths=([\d,]+)/.exec(String(res[1] ?? ""))?.[1] ?? "";
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6371");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(4, 601, 0, 0), "latin1");
  const merged = join(dir, "merged.pdf");

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommand(frame, cmdId("CmdMergePDF"));
    await waitFor("the dialog didn't open", async () => {
      const res = await client.request(ControlCommand.TestMergePdf, ["", "", 0]);
      return res[0] === 0;
    });

    await merge(client, "rotate", "0,2", 90);
    await merge(client, "rotate", "2", 90);
    const raw = await merge(client, "rotate", "1", -90);
    const want = "0:1@90,0:2@270s,0:3@180,0:4";
    if (itemsOf(raw) !== want) {
      throw new Error(`issue-6371: want items=${want}: ${raw}`);
    }

    // a full turn is no rotation
    await merge(client, "rotate", "3", 90);
    await merge(client, "rotate", "3", 270);
    const raw2 = await merge(client);
    if (itemsOf(raw2) !== "0:1@90,0:2@270,0:3@180,0:4s") {
      throw new Error(`issue-6371: a full turn: ${raw2}`);
    }

    const res = await client.request(ControlCommand.TestMergePdf, ["saveas", merged, 0]);
    if (String(res[1] ?? "") !== "OK saved=1") {
      throw new Error(`issue-6371: saveas: ${res[1]}`);
    }
    await waitFor("the merged PDF didn't open", async () => (await widths(client)) === "792,792,603,604");
    const data = readFileSync(merged, "latin1");
    for (const deg of [90, 180, 270]) {
      if (!new RegExp(`/Rotate ${deg}\\b`).test(data)) {
        throw new Error(`issue-6371: no /Rotate ${deg} in the saved PDF`);
      }
    }
    console.log("issue-6371: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
