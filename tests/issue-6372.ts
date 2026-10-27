// Ctrl+A in Merge PDF selects every page. The dialog is owned by the frame,
// so the frame's accelerators used to turn it into the document's Select All.
//
// Run: bun tests/issue-6372.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { cmdId, makePdf, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const VK_A = 0x41;

async function merge(client: ControlClient, action = ""): Promise<{ code: number; raw: string }> {
  const res = await client.request(ControlCommand.TestMergePdf, [action, "", 0]);
  return { code: Number(res[0]), raw: String(res[1] ?? "") };
}

function itemsOf(raw: string): string {
  return /items=(\S*)/.exec(raw)?.[1] ?? "";
}

async function waitFor(what: string, f: () => Promise<boolean>) {
  const deadline = Date.now() + 5000;
  while (!(await f())) {
    if (Date.now() > deadline) {
      throw new Error(`issue-6372: ${what}`);
    }
    await sleep(100);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6372");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(3, 601, 0, 0), "latin1");

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommand(frame, cmdId("CmdMergePDF"));
    await waitFor("the dialog didn't open", async () => (await merge(client)).code === 0);

    if (USE_NG) {
      // the key needs the window's first frame: until it's drawn gpui has no
      // key listener to call
      await waitFor("the dialog's window didn't draw", async () => {
        const r = await client.request(ControlCommand.TestToolWindow, ["layout", "mergepdf", "", 0, 0, 0, 0]);
        return String(r[1] ?? "").includes("hit rect");
      });
      // gpui's key flags: 1 Ctrl, 16 Command
      const r = await client.request(ControlCommand.TestToolWindow, [
        "input",
        "mergepdf",
        "key",
        VK_A,
        IS_MAC ? 16 : 1,
        0,
        0,
      ]);
      const res = String(r[1] ?? "");
      if (res !== "OK eaten=1") {
        throw new Error(`issue-6372: Ctrl+A not handled: ${res}`);
      }
    } else {
      await merge(client, "ctrla");
    }
    const want = "0:1s,0:2s,0:3s";
    let raw = "";
    await waitFor(`Ctrl+A didn't select all pages, want items=${want}`, async () => {
      raw = (await merge(client)).raw;
      return itemsOf(raw) === want;
    }).catch((e) => {
      throw new Error(`${e.message}: ${raw}`);
    });
    console.log("issue-6372: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
