// Ad-hoc test: verify libjpeg-turbo JPEG decoding works end-to-end after the
// 3.1.4.1 upgrade. Opens a real photographic JPEG in the GUI, captures the
// canvas, and asserts the rendered content is a real decoded image (lots of
// distinct colors / high luminance variance) rather than a blank/error canvas.
//
// This exercises the full mupdf -> libjpeg-turbo decode path, including the
// x86-64 NASM SIMD (YCbCr->RGB color conversion, IDCT, upsampling).
//
// GUI automation, so it lives as an ad-hoc test (not in tests/all.ts). Run:
//   bun tests/ad-hoc-jpeg-decode.ts [--no-build]

import { existsSync } from "node:fs";
import { runStandalone, tmpPath } from "./util.ts";
import { launchSumatra, waitForFrame, findCanvas } from "./win-automation.ts";
import { sleep, captureWindowToPng, getClientRect } from "./winapi.ts";

const BUGS_DIR = "C:\\Users\\kjk\\OneDrive\\!sumatra\\bugs";
// real-world photographic JPEGs committed as bug repros
const JPEGS = ["bug-1544.jpg", "bug-4842-62-63.jpg"];

// Analyze a captured PNG with .NET System.Drawing (via PowerShell): returns
// the number of distinct quantized colors and the luminance standard deviation.
function analyzePng(pngPath: string): { distinct: number; stddev: number } {
  const ps = `
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$bmp = [System.Drawing.Bitmap]::FromFile('${pngPath.replace(/\\/g, "\\\\")}')
$w = $bmp.Width; $h = $bmp.Height
$colors = @{}
$sum = 0.0; $sumSq = 0.0; $n = 0
# sample on a grid to keep it fast
$stepX = [Math]::Max(1, [int]($w / 120))
$stepY = [Math]::Max(1, [int]($h / 120))
for ($y = 0; $y -lt $h; $y += $stepY) {
  for ($x = 0; $x -lt $w; $x += $stepX) {
    $c = $bmp.GetPixel($x, $y)
    $key = ([int]($c.R / 16)) * 256 + ([int]($c.G / 16)) * 16 + [int]($c.B / 16)
    $colors[$key] = 1
    $lum = 0.299 * $c.R + 0.587 * $c.G + 0.114 * $c.B
    $sum += $lum; $sumSq += $lum * $lum; $n++
  }
}
$bmp.Dispose()
$mean = $sum / $n
$var = ($sumSq / $n) - ($mean * $mean)
if ($var -lt 0) { $var = 0 }
$stddev = [Math]::Sqrt($var)
Write-Output ("$($colors.Count) $stddev")
`;
  const res = Bun.spawnSync(["powershell", "-NoProfile", "-Command", ps]);
  const out = res.stdout.toString().trim();
  if (!res.success || !out) {
    throw new Error(`analyzePng failed: ${res.stderr.toString()}`);
  }
  const [distinct, stddev] = out.split(/\s+/).map(Number);
  return { distinct, stddev };
}

async function renderAndCheck(jpg: string): Promise<void> {
  const path = `${BUGS_DIR}\\${jpg}`;
  if (!existsSync(path)) {
    console.log(`SKIP ${jpg}: repro file not found at ${path}`);
    return;
  }

  const proc = launchSumatra([path]);
  try {
    const frame = await waitForFrame(proc.pid!);
    // give the async image decode + paint time to complete
    await sleep(2500);
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error(`${jpg}: could not find canvas window`);
    }
    const rc = getClientRect(canvas);
    if (rc.right - rc.left < 50 || rc.bottom - rc.top < 50) {
      throw new Error(`${jpg}: canvas too small (${rc.right - rc.left}x${rc.bottom - rc.top})`);
    }
    const png = tmpPath(`jpeg-decode-${jpg}.png`);
    if (!captureWindowToPng(canvas, png)) {
      throw new Error(`${jpg}: captureWindowToPng failed`);
    }
    const { distinct, stddev } = analyzePng(png);
    console.log(`${jpg}: distinct(quantized)=${distinct} lumStdDev=${stddev.toFixed(2)} -> ${png}`);

    // A blank / solid-color / error canvas yields ~1-3 distinct (quantized)
    // colors and near-zero luminance variance. A correctly decoded image has
    // many distinct colors and substantial luminance variance. (Low-saturation
    // / near-grayscale photos collapse into few RGB buckets, so the luminance
    // standard deviation is the primary signal.)
    if (distinct < 6) {
      throw new Error(`${jpg}: too few distinct colors (${distinct}); image likely not decoded`);
    }
    if (stddev < 12) {
      throw new Error(`${jpg}: luminance variance too low (${stddev.toFixed(2)}); canvas likely blank`);
    }
  } finally {
    proc.kill();
    await sleep(300);
  }
}

export async function testit(): Promise<void> {
  let tested = 0;
  for (const jpg of JPEGS) {
    const path = `${BUGS_DIR}\\${jpg}`;
    if (!existsSync(path)) {
      console.log(`SKIP ${jpg}: not found`);
      continue;
    }
    await renderAndCheck(jpg);
    tested++;
  }
  if (tested === 0) {
    console.log("no JPEG repro files available; nothing verified");
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
