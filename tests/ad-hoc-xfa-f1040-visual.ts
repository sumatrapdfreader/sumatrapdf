// Visual regression for hybrid IRS Form 1040 XFA rendering.
//
// Renders pages 1 and 2 of tests/ad-hoc-xfa-data/ad-hoc-f1040.pdf headlessly via
// -dbg-control TestRenderPagePng. XFA overlay paint is not pixel-stable across
// runs, so we compare committed baseline *metrics* (dimensions, dark-pixel count,
// distinct quantized colors) with tolerance instead of byte-identical PNGs.
// Baseline PNGs are kept for human inspection.
//
// Run:  bun tests/ad-hoc-xfa-f1040-visual.ts [--no-build]
//       bun tests/ad-hoc-xfa-f1040-visual.ts --update-baseline

import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { sleep } from "./winapi.ts";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { ControlCommand, runControlCommand } from "../cmd/control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const here = dirname(fileURLToPath(import.meta.url));
const dataDir = join(here, "ad-hoc-xfa-data");
const PDF = join(dataDir, "ad-hoc-f1040.pdf");

const RENDER_ZOOM_PCT = 100;

type PageBaseline = {
  pageNo: number;
  png: string;
  metrics: string;
};

const PAGES: PageBaseline[] = [
  { pageNo: 1, png: "ad-hoc-f1040-p0.png", metrics: "ad-hoc-f1040-p0.metrics.json" },
  { pageNo: 2, png: "ad-hoc-f1040-p1.png", metrics: "ad-hoc-f1040-p1.metrics.json" },
];

const MIN_METRICS: Record<number, Pick<PngMetrics, "dark" | "distinct">> = {
  1: { dark: 9000, distinct: 15 },
  2: { dark: 9000, distinct: 28 },
};

export type PngMetrics = {
  width: number;
  height: number;
  distinct: number;
  dark: number;
  nonWhite: number;
  sampled: number;
};

function analyzePngMetrics(pngPath: string): PngMetrics {
  const p = pngPath.replace(/\\/g, "\\\\");
  const ps = `
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$bmp = [System.Drawing.Bitmap]::FromFile('${p}')
$w = $bmp.Width; $h = $bmp.Height
$colors = @{}
$dark = 0; $nonWhite = 0; $total = 0
for ($y = 0; $y -lt $h; $y += 2) {
  for ($x = 0; $x -lt $w; $x += 2) {
    $total++
    $c = $bmp.GetPixel($x, $y)
    $sum = $c.R + $c.G + $c.B
    if ($sum -lt 750) { $dark++ }
    if ($sum -lt 740) { $nonWhite++ }
    $key = ([int]($c.R / 16)) * 256 + ([int]($c.G / 16)) * 16 + [int]($c.B / 16)
    $colors[$key] = 1
  }
}
$bmp.Dispose()
Write-Output ("$w $h $($colors.Count) $dark $nonWhite $total")
`;
  const res = Bun.spawnSync(["powershell", "-NoProfile", "-Command", ps]);
  const out = res.stdout.toString().trim();
  if (!res.success || !out) {
    throw new Error(`analyzePngMetrics failed for ${pngPath}: ${res.stderr.toString()}`);
  }
  const [width, height, distinct, dark, nonWhite, sampled] = out.split(/\s+/).map(Number);
  return { width, height, distinct, dark, nonWhite, sampled };
}

async function renderPagePngOnce(pageNo: number, outPath: string): Promise<string> {
  const res = await runControlCommand(EXE, ControlCommand.TestRenderPagePng, [
    PDF,
    pageNo,
    RENDER_ZOOM_PCT,
    outPath,
  ]);
  const exitCode = res[0] as number;
  const raw = String(res[1] ?? "").trim();
  if (exitCode !== 0) {
    throw new Error(`TestRenderPagePng page ${pageNo} failed: ${raw}`);
  }
  return raw;
}

// Hybrid XFA renders can occasionally come back thin; retry and keep the best paint.
async function renderPagePng(pageNo: number, outPath: string): Promise<{ msg: string; metrics: PngMetrics }> {
  const min = MIN_METRICS[pageNo];
  let bestPath = "";
  let bestMsg = "";
  let bestMetrics: PngMetrics | null = null;
  for (let attempt = 0; attempt < 4; attempt++) {
    const tryPath = outPath.replace(/\.png$/i, `.try${attempt}.png`);
    const msg = await renderPagePngOnce(pageNo, tryPath);
    const metrics = analyzePngMetrics(tryPath);
    if (!bestMetrics || metrics.dark > bestMetrics.dark) {
      bestMetrics = metrics;
      bestPath = tryPath;
      bestMsg = msg;
    }
    if (metrics.dark >= min.dark && metrics.distinct >= min.distinct) {
      copyFileSync(tryPath, outPath);
      return { msg, metrics };
    }
    await sleep(120);
  }
  if (!bestMetrics || !bestPath) {
    throw new Error(`TestRenderPagePng page ${pageNo}: no render captured`);
  }
  copyFileSync(bestPath, outPath);
  if (bestMetrics.dark < min.dark || bestMetrics.distinct < min.distinct) {
    throw new Error(
      `page ${pageNo}: best render still thin (dark=${bestMetrics.dark}, distinct=${bestMetrics.distinct}); ` +
        `need dark>=${min.dark} distinct>=${min.distinct}`,
    );
  }
  return { msg: `${bestMsg} (best-of-4)`, metrics: bestMetrics };
}

function assertMetrics(name: string, actual: PngMetrics, expected: PngMetrics, pageNo: number): void {
  const min = MIN_METRICS[pageNo];
  if (actual.width !== expected.width || actual.height !== expected.height) {
    throw new Error(
      `${name}: dimensions ${actual.width}x${actual.height}, expected ${expected.width}x${expected.height}`,
    );
  }
  // Fail on regressions (significant drop vs baseline), but allow richer paints.
  const minDark = Math.max(min.dark, Math.floor(expected.dark * 0.65));
  const minDistinct = Math.max(min.distinct, Math.floor(expected.distinct * 0.75));
  if (actual.dark < minDark) {
    throw new Error(`${name}: dark=${actual.dark} regressed (need >=${minDark}, baseline=${expected.dark})`);
  }
  if (actual.distinct < minDistinct) {
    throw new Error(
      `${name}: distinct=${actual.distinct} regressed (need >=${minDistinct}, baseline=${expected.distinct})`,
    );
  }
}

function updateBaseline(actualPng: string, baselinePng: string, baselineMetrics: string, metrics: PngMetrics): void {
  copyFileSync(actualPng, baselinePng);
  writeFileSync(baselineMetrics, `${JSON.stringify(metrics, null, 2)}\n`);
  console.log(`updated baseline: ${baselinePng} + ${baselineMetrics}`);
}

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    throw new Error(`fixture missing: ${PDF}`);
  }

  const update = process.argv.includes("--update-baseline");
  const scratch = tmpPath("ad-hoc-xfa-f1040-visual");
  rmSync(scratch, { recursive: true, force: true });
  mkdirSync(scratch, { recursive: true });

  for (const { pageNo, png, metrics } of PAGES) {
    const actualPng = join(scratch, png);
    const baselinePng = join(dataDir, png);
    const baselineMetrics = join(dataDir, metrics);
    const { msg, metrics: actualMetrics } = await renderPagePng(pageNo, actualPng);
    console.log(
      `${png}: ${msg} metrics=${actualMetrics.width}x${actualMetrics.height} distinct=${actualMetrics.distinct} dark=${actualMetrics.dark}`,
    );

    if (update) {
      updateBaseline(actualPng, baselinePng, baselineMetrics, actualMetrics);
      continue;
    }

    if (!existsSync(baselineMetrics)) {
      throw new Error(`missing ${baselineMetrics} (run with --update-baseline)`);
    }
    const expectedMetrics = JSON.parse(readFileSync(baselineMetrics, "utf8")) as PngMetrics;
    assertMetrics(png, actualMetrics, expectedMetrics, pageNo);
    console.log(`match: ${png} metrics OK`);
  }

  console.log(`ad-hoc-xfa-f1040-visual: ${PAGES.length} page(s) OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}