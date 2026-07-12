// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/3731
//
// A PDF can reference an image stored in a separate file via the stream's /F
// (file specification) + /FFilter, with an empty embedded stream. mupdf has no
// support for this, so the image doesn't render. We added an opt-in loader
// (AllowExternalImages setting) that reads the sibling file via a callback.
//
// This builds a tiny external-image PDF next to a colorful JPEG and checks:
//   - AllowExternalImages = true  -> the image renders (canvas has color)
//   - AllowExternalImages = false -> blank (secure default; image not loaded)
//
// Run:  bun tests/issue-3731.ts [--no-build]   (or via tests/all.ts)

import { mkdirSync, rmSync, writeFileSync, existsSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { launchSumatra, waitForFrame, findCanvas } from "./win-automation.ts";
import { sleep, captureWindowToPng } from "./winapi.ts";

// build a PDF whose only image is an external-file stream (/F + /FFilter), with
// an empty embedded stream. imgName sits next to the PDF; w/h are the image dims
function makeExternalImagePdf(imgName: string, w: number, h: number): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc("<</Pages 2 0 R/Type/Catalog>>");
  body[2] = enc("<</Count 1/Kids[3 0 R]/Type/Pages>>");
  body[3] = enc(
    `<</Parent 2 0 R/Resources<</XObject<</Im1 4 0 R>>>>/Contents 5 0 R/MediaBox[0 0 ${w} ${h}]/Type/Page>>`,
  );
  // the image XObject: data comes from the external file imgName, decoded with
  // /FFilter (DCTDecode); the embedded stream is empty.
  body[4] = Buffer.concat([
    enc(
      `<</BitsPerComponent 8/ColorSpace/DeviceRGB/F(${imgName})/FFilter/DCTDecode` +
        `/Height ${h}/Length 0/Subtype/Image/Type/XObject/Width ${w}>>\nstream\nendstream`,
    ),
  ]);
  const content = `q ${w} 0 0 ${h} 0 0 cm /Im1 Do Q`;
  body[5] = enc(`<</Length ${content.length}>>\nstream\n${content}\nendstream`);

  const maxN = 5;
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xb5\xb6\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0].length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj `), body[n], enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let n = 1; n <= maxN; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<</Size ${maxN + 1}/Root 1 0 R>>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

// count distinct quantized colors in the canvas capture. The loaded image is
// colorful (many colors); a denied external image renders as a solid black
// rectangle (so just black + the gray window background => very few colors).
//
// Only sample the central 50% of the canvas: the page (and thus the image)
// fills the middle, while transient overlays such as the "Errors in PDF"
// notification toast (which appears in the OFF case because the denied stream
// makes mupdf report a PDF error) sit in the bottom-right corner. Counting the
// whole canvas would fold that yellow/red toast into the color count and
// spuriously push the denied case over the threshold.
function distinctColors(png: string): number {
  const p = png.split("\\").join("\\\\");
  const ps =
    `Add-Type -AssemblyName System.Drawing; $b=[System.Drawing.Bitmap]::FromFile('${p}'); ` +
    `$x0=[int]($b.Width*0.25); $x1=[int]($b.Width*0.75); $y0=[int]($b.Height*0.25); $y1=[int]($b.Height*0.75); ` +
    `$h=@{}; for($y=$y0;$y -lt $y1;$y+=4){for($x=$x0;$x -lt $x1;$x+=4){` +
    `$c=$b.GetPixel($x,$y); $k=[int]($c.R/32)*64+[int]($c.G/32)*8+[int]($c.B/32); $h[$k]=1}}; ` +
    `$b.Dispose(); Write-Output $h.Count`;
  const r = Bun.spawnSync(["powershell", "-NoProfile", "-Command", ps]);
  return parseInt(r.stdout.toString().trim(), 10) || 0;
}

async function renderDistinctColors(pdf: string, appdata: string, label: string): Promise<number> {
  Bun.spawnSync(["taskkill", "/F", "/IM", "SumatraPDF.exe"]);
  await sleep(300);
  const proc = launchSumatra(["-appdata", appdata, pdf]);
  try {
    const frame = await waitForFrame(proc.pid!);
    const png = tmpPath(`issue-3731-${label}.png`);
    // Capture once external decode + paint has *settled* rather than at a fixed
    // delay: under full-suite load 2s wasn't enough and we'd catch an
    // intermediate paint state (a count between the denied ~2 and rendered ~450),
    // failing spuriously. Poll until the color count is stable across two
    // consecutive captures (the canvas is static once rendering finishes).
    let prev = -1;
    let val = 0;
    let stable = 0;
    const deadline = Date.now() + 15000;
    while (Date.now() < deadline) {
      await sleep(500);
      const canvas = findCanvas(frame);
      if (!canvas) {
        continue;
      }
      if (!captureWindowToPng(canvas, png)) {
        throw new Error("capture failed");
      }
      val = distinctColors(png);
      if (val === prev) {
        stable++;
        if (stable >= 2) {
          break;
        }
      } else {
        stable = 0;
        prev = val;
      }
    }
    return val;
  } finally {
    proc.kill();
    await sleep(300);
  }
}

export async function testit(): Promise<void> {
  if (!existsSync(EXE)) {
    throw new Error(`app not found: ${EXE}`);
  }
  const dir = tmpPath("issue-3731");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });

  // a colorful external image + the PDF that references it by name
  const w = 320;
  const h = 240;
  const jpg = join(dir, "imagestream.jpg");
  const psMakeJpg =
    "Add-Type -AssemblyName System.Drawing; " +
    `$b=New-Object System.Drawing.Bitmap ${w},${h}; $g=[System.Drawing.Graphics]::FromImage($b); ` +
    "$r=New-Object System.Random 7; for($i=0;$i -lt 200;$i++){$br=New-Object System.Drawing.SolidBrush " +
    "([System.Drawing.Color]::FromArgb($r.Next(255),$r.Next(255),$r.Next(255)));" +
    `$g.FillRectangle($br,$r.Next(${w}),$r.Next(${h}),40,40)}; $g.Dispose();` +
    `$b.Save('${jpg.split("\\").join("\\\\")}',[System.Drawing.Imaging.ImageFormat]::Jpeg); $b.Dispose()`;
  const mk = Bun.spawnSync(["powershell", "-NoProfile", "-Command", psMakeJpg]);
  if (!mk.success || !existsSync(jpg)) {
    throw new Error(`failed to create test JPEG: ${mk.stderr.toString()}`);
  }
  const pdf = join(dir, "imagestream.pdf");
  writeFileSync(pdf, makeExternalImagePdf("imagestream.jpg", w, h));

  const mkAppdata = (name: string, allow: boolean): string => {
    const ad = join(dir, name);
    mkdirSync(ad, { recursive: true });
    writeFileSync(
      join(ad, "SumatraPDF-settings.txt"),
      `AllowExternalImages = ${allow ? "true" : "false"}\nReuseInstance = false\n` +
        "RestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
    );
    return ad;
  };

  const onColors = await renderDistinctColors(pdf, mkAppdata("ad-on", true), "on");
  const offColors = await renderDistinctColors(pdf, mkAppdata("ad-off", false), "off");

  console.log(`AllowExternalImages on : distinct colors = ${onColors} (expect many, image rendered)`);
  console.log(`AllowExternalImages off: distinct colors = ${offColors} (expect few, image denied)`);

  if (offColors > 8) {
    throw new Error(`external image rendered with the setting OFF (${offColors} colors); secure default broken`);
  }
  if (onColors < 25) {
    throw new Error(`external image did NOT render with the setting ON (${onColors} colors); #3731 not working`);
  }
  console.log("PASS: external-file image loads only when AllowExternalImages is on (issue #3731)");
}

if (import.meta.main) {
  await runStandalone(testit);
}
