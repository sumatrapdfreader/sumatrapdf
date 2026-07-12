// Ad-hoc test for background PNG optimization (ext/zopfli) of saved PNGs.
//
// CmdPasteClipboardImage saves the clipboard image as clipboard*.png in the
// Downloads folder, then OptimizePngFileAsync() recompresses it in place on a
// background thread. This test puts a large gradient bitmap on the clipboard
// (big enough that zopfli takes a couple of seconds, so we can observe the
// initial file before it's swapped), runs the command, and verifies the file
// shrinks and still decodes to the same dimensions.
//
// Touches the system clipboard and the real Downloads folder, so it's an
// ad-hoc test (not in all.ts). Cleans up the file it creates.
//
// Run:  bun tests/ad-hoc-png-optimize.ts

import { readdirSync, readFileSync, rmSync, statSync } from "node:fs";
import { join } from "node:path";
import { cmdId, EXE, runStandalone } from "./util.ts";
import { waitForFrame, sendCommand } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const IMG_W = 1200;
const IMG_H = 900;

function ps(cmd: string, sta = false): string {
  const args = ["powershell", "-NoProfile"];
  if (sta) {
    args.push("-STA");
  }
  args.push("-Command", cmd);
  const r = Bun.spawnSync(args);
  if (!r.success) {
    throw new Error(`powershell failed: ${r.stderr.toString()}`);
  }
  return r.stdout.toString().trim();
}

function downloadsDir(): string {
  return ps("(New-Object -ComObject Shell.Application).Namespace('shell:Downloads').Self.Path");
}

function clipboardPngs(dir: string): Set<string> {
  const res = new Set<string>();
  for (const f of readdirSync(dir)) {
    if (/^clipboard(\.\d+)?\.png$/i.test(f)) {
      res.add(f);
    }
  }
  return res;
}

// gradient + diagonal lines: compresses OK but not trivially, so the WIC/GDI+
// encoder output is big enough for zopfli to (a) take a moment and (b) shrink
function setClipboardImage(): void {
  const cmd =
    "Add-Type -AssemblyName System.Windows.Forms,System.Drawing; " +
    `$b = New-Object System.Drawing.Bitmap ${IMG_W},${IMG_H}; ` +
    "$g = [System.Drawing.Graphics]::FromImage($b); " +
    `$rect = New-Object System.Drawing.Rectangle 0,0,${IMG_W},${IMG_H}; ` +
    "$br = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect,([System.Drawing.Color]::Navy),([System.Drawing.Color]::Orange),45; " +
    "$g.FillRectangle($br, $rect); " +
    "$pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::White),3; " +
    `for ($x = 0; $x -lt ${IMG_W}; $x += 50) { $g.DrawLine($pen, $x, 0, ${IMG_W} - $x, ${IMG_H}) }; ` +
    "$g.Dispose(); [System.Windows.Forms.Clipboard]::SetImage($b)";
  ps(cmd, true);
}

function pngDims(path: string): string {
  const p = path.split("\\").join("\\\\");
  return ps(
    `Add-Type -AssemblyName System.Drawing; $b=[System.Drawing.Bitmap]::FromFile('${p}'); Write-Output "$($b.Width)x$($b.Height)"; $b.Dispose()`,
  );
}

export async function testit(): Promise<void> {
  const dlDir = downloadsDir();
  const before = clipboardPngs(dlDir);

  setClipboardImage();

  Bun.spawnSync(["taskkill", "/F", "/IM", "SumatraPDF.exe"]);
  await sleep(300);
  const proc = Bun.spawn([EXE, "-for-testing"], { stdout: "ignore", stderr: "ignore" });
  let newFile = "";
  try {
    const frame = await waitForFrame(proc.pid!);
    await sleep(1000);

    sendCommand(frame, cmdId("CmdPasteClipboardImage"));

    // wait for the new clipboard*.png to appear
    for (let i = 0; i < 100 && !newFile; i++) {
      for (const f of clipboardPngs(dlDir)) {
        if (!before.has(f)) {
          newFile = join(dlDir, f);
          break;
        }
      }
      if (!newFile) {
        await sleep(50);
      }
    }
    if (!newFile) {
      throw new Error("clipboard*.png did not appear in Downloads");
    }
    const sizeOrig = statSync(newFile).size;
    console.log(`saved: ${newFile}, ${sizeOrig} bytes`);

    // wait for the background optimizer to swap in the smaller file
    // (zopfli takes anywhere from a few seconds to a couple of minutes)
    let sizeOpt = sizeOrig;
    for (let i = 0; i < 1800; i++) {
      await sleep(100);
      const s = statSync(newFile).size;
      if (s !== sizeOrig) {
        sizeOpt = s;
        break;
      }
    }
    if (sizeOpt >= sizeOrig) {
      throw new Error(`file did not shrink (${sizeOrig} => ${sizeOpt} bytes)`);
    }
    const pct = (100 - (sizeOpt * 100) / sizeOrig).toFixed(1);
    console.log(`optimized: ${sizeOrig} => ${sizeOpt} bytes (saved ${pct}%)`);

    const dims = pngDims(newFile);
    if (dims !== `${IMG_W}x${IMG_H}`) {
      throw new Error(`optimized png has wrong dimensions: ${dims}`);
    }
    if (statSync(newFile + ".zopfli-tmp", { throwIfNoEntry: false })) {
      throw new Error("leftover .zopfli-tmp file");
    }

    // the "optimized by us" tEXt marker chunk must sit right after IHDR
    // (offset 33) so future optimize runs skip this file
    const head = readFileSync(newFile).subarray(33, 33 + 64).toString("latin1");
    if (!head.startsWith("\0\0\0\x1atEXtSoftware\0SumatraPDF zopfli")) {
      throw new Error("optimized png is missing the SumatraPDF zopfli marker chunk after IHDR");
    }
    console.log("PASS: saved png was optimized in place, has the marker chunk and still decodes");
  } finally {
    proc.kill();
    await sleep(300);
    if (newFile) {
      rmSync(newFile, { force: true });
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
