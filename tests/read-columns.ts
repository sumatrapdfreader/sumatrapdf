// View > Read Columns reads a two-column page down its left column, then its
// right one, then the next page's left column; scrolling up goes back.
//
// Each page of the test document has a red left column and a blue right one,
// so the column in view can be read off the canvas.
//
// Run: bun tests/read-columns.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { captureWindowPixels, postMessage, sleep, WM_KEYDOWN } from "./winapi.ts";

const kPages = 3;
// Page Down presses allowed to reach the next column
const kMaxSteps = 40;

type Color = "red" | "blue";

function makeColumnsPdf(): string {
  const content = "1 0 0 rg 40 20 240 360 re f 0 0 1 rg 320 20 240 360 re f";
  const kids = Array.from({ length: kPages }, (_, i) => `${3 + i} 0 R`).join(" ");
  const objs = ["<< /Type /Catalog /Pages 2 0 R >>", `<< /Type /Pages /Count ${kPages} /Kids [${kids}] >>`];
  const contentObj = 3 + kPages;
  for (let i = 0; i < kPages; i++) {
    objs.push(`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 600 400] /Contents ${contentObj} 0 R >>`);
  }
  objs.push(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);
  return assemblePdf(objs);
}

// what fills the canvas: red, blue, or both
function colors(frame: number): { red: number; blue: number } {
  const cap = captureWindowPixels(findCanvas(frame));
  if (!cap) {
    throw new Error("read-columns: could not capture the canvas");
  }
  let red = 0;
  let blue = 0;
  const { data } = cap;
  for (let i = 0; i < data.length; i += 4) {
    const b = data[i]!;
    const g = data[i + 1]!;
    const r = data[i + 2]!;
    if (r > 180 && g < 80 && b < 80) red++;
    else if (b > 180 && r < 80 && g < 80) blue++;
  }
  return { red, blue };
}

function only(c: { red: number; blue: number }): Color | null {
  if (c.red > 1000 && c.blue === 0) return "red";
  if (c.blue > 1000 && c.red === 0) return "blue";
  return null;
}

async function pageNo(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  return Number(/page=(\d+)/.exec(String(res[1] ?? ""))?.[1] ?? 0);
}

// presses Page Down (or Up) until the view shows only `want`, on page `page`
async function scrollTo(client: ControlClient, frame: number, down: boolean, want: Color, page: number) {
  const cmd = cmdId(down ? "CmdScrollDownPage" : "CmdScrollUpPage");
  for (let i = 0; i < kMaxSteps; i++) {
    sendCommand(frame, cmd);
    await client.waitForRenderIdle();
    const c = colors(frame);
    const seen = only(c);
    const p = await pageNo(client);
    if (seen === want && p === page) {
      return;
    }
    // both columns at once means the view isn't following the columns
    if (c.red > 1000 && c.blue > 1000) {
      throw new Error(`read-columns: both columns in view while looking for ${want} on page ${p}`);
    }
    if (seen && seen !== want && p !== page && p > page === down) {
      throw new Error(`read-columns: went past ${want} on page ${page} to ${seen} on page ${p}`);
    }
  }
  throw new Error(`read-columns: never reached the ${want} column of page ${page}`);
}

export async function testit(): Promise<void> {
  if (!IS_WIN || USE_NG) {
    console.log("read-columns: drives orig's canvas scrolling");
    return;
  }
  const pdf = tmpPath("read-columns.pdf");
  writeFileSync(pdf, makeColumnsPdf(), "latin1");
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleReadColumns"]);
    await client.waitForRenderIdle();
    await sleep(200);
    const start = only(colors(frame));
    if (start !== "red") {
      throw new Error(`read-columns: should start on the left column alone, got ${JSON.stringify(colors(frame))}`);
    }

    await scrollTo(client, frame, true, "blue", 1);
    await scrollTo(client, frame, true, "red", 2);
    await scrollTo(client, frame, false, "blue", 1);
    console.log("  page 1 left -> right -> page 2 left, and back up");

    // Automatically Scroll follows the columns too: from the end of page 1's
    // right column on to page 2's left one
    await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleAutomaticallyScroll"]);
    postMessage(frame, WM_KEYDOWN, "9".charCodeAt(0), 1);
    const deadline = Date.now() + 10000;
    for (;;) {
      if (only(colors(frame)) === "red" && (await pageNo(client)) === 2) {
        break;
      }
      if (Date.now() > deadline) {
        throw new Error(
          `read-columns: auto-scroll didn't reach page 2's left column: ${JSON.stringify(colors(frame))}`,
        );
      }
      await sleep(100);
    }
    await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleAutomaticallyScroll"]);
    console.log("  auto-scroll: page 1 right -> page 2 left");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
