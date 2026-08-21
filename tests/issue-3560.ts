// Test for issue #3560: "Outline styles were ignored".
//
// A PDF outline entry can ask for a colour (/C) and for bold / italic (/F), and
// the Bookmarks sidebar draws them. Since 3.3 every entry came out plain black:
// mupdf's fz_load_outline() copies the title, the uri and is_open out of the
// outline iterator but drops flags / r / g / b, so the styles never reached us.
//
// The test builds a PDF whose first bookmark is red and bold and whose second
// is blue, then reads the colours of the bookmarks tree straight off the screen.

import { writeFileSync } from "node:fs";
import { tmpPath } from "./util";
import { launchControlled, killAndWait } from "./win-automation";
import { captureWindowPixels, findChildWindow, isZoomed, moveWindow, showWindow, sleep, SW_RESTORE } from "./winapi";

// two pages and an outline with a red bold entry and a blue one. PageMode
// UseOutlines so the bookmarks sidebar opens with the document.
function makePdf(): string {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R /Outlines 5 0 R /PageMode /UseOutlines >>`,
    `<< /Type /Pages /Count 2 /Kids [3 0 R 4 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>`,
    `<< /Type /Outlines /Count 2 /First 6 0 R /Last 7 0 R >>`,
    `<< /Title (RedBoldRedBoldRedBold) /Parent 5 0 R /Next 7 0 R /Dest [3 0 R /XYZ null null null] /C [1 0 0] /F 2 >>`,
    `<< /Title (BlueBlueBlueBlueBlue) /Parent 5 0 R /Prev 6 0 R /Dest [4 0 R /XYZ null null null] /C [0 0 1] >>`,
  ];
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

type Tally = { red: number; blue: number };

// count clearly red and clearly blue pixels in the bookmarks tree
function countColoredText(tree: number): Tally {
  const cap = captureWindowPixels(tree);
  if (!cap) {
    throw new Error("could not capture the bookmarks tree");
  }
  const res: Tally = { red: 0, blue: 0 };
  const { w, h, data } = cap;
  // only the left half: the page numbers on the right are drawn in the theme's
  // accent colour and would be counted as coloured text
  const maxX = Math.floor(w / 2);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < maxX; x++) {
      const i = (y * w + x) * 4;
      const b = data[i]!;
      const g = data[i + 1]!;
      const r = data[i + 2]!;
      // strong, saturated colours only: anti-aliasing fringes on black text
      // are dark, and the theme's own text colours are not saturated
      if (r > 150 && g < 100 && b < 100) {
        res.red++;
      } else if (b > 150 && r < 100 && g < 100) {
        res.blue++;
      }
    }
  }
  return res;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-3560.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client, frame } = await launchControlled([pdf], { defaultWindowPos: true });
  try {
    await client.waitForRenderIdle();
    if (isZoomed(frame)) {
      showWindow(frame, SW_RESTORE);
    }
    moveWindow(frame, 40, 20, 1000, 800);
    await client.waitForRenderIdle();

    const deadline = Date.now() + 5000;
    let tree = 0;
    let n = { red: 0, blue: 0 };
    while (Date.now() < deadline) {
      tree = findChildWindow(frame, "SysTreeView32");
      if (tree) {
        n = countColoredText(tree);
        if (n.red >= 40 && n.blue >= 40) {
          break;
        }
      }
      await sleep(40);
    }
    if (!tree) {
      throw new Error("the bookmarks tree did not appear");
    }
    // each title is a long word, so the coloured glyphs are hundreds of pixels
    if (n.red < 40) {
      throw new Error(`the bold red bookmark is not red (${n.red} red px, ${n.blue} blue px)`);
    }
    if (n.blue < 40) {
      throw new Error(`the blue bookmark is not blue (${n.red} red px, ${n.blue} blue px)`);
    }
    console.log(`  outline colors reach the bookmarks tree: ${n.red} red px, ${n.blue} blue px ✓`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
