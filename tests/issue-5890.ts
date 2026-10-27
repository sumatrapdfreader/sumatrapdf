// Merge PDF: Ctrl+C / Ctrl+X / Ctrl+V on pages, Ctrl+V of PDF files copied
// in a file manager (whatever was copied last wins) and the count of selected
// pages in the status. A pasted copy of a page is rotated on its own.
// Discussion #5890.
//
// Run: bun tests/issue-5890.ts [--no-build]

import { spawnSync } from "node:child_process";
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, makePdf, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

async function merge(client: ControlClient, action = "", arg = "", n = 0): Promise<string> {
  const res = await client.request(ControlCommand.TestMergePdf, [action, arg, n]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`issue-5890: TestMergePdf ${action}: ${raw}`);
  }
  return raw;
}

function itemsOf(raw: string): string {
  return /items=(\S*)/.exec(raw)?.[1] ?? "";
}

function infoOf(raw: string): string {
  return /info="([^"]*)"/.exec(raw)?.[1] ?? "";
}

function expectItems(raw: string, want: string, what: string) {
  if (itemsOf(raw) !== want) {
    throw new Error(`issue-5890: ${what}: want items=${want}: ${raw}`);
  }
}

async function waitFor(what: string, f: () => Promise<boolean>) {
  const deadline = Date.now() + 5000;
  while (!(await f())) {
    if (Date.now() > deadline) {
      throw new Error(`issue-5890: ${what}`);
    }
    await sleep(100);
  }
}

async function widths(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestPageInfo, []);
  return /widths=([\d,]+)/.exec(String(res[1] ?? ""))?.[1] ?? "";
}

// what Explorer's Copy puts on the clipboard
function copyFileToClipboard(path: string) {
  const r = spawnSync("powershell", ["-NoProfile", "-Command", `Set-Clipboard -Path '${path}'`], {
    encoding: "utf8",
    timeout: 20000,
  });
  if (r.status !== 0) {
    throw new Error(`issue-5890: Set-Clipboard failed: ${r.stderr}`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5890");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(4, 601, 0, 0), "latin1");
  const other = join(dir, "other.pdf");
  writeFileSync(other, makePdf(2, 701, 0, 0), "latin1");
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

    await merge(client, "copy", "0,1");
    let raw = await merge(client, "paste", "3");
    expectItems(raw, "0:1,0:2,0:3,0:4,0:1s,0:2s", "copy, paste");
    if (infoOf(raw) !== "6 pages, 2 selected") {
      throw new Error(`issue-5890: the status: ${raw}`);
    }

    raw = await merge(client, "cut", "0");
    expectItems(raw, "0:2,0:3,0:4,0:1,0:2", "cut");
    if (infoOf(raw) !== "5 pages") {
      throw new Error(`issue-5890: the status after cut: ${raw}`);
    }
    raw = await merge(client, "paste", "4");
    expectItems(raw, "0:2,0:3,0:4,0:1,0:2,0:1s", "cut, paste");
    await merge(client, "rotate", "5", 90);

    // copied after the pages: the file is pasted
    copyFileToClipboard(other);
    raw = await merge(client, "paste", "0");
    expectItems(raw, "0:2,1:1s,1:2s,0:3,0:4,0:1,0:2,0:1@90", "paste a file");

    const res = await client.request(ControlCommand.TestMergePdf, ["saveas", merged, 0]);
    if (String(res[1] ?? "") !== "OK saved=1") {
      throw new Error(`issue-5890: saveas: ${res[1]}`);
    }
    const want = "602,701,702,603,604,601,602,792";
    await waitFor(`the merged PDF didn't open with widths ${want}`, async () => (await widths(client)) === want);
    console.log("issue-5890: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
