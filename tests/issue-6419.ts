// issue #6419: after switching to an HTML tab, arrow / Page / Home / End keys
// didn't scroll it until the page was clicked. The frame keeps the focus and
// forwards those keys to the WebView2 host window, which Chromium ignores (the
// same as the wheel in #6142), so the keys must scroll the page via script.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { USE_NG, cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { WM_KEYDOWN, postMessage, sleep } from "./winapi.ts";

const VK_NEXT = 0x22;
const VK_END = 0x23;
const VK_HOME = 0x24;
const VK_UP = 0x26;
const VK_DOWN = 0x28;

async function scrollY(client: ControlClient): Promise<number> {
  const deadline = Date.now() + 8000;
  let last = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkdownTocNavigate, [0, 0]);
    last = String(res[1] ?? "").trim();
    const m = /^OK scrollX=\d+ scrollY=(-?\d+)/.exec(last);
    if (res[0] === 0 && m && Number(m[1]) >= 0) {
      return Number(m[1]);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6419: html never reported scroll: ${last}`);
    }
    await sleep(80);
  }
}

// post key to the frame (where the keyboard focus is) and wait for scrollY to
// satisfy ok
async function keyScrolls(
  client: ControlClient,
  frame: number,
  key: number,
  name: string,
  ok: (before: number, after: number) => boolean,
): Promise<void> {
  const before = await scrollY(client);
  postMessage(frame, WM_KEYDOWN, key, 0);
  const deadline = Date.now() + 3000;
  let after = before;
  while (Date.now() < deadline) {
    after = await scrollY(client);
    if (ok(before, after)) {
      console.log(`issue-6419: ${name} scrolled ${before} -> ${after}`);
      return;
    }
    await sleep(80);
  }
  throw new Error(`issue-6419: ${name} did not scroll the html tab (scrollY ${before} -> ${after})`);
}

export async function testit(): Promise<void> {
  if (IS_MAC || USE_NG) {
    console.log("issue-6419: skipped, ng routes keys to its browser view itself");
    return;
  }
  const dir = tmpPath("issue-6419");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const paragraphs = Array.from({ length: 120 }, (_, i) => `<p>Paragraph ${i + 1}: enough text to overflow.</p>`);
  const longHtml = join(dir, "long.html");
  writeFileSync(longHtml, `<!doctype html><html><body><h1>Start</h1>${paragraphs.join("\n")}</body></html>`);
  const otherHtml = join(dir, "other.html");
  writeFileSync(otherHtml, "<!doctype html><html><body><p>other tab</p></body></html>");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), "RestoreSession = false\nShowStartPage = false\n");

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, longHtml, otherHtml]);
  try {
    await client.waitForRenderIdle();
    // away from the long page and back, the way the report gets there
    sendCommand(frame, cmdId("CmdPrevTab"));
    await client.waitForRenderIdle();
    await scrollY(client);

    await keyScrolls(client, frame, VK_DOWN, "Down", (b, a) => a > b);
    await keyScrolls(client, frame, VK_NEXT, "Page Down", (b, a) => a > b + 100);
    await keyScrolls(client, frame, VK_UP, "Up", (b, a) => a < b);
    await keyScrolls(client, frame, VK_END, "End", (b, a) => a > b + 500);
    await keyScrolls(client, frame, VK_HOME, "Home", (_b, a) => a === 0);
    console.log("issue-6419: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
  rmSync(dir, { recursive: true, force: true });
}

if (import.meta.main) {
  await runStandalone(testit);
}
