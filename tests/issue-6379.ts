// issue #6379: auto-reloading a modified .md file scrolled it back to the top.
// The same path restores a .md from the session, which also lost the position.
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const TARGET_DEST_NO = 3;
const MIN_SCROLL_Y = 500;
const kWaitMs = 15000;

// TestMarkdownTocNavigate until it stops reporting NOTREADY
async function mdRequest(client: ControlClient, destNo: number, minScrollY: number, want: string): Promise<string> {
  const deadline = Date.now() + kWaitMs;
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkdownTocNavigate, [destNo, minScrollY]);
    const out = String(res[1] ?? "").trim();
    if (res[0] === 0 && out.startsWith(want)) {
      return out;
    }
    if (res[0] !== 2 || Date.now() > deadline) {
      throw new Error(`issue-6379: markdown navigation failed: ${out}`);
    }
    await sleep(100);
  }
}

async function scrollY(client: ControlClient): Promise<number> {
  const deadline = Date.now() + kWaitMs;
  let last = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkdownTocNavigate, [0, 0]);
    last = String(res[1] ?? "").trim();
    const m = /^OK scrollX=\d+ scrollY=(-?\d+)/.exec(last);
    if (res[0] === 0 && m && Number(m[1]) >= 0) {
      return Number(m[1]);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6379: no scroll position reported: ${last}`);
    }
    await sleep(80);
  }
}

function makeMarkdown(extra: string): string {
  const paragraphs = Array.from({ length: 200 }, (_, i) => `Paragraph ${i + 1}: enough text to overflow the window.`);
  return ["# Start", ...paragraphs, "## Target Heading", extra].join("\n\n");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6379");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const md = join(dir, "long.md");
  writeFileSync(md, makeMarkdown("v1"));
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      "MarkdownUI [",
      "\tUseFixedPageUI = false",
      "]",
      "ReloadModifiedDocuments = true",
      "RestoreSession = false",
      "ShowStartPage = false",
      "",
    ].join("\n"),
  );
  const logPath = join(dir, "app.log");

  const { proc, client } = await launchControlled(["-appdata", appdata, "-log-to-file", logPath, md]);
  try {
    await client.waitForRenderIdle();
    // jump to a heading far down; the first navigation must not land in a 0-size view
    await mdRequest(client, 0, 0, "OK");
    await mdRequest(client, TARGET_DEST_NO, MIN_SCROLL_Y, "NAVIGATING");
    await mdRequest(client, 0, MIN_SCROLL_Y, "OK");
    const before = await scrollY(client);

    // an editor saves the file: wait for the auto-reload to finish
    writeFileSync(md, makeMarkdown("v2"));
    const reloadDeadline = Date.now() + kWaitMs;
    while (!/ReloadDocument: (.* )?reloaded in /.test(readFileSync(logPath, "utf8"))) {
      if (Date.now() > reloadDeadline) {
        throw new Error("issue-6379: modified file was not reloaded");
      }
      await sleep(80);
    }
    await client.waitForRenderIdle();

    // the reloaded page restores its scroll once the document completes
    let after = 0;
    const restoreDeadline = Date.now() + 5000;
    while (Date.now() < restoreDeadline) {
      after = await scrollY(client);
      if (Math.abs(after - before) <= 2) {
        break;
      }
      await sleep(80);
    }
    if (Math.abs(after - before) > 2) {
      throw new Error(`issue-6379: reload lost the scroll position (scrollY ${before} -> ${after})`);
    }
    console.log(`issue-6379: scrollY ${before} kept across reload`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("issue-6379: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
