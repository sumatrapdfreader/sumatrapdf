// Editing an annotation while the background annotation-loading thread walks
// the document's pages used to be a use-after-free: loading a page has MuPDF
// generate every annotation's appearance stream, reading the same pdf objects
// SetRect() rewrites, and page loading did not hold docLock.
//
// Only an asan build can see it, so this is ad-hoc rather than part of the
// suite. Run:
//   bun cmd/build.ts -asan
//   $env:SUMATRA_TEST_EXE = "$PWD\out\dbg64_asan\SumatraPDF-static.exe"
//   bun tests/ad-hoc-annot-edit-race.ts --no-build

import { mkdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, uniquePipeName } from "./control.ts";
import { cmdId, EXE, runStandalone, tmpPath } from "./util.ts";
import {
  clientToScreen,
  getClientRect,
  packCoords,
  sendMessage,
  setCursorPos,
  sleep,
  MK_LBUTTON,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { findCanvas, killAndWait, sendCommandSync, waitForFrame } from "./win-automation.ts";

const N_PAGES = 400;
const ROUNDS = 4;
const DRAGS = 60;

// Every page carries a square in the middle (the drag target) plus five free
// text annotations and an ink scribble, none with an /AP: MuPDF has to lay
// them out when the page is loaded, which is what keeps the loading thread
// busy long enough to overlap with the edits.
function makePdf(): string {
  const objs: string[] = [];
  const kids: string[] = [];
  const perPage = 8; // page + square + 5 free text + ink
  for (let i = 0; i < N_PAGES; i++) {
    kids.push(`${4 + i * perPage} 0 R`);
  }
  objs.push("<< /Type /Catalog /Pages 2 0 R >>");
  objs.push(`<< /Type /Pages /Count ${N_PAGES} /Kids [${kids.join(" ")}] >>`);
  objs.push("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
  const lorem =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore " +
    "et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut " +
    "aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse.";
  const inkPts: string[] = [];
  for (let k = 0; k < 300; k++) {
    inkPts.push(`${60 + (k % 200)} ${80 + (k % 90)}`);
  }
  const inkList = `[[${inkPts.join(" ")}]]`;
  for (let i = 0; i < N_PAGES; i++) {
    const pageNo = 4 + i * perPage;
    const annots: string[] = [];
    for (let k = 1; k < perPage; k++) {
      annots.push(`${pageNo + k} 0 R`);
    }
    objs.push(`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${annots.join(" ")}] >>`);
    objs.push(
      `<< /Type /Annot /Subtype /Square /P ${pageNo} 0 R /Rect [206 296 406 496] ` +
        `/C [1 0 0] /IC [1 1 0.6] /BS << /W 2 >> /T (a${i}) /Contents (square ${i}) >>`,
    );
    for (let k = 0; k < 5; k++) {
      objs.push(
        `<< /Type /Annot /Subtype /FreeText /P ${pageNo} 0 R /Rect [40 ${700 - k * 30} 560 ${730 - k * 30}] ` +
          `/DA (/Helv 9 Tf 0 g) /BS << /W 1 >> /T (t${i}) /Contents (${lorem}) >>`,
      );
    }
    objs.push(
      `<< /Type /Annot /Subtype /Ink /P ${pageNo} 0 R /Rect [40 60 300 180] /C [0 0 1] ` +
        `/BS << /W 2 >> /InkList ${inkList} >>`,
    );
  }

  let out = "%PDF-1.7\n";
  const offsets: number[] = [0];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(out.length);
    out += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefPos = out.length;
  out += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (let i = 1; i <= objs.length; i++) {
    out += `${String(offsets[i]).padStart(10, "0")} 00000 n \n`;
  }
  out += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xrefPos}\n%%EOF\n`;
  return out;
}

// like launchControlled(), but keeps the app's stderr so an asan report can be
// read back instead of being thrown away
async function launchWithStderr(args: string[], errPath: string) {
  const pipe = uniquePipeName();
  const proc = Bun.spawn([EXE, "-for-testing", "-dbg-control", pipe, ...args], {
    stdout: "ignore",
    stderr: Bun.file(errPath),
  });
  const client = await ControlClient.connect(pipe);
  const frame = await waitForFrame(proc.pid!);
  if (!frame) {
    client.close();
    throw new Error("ad-hoc-annot-edit-race: no main window");
  }
  return { proc, client, frame };
}

export async function testit(): Promise<void> {
  if (!/asan/i.test(EXE)) {
    console.log(
      "ad-hoc-annot-edit-race: needs an asan build to see anything. Run:\n" +
        "  bun cmd/build.ts -asan\n" +
        '  $env:SUMATRA_TEST_EXE = "$PWD\\out\\dbg64_asan\\SumatraPDF-static.exe"\n' +
        "  bun tests/ad-hoc-annot-edit-race.ts --no-build",
    );
    return;
  }

  const dir = tmpPath("ad-hoc-annot-edit-race");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "many-annots.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  for (let round = 0; round < ROUNDS; round++) {
    const errPath = join(dir, `stderr-${round}.txt`);
    writeFileSync(errPath, "");
    const { proc, client, frame } = await launchWithStderr(["-view", "single page", "-zoom", "fit page", pdf], errPath);
    try {
      await client.waitForRenderIdle();
      const canvas = findCanvas(frame);
      const cr = getClientRect(canvas);
      const mid = { x: Math.floor(cr.right / 2), y: Math.floor(cr.bottom / 2) };
      // turning on Edit PDF mode starts the annotation-loading thread
      sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
      await sleep(500);

      // move the square back and forth while the pages keep loading
      for (let i = 0; i < DRAGS && proc.exitCode === null; i++) {
        const to = { x: mid.x + (i % 2 === 0 ? 18 : -18), y: mid.y + (i % 2 === 0 ? 12 : -12) };
        const sp = clientToScreen(canvas, to.x, to.y);
        setCursorPos(sp.x, sp.y);
        sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(mid.x, mid.y));
        sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(mid.x, mid.y));
        sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(to.x, to.y));
        sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(to.x, to.y));
        await sleep(10);
      }
      // reads only the selected annotation, whose page is laid out (the
      // markup-annots dump would convert every loaded one, and pages that were
      // never laid out trip getZoomSafe's assert)
      await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
      if (proc.exitCode !== null) {
        throw new Error(`the app died in round ${round}`);
      }
    } finally {
      client.close();
      await killAndWait(proc);
    }
    if (statSync(errPath).size > 0) {
      const report = readFileSync(errPath, "utf8").split("\n").slice(0, 25).join("\n");
      throw new Error(`ad-hoc-annot-edit-race: asan reported an error in round ${round}:\n${report}`);
    }
  }

  console.log(`ad-hoc-annot-edit-race: OK (${ROUNDS} rounds x ${DRAGS} drags over ${N_PAGES} pages)`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
