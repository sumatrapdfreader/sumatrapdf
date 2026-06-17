// Test for CmdStartAutoScroll: it triggers middle-click-style auto-scroll
// without a middle button (e.g. for laptops / trackpads), by anchoring at the
// current cursor position. Implemented as StartAutoScrollAtCursor in
// src/Canvas.cpp, dispatched from src/SumatraPDF.cpp.
//
// The test puts the real cursor over the canvas, invokes the command via
// WM_COMMAND, moves the cursor offset to set a scroll speed, and checks the
// document scrolls -- then invokes the command again and checks it stops.

import { spawnSync } from "node:child_process";
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT, tmpPath } from "./util";

function makePdf(nPages: number): string {
  const objs: string[] = [];
  objs.push(`<< /Type /Catalog /Pages 2 0 R >>`);
  const kids = Array.from({ length: nPages }, (_, i) => `${3 + i} 0 R`).join(" ");
  objs.push(`<< /Type /Pages /Count ${nPages} /Kids [${kids}] >>`);
  for (let i = 0; i < nPages; i++) {
    objs.push(`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>`);
  }
  let body = "%PDF-1.4\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(body.length);
    body += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefStart = body.length;
  const size = objs.length + 1;
  body += `xref\n0 ${size}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    body += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  body += `trailer\n<< /Size ${size} /Root 1 0 R >>\nstartxref\n${xrefStart}\n%%EOF\n`;
  return body;
}

const SETTINGS = [
  `DefaultDisplayMode = continuous`,
  `DefaultZoom = fit page`,
  `Scrollbars = windows`,
  `RestoreSession = false`,
  `ShowStartPage = false`,
  `CheckForUpdates = false`,
  ``,
].join("\n");

export async function testit(): Promise<void> {
  const pdf = tmpPath("cmd-autoscroll.pdf");
  writeFileSync(pdf, makePdf(12), "latin1");

  const appdata = tmpPath("cmd-autoscroll-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  const ps1 = join(ROOT, "tests", "cmd-start-autoscroll.verify.ps1");
  const r = spawnSync(
    "powershell.exe",
    ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", ps1, "-Exe", EXE, "-Pdf", pdf, "-AppData", appdata],
    { encoding: "utf8", timeout: 90_000 },
  );
  const out = (r.stdout || "") + (r.stderr || "");
  const m = out.match(/RESULT start=(\d+) mid=(\d+) afterStop=(\d+)/);
  if (!m) {
    throw new Error(`could not read scroll position; output:\n${out}`);
  }
  const start = parseInt(m[1], 10);
  const mid = parseInt(m[2], 10);
  const afterStop = parseInt(m[3], 10);
  const scrolled = mid - start;
  const afterStopDelta = afterStop - mid;
  console.log(`  invoked command: scrolled ${scrolled}px (pos ${start}->${mid}), then +${afterStopDelta}px after stop`);

  if (scrolled < 30) {
    throw new Error(`CmdStartAutoScroll did not start auto-scroll (moved only ${scrolled}px)`);
  }
  // after the second invocation the auto-scroll must stop (allow a little timer slack)
  if (afterStopDelta > 15) {
    throw new Error(`CmdStartAutoScroll did not stop on the second invocation (+${afterStopDelta}px after stop)`);
  }
  console.log(`  CmdStartAutoScroll starts and stops auto-scroll ✓`);
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
