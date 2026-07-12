// Ad-hoc test for in-page find in markdown files rendered in a WebView2.
//
// Ctrl+F on a markdown doc now shows Sumatra's own find bar (not the
// browser's find UI); the search runs over the rendered DOM text inside the
// webview (kFindInPageJs) and highlights matches via the CSS Custom Highlight
// API. This test opens a .md with 3 occurrences of "zebra" (one spanning
// text-node boundaries via **bold**, one capitalized), drives the find bar,
// and verifies the "n / m" status and the highlight colors on screen.
//
// Run:  bun tests/ad-hoc-md-find.ts

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, EXE, runStandalone, tmpPath } from "./util.ts";
import { waitForFrame, sendCommand, findCanvas } from "./win-automation.ts";
import {
  sleep,
  enumWindows,
  getWindowPid,
  findChildWindow,
  getWindowText,
  sendText,
  captureWindowToPng,
} from "./winapi.ts";

// count pixels close to (r,g,b) in a png, sampling every 3rd pixel. The
// tolerance is generous because WebView2 renders through the display color
// profile, shifting the CSS colors (e.g. #ff9632 comes out as 243,152,43)
function countColorPixels(png: string, r: number, g: number, b: number): number {
  const p = png.split("\\").join("\\\\");
  const ps =
    `Add-Type -AssemblyName System.Drawing; $bmp=[System.Drawing.Bitmap]::FromFile('${p}'); $n=0; ` +
    `for($y=0;$y -lt $bmp.Height;$y+=3){for($x=0;$x -lt $bmp.Width;$x+=3){$c=$bmp.GetPixel($x,$y); ` +
    `if([Math]::Abs($c.R-${r}) -le 20 -and [Math]::Abs($c.G-${g}) -le 20 -and [Math]::Abs($c.B-${b}) -le 20){$n++}}}; ` +
    `$bmp.Dispose(); Write-Output $n`;
  const res = Bun.spawnSync(["powershell", "-NoProfile", "-Command", ps]);
  return parseInt(res.stdout.toString().trim(), 10) || 0;
}

// the find bar is a WS_POPUP owned by the frame with an Edit + Static inside
function findFindBar(pid: number, frame: number): { edit: number; status: number } | null {
  let res: { edit: number; status: number } | null = null;
  enumWindows((hwnd) => {
    if (hwnd === frame || getWindowPid(hwnd) !== pid) {
      return true;
    }
    const edit = findChildWindow(hwnd, "Edit");
    const status = findChildWindow(hwnd, "Static");
    if (edit && status) {
      res = { edit, status };
      return false;
    }
    return true;
  });
  return res;
}

async function waitForStatus(status: number, want: string, timeoutMs = 8000): Promise<string> {
  const deadline = Date.now() + timeoutMs;
  let last = "";
  while (Date.now() < deadline) {
    last = getWindowText(status);
    if (last === want) {
      return last;
    }
    await sleep(100);
  }
  return last;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("ad-hoc-md-find");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const md = join(dir, "find-test.md");
  writeFileSync(
    md,
    "# Find test\n\nzebra one\n\nsome ze**br**a bold\n\nlast Zebra here\n\nnothing else\n",
  );

  Bun.spawnSync(["taskkill", "/F", "/IM", "SumatraPDF.exe"]);
  await sleep(300);
  const proc = Bun.spawn([EXE, "-for-testing", md], { stdout: "ignore", stderr: "ignore" });
  try {
    const frame = await waitForFrame(proc.pid!);
    await sleep(3000); // let the WebView2 initialize and render the markdown

    sendCommand(frame, cmdId("CmdFindFirst"));
    await sleep(500);
    const bar = findFindBar(proc.pid!, frame);
    if (!bar) {
      throw new Error("native find bar did not appear (still using webview's find UI?)");
    }

    // find-as-you-type: "zebra" matches case-insensitively, incl. one spanning
    // bold text-node boundaries (ze**br**a) and one capitalized (Zebra)
    sendText(bar.edit, "zebra");
    let s = await waitForStatus(bar.status, "1 / 3");
    if (s !== "1 / 3") {
      throw new Error(`expected status "1 / 3", got "${s}"`);
    }

    sendCommand(frame, cmdId("CmdFindNext"));
    s = await waitForStatus(bar.status, "2 / 3");
    if (s !== "2 / 3") {
      throw new Error(`expected status "2 / 3" after find-next, got "${s}"`);
    }

    sendCommand(frame, cmdId("CmdFindPrev"));
    s = await waitForStatus(bar.status, "1 / 3");
    if (s !== "1 / 3") {
      throw new Error(`expected status "1 / 3" after find-prev, got "${s}"`);
    }

    // highlight colors from kFindInPageJs: #ffee70 all matches, #ff9632 current
    const canvas = findCanvas(frame);
    const shot1 = join(dir, "highlighted.png");
    captureWindowToPng(canvas || frame, shot1);
    const yellow = countColorPixels(shot1, 0xff, 0xee, 0x70);
    const orange = countColorPixels(shot1, 0xff, 0x96, 0x32);
    console.log(`highlight pixels: yellow=${yellow} orange=${orange}`);
    if (yellow < 5 || orange < 5) {
      throw new Error(`expected visible highlights (yellow=${yellow}, orange=${orange}) -> ${shot1}`);
    }

    // clearing the text must remove the highlights in the webview
    sendText(bar.edit, "");
    await sleep(1000);
    const shot2 = join(dir, "cleared.png");
    captureWindowToPng(canvas || frame, shot2);
    const yellow2 = countColorPixels(shot2, 0xff, 0xee, 0x70);
    const orange2 = countColorPixels(shot2, 0xff, 0x96, 0x32);
    if (yellow2 >= 5 || orange2 >= 5) {
      throw new Error(`highlights not cleared (yellow=${yellow2}, orange=${orange2}) -> ${shot2}`);
    }

    console.log("PASS: native find bar drives in-page search + highlighting in the markdown webview");
  } finally {
    proc.kill();
    await sleep(300);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
