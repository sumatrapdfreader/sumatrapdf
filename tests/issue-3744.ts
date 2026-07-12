// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/3744
//
// "Go to Next/Previous Favorite" jump to the nearest favorite (bookmark) page
// after / before the current page.
//
// We open a 25-page PDF (each page renders a big "Page N"), add favorites at
// pages 10 and 20 through the UI (CmdFavoriteAdd + dismiss its dialog), then
// drive the new commands and compare canvas screenshots: the same page renders
// identically, different pages differ. So we can assert navigation landed on
// the right favorite without reading any control text.
//
// Run:  bun tests/issue-3744.ts [--no-build]   (or via tests/all.ts)

import { mkdirSync, rmSync, writeFileSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, EXE, runStandalone, tmpPath } from "./util.ts";
import { waitForFrame, sendCommand, findCanvas } from "./win-automation.ts";
import { sleep, enumWindows, getWindowPid, getClassName, sendMessage, captureWindowToPng } from "./winapi.ts";

const WM_COMMAND = 0x111;
const IDOK = 1;

// minimal N-page PDF. Each page is filled with a distinct grey level so that
// screenshots differ per page *regardless of the view's zoom/scroll* - drawing
// only a "Page N" label isn't enough because the canvas can clip the right edge
// and hide the units digit (making "Page 10" look identical to "Page 1").
function makePdf(nPages: number): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R >>");
  const kids: string[] = [];
  for (let i = 0; i < nPages; i++) {
    kids.push(`${4 + i * 2} 0 R`);
  }
  body[2] = enc(`<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${nPages} >>`);
  body[3] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
  for (let i = 0; i < nPages; i++) {
    const po = 4 + i * 2;
    const co = po + 1;
    const grey = ((i + 1) / (nPages + 1)).toFixed(3);
    const stream = `${grey} g 0 0 612 792 re f 0 g BT /F1 60 Tf 72 690 Td (Page ${i + 1}) Tj ET`;
    body[po] = enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
        `/Resources << /Font << /F1 3 0 R >> >> /Contents ${co} 0 R >>`,
    );
    body[co] = enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);
  }
  const maxN = 3 + nPages * 2;
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0].length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n], enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let n = 1; n <= maxN; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-3744");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "issue-3744.pdf");
  writeFileSync(pdf, makePdf(25));
  // isolated appdata so the favorites we add can't touch the tester's settings
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "ReuseInstance = false\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
  );

  const cNextPage = cmdId("CmdGoToNextPage");
  const cFirstPage = cmdId("CmdGoToFirstPage");
  const cFavAdd = cmdId("CmdFavoriteAdd");
  const cNextFav = cmdId("CmdGoToNextFavorite");
  const cPrevFav = cmdId("CmdGoToPrevFavorite");

  Bun.spawnSync(["taskkill", "/F", "/IM", "SumatraPDF.exe"]);
  await sleep(300);
  const proc = Bun.spawn([EXE, "-for-testing", "-appdata", appdata, pdf], { stdout: "ignore", stderr: "ignore" });
  try {
    const frame = await waitForFrame(proc.pid!);
    await sleep(1500);
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("no canvas");
    }

    async function dismissDialog(): Promise<boolean> {
      for (let i = 0; i < 25; i++) {
        let dlg = 0;
        enumWindows((h) => {
          if (getWindowPid(h) === proc.pid! && getClassName(h) === "#32770") {
            dlg = h;
            return false;
          }
          return true;
        });
        if (dlg) {
          sendMessage(dlg, WM_COMMAND, IDOK, 0); // accept the "Add Favorite" dialog
          return true;
        }
        await sleep(80);
      }
      return false;
    }

    async function gotoPage(n: number): Promise<void> {
      sendCommand(frame, cFirstPage);
      await sleep(250);
      for (let i = 1; i < n; i++) {
        sendCommand(frame, cNextPage);
      }
      await sleep(450);
    }

    // add favorites at pages 10 and 20
    for (const p of [10, 20]) {
      await gotoPage(p);
      sendCommand(frame, cFavAdd);
      if (!(await dismissDialog())) {
        throw new Error(`Add Favorite dialog didn't appear for page ${p}`);
      }
      await sleep(400);
    }

    const cap = (name: string): Buffer => {
      const p = join(dir, name);
      if (!captureWindowToPng(canvas, p)) {
        throw new Error(`capture failed: ${name}`);
      }
      return readFileSync(p);
    };
    const eq = (a: Buffer, b: Buffer) => a.length === b.length && a.equals(b);

    await gotoPage(1);
    const page1 = cap("p1.png");

    sendCommand(frame, cNextFav); // 1 -> 10
    await sleep(700);
    const fav10 = cap("fav10.png");

    sendCommand(frame, cNextFav); // 10 -> 20
    await sleep(700);
    const fav20 = cap("fav20.png");

    sendCommand(frame, cNextFav); // 20 -> 20 (no wrap)
    await sleep(700);
    const fav20b = cap("fav20b.png");

    sendCommand(frame, cPrevFav); // 20 -> 10
    await sleep(700);
    const back10 = cap("back10.png");

    // assertions (compare rendered pages, not control text)
    const checks: [string, boolean][] = [
      ["next from page 1 moved off page 1", !eq(fav10, page1)],
      ["next advanced 10 -> 20 (different page)", !eq(fav20, fav10)],
      ["next at last favorite stays on 20 (no wrap)", eq(fav20b, fav20)],
      ["prev from 20 returns to the page-10 favorite", eq(back10, fav10)],
    ];
    let ok = true;
    for (const [label, pass] of checks) {
      console.log(`${pass ? "PASS" : "FAIL"} ${label}`);
      ok &&= pass;
    }
    if (!ok) {
      throw new Error("Go to Next/Previous Favorite navigation incorrect (issue #3744)");
    }
    console.log("PASS: Go to Next/Previous Favorite works (issue #3744)");
  } finally {
    proc.kill();
    await sleep(300);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
