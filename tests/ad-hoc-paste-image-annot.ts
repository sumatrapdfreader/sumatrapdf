// Ad-hoc test for the "Create Image Annotation From Clipboard" feature
// (CmdCreateAnnotImageFromClipboard): pasting a clipboard image into a PDF as an
// image Stamp annotation.
//
// It puts a solid-red bitmap on the clipboard, opens a PDF, runs the command,
// and verifies a red region was stamped onto the page. Because it touches the
// system clipboard and drives the GUI, it's an ad-hoc test (not in all.ts).
//
// Run:  bun tests/ad-hoc-paste-image-annot.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, EXE, runStandalone, tmpPath } from "./util.ts";
import { waitForFrame, sendCommand, findCanvas } from "./win-automation.ts";
import { sleep, captureWindowToPng } from "./winapi.ts";

function makePdf(n: number): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R >>");
  const kids: string[] = [];
  for (let i = 0; i < n; i++) {
    kids.push(`${4 + i * 2} 0 R`);
  }
  body[2] = enc(`<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${n} >>`);
  body[3] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
  for (let i = 0; i < n; i++) {
    const po = 4 + i * 2;
    const co = po + 1;
    const st = `BT /F1 24 Tf 72 740 Td (Page ${i + 1}) Tj ET`;
    body[po] = enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
        `/Resources << /Font << /F1 3 0 R >> >> /Contents ${co} 0 R >>`,
    );
    body[co] = enc(`<< /Length ${st.length} >>\nstream\n${st}\nendstream`);
  }
  const maxN = 3 + n * 2;
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const off: Record<number, number> = {};
  let pos = parts[0].length;
  for (let k = 1; k <= maxN; k++) {
    off[k] = pos;
    const o = Buffer.concat([enc(`${k} 0 obj\n`), body[k], enc("\nendobj\n")]);
    parts.push(o);
    pos += o.length;
  }
  let x = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let k = 1; k <= maxN; k++) {
    x += `${String(off[k]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${x}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

function countRedPixels(png: string): number {
  const p = png.split("\\").join("\\\\");
  const ps = `Add-Type -AssemblyName System.Drawing; $b=[System.Drawing.Bitmap]::FromFile('${p}'); $n=0; for($y=0;$y -lt $b.Height;$y+=3){for($x=0;$x -lt $b.Width;$x+=3){$c=$b.GetPixel($x,$y); if($c.R -gt 180 -and $c.G -lt 80 -and $c.B -lt 80){$n++}}}; $b.Dispose(); Write-Output $n`;
  const r = Bun.spawnSync(["powershell", "-NoProfile", "-Command", ps]);
  return parseInt(r.stdout.toString().trim(), 10) || 0;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("ad-hoc-paste-image-annot");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "t.pdf");
  writeFileSync(pdf, makePdf(3));

  // put a solid-red image on the clipboard (SetImage requires an STA thread)
  const psSet =
    "Add-Type -AssemblyName System.Windows.Forms,System.Drawing; " +
    "$b = New-Object System.Drawing.Bitmap 160,120; $g=[System.Drawing.Graphics]::FromImage($b); " +
    "$g.Clear([System.Drawing.Color]::Red); $g.Dispose(); [System.Windows.Forms.Clipboard]::SetImage($b)";
  const set = Bun.spawnSync(["powershell", "-NoProfile", "-STA", "-Command", psSet]);
  if (!set.success) {
    throw new Error(`failed to set clipboard image: ${set.stderr.toString()}`);
  }

  Bun.spawnSync(["taskkill", "/F", "/IM", "SumatraPDF.exe"]);
  await sleep(300);
  const proc = Bun.spawn([EXE, "-for-testing", pdf], { stdout: "ignore", stderr: "ignore" });
  try {
    const frame = await waitForFrame(proc.pid!);
    await sleep(1500);
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("no canvas");
    }

    const before = join(dir, "before.png");
    captureWindowToPng(canvas, before);
    const redBefore = countRedPixels(before);

    sendCommand(frame, cmdId("CmdCreateAnnotImageFromClipboard"));
    await sleep(1000);

    const after = join(dir, "after.png");
    captureWindowToPng(canvas, after);
    const redAfter = countRedPixels(after);

    console.log(`red pixels: before=${redBefore} after=${redAfter} -> ${after}`);
    if (redAfter < redBefore + 100) {
      throw new Error(`expected a red image stamp on the page (before=${redBefore}, after=${redAfter})`);
    }
    console.log("PASS: clipboard image pasted as an image Stamp annotation");
  } finally {
    proc.kill();
    await sleep(300);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
