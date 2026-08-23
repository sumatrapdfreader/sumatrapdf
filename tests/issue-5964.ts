// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5964
//
// A PDF prepared for signing has an empty signature field. Clicking it used to
// do nothing - the only way to sign was to find Sign Document in a menu. Now a
// click opens the Sign Document dialog with that field already chosen in the
// placement drop-down.
//
// The fixture's signature field covers nearly the whole page, so a click in the
// middle of the canvas lands on it without any coordinate conversion. The field
// is named CEO, which is what the placement entry has to mention.
//
// Run:  bun tests/issue-5964.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { clickAt, findCanvas, killAndWait, launchControlled } from "./win-automation.ts";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getClientRect,
  getControlText,
  getWindowPid,
  getWindowText,
  postMessage,
  sleep,
  WM_CLOSE,
} from "./winapi.ts";

// one page, one empty signature field named CEO covering most of the page
function writePdfWithBigSigField(path: string): void {
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [6 0 R] /SigFlags 3 >> >>";
  objs[2] = "<< /Type /Pages /Kids [3 0 R] /Count 1 >>";
  objs[3] =
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] /Resources << /Font << /F1 5 0 R >> >> " +
    "/Contents 4 0 R /Annots [6 0 R] >>";
  const content = "BT /F1 10 Tf 10 185 Td (please sign) Tj ET";
  objs[4] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
  objs[5] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>";
  objs[6] =
    "<< /Type /Annot /Subtype /Widget /FT /Sig /T (CEO) /Rect [20 20 280 170] /F 4 /P 3 0 R /DA (/F1 0 Tf 0 g) >>";

  let pdf = "%PDF-1.7\n";
  const offsets: number[] = [];
  for (let i = 1; i < objs.length; i++) {
    offsets[i] = pdf.length;
    pdf += `${i} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefAt = pdf.length;
  pdf += `xref\n0 ${objs.length}\n0000000000 65535 f \n`;
  for (let i = 1; i < objs.length; i++) {
    pdf += String(offsets[i]).padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${objs.length} /Root 1 0 R >>\nstartxref\n${xrefAt}\n%%EOF\n`;
  writeFileSync(path, pdf, "latin1");
}

// a top-level window of this process whose title is `title`, or 0
function findWindowByTitle(pid: number, title: string): number {
  let found = 0;
  enumWindows((h) => {
    if (getWindowPid(h) !== pid || getWindowText(h) !== title) {
      return true;
    }
    found = h;
    return false;
  });
  return found;
}

// the dialog enumerates certificates before it shows, which can take a while
async function waitForWindowByTitle(
  pid: number,
  title: string,
  timeoutMs = 12000 * SLOW_BUILD_FACTOR,
): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const h = findWindowByTitle(pid, title);
    if (h) {
      return h;
    }
    await sleep(100);
  }
  return 0;
}

// text of the placement ComboBox (the one that names a signature field or
// "new signature"). The dialog also has a certificate drop-down now (#5965).
function comboText(hwnd: number): string {
  let first = "";
  let placement = "";
  enumChildWindows(hwnd, (h) => {
    if (getClassName(h) !== "ComboBox") {
      return true;
    }
    const t = getControlText(h);
    if (!first) {
      first = t;
    }
    if (/signature|CEO|page/i.test(t)) {
      placement = t;
      return false;
    }
    return true;
  });
  return placement || first;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5964.pdf");
  writePdfWithBigSigField(pdf);

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.setNotificationsEnabled(false);
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-5964: could not find the canvas window");
    }
    const pid = getWindowPid(frame);
    if (findWindowByTitle(pid, "Sign Document")) {
      throw new Error("issue-5964: Sign Document was already open before the click");
    }

    // click the middle of the canvas, which is inside the signature field
    const cr = getClientRect(canvas);
    if (cr.right < 100 || cr.bottom < 100) {
      throw new Error(`issue-5964: canvas is ${cr.right}x${cr.bottom}, too small to click into the field`);
    }
    const cx = Math.floor(cr.right / 2);
    const cy = Math.floor(cr.bottom / 2);

    // A click is hit-tested against the page's widgets through
    // EngineMupdf::GetFzPageInfoCanFail(), which TryLocks the engine's page and
    // render locks and gives up rather than block the ui thread behind a render
    // (issue #4145). A click that loses that race is dropped silently, so a
    // single click proves nothing: render-idle doesn't stop the render thread
    // from picking up follow-up work, and under asan it's slow enough to still
    // be holding a lock. Click again until the dialog shows up.
    let dlg = 0;
    let clicks = 0;
    while (!dlg && clicks < 6) {
      if (clicks > 0) {
        await client.waitForRenderIdle();
        await sleep(500);
      }
      await clickAt(canvas, cx, cy, 0);
      clicks++;
      dlg = await waitForWindowByTitle(pid, "Sign Document", 1500 * SLOW_BUILD_FACTOR);
    }
    if (!dlg) {
      throw new Error(`issue-5964: ${clicks} clicks on the empty signature field didn't open Sign Document`);
    }

    // and it has to be aimed at the field that was clicked, not at "new
    // signature on page N". The window shows up before its controls are
    // filled in, so give the drop-down a moment.
    let placement = "";
    for (const deadline = Date.now() + 3000 * SLOW_BUILD_FACTOR; Date.now() < deadline;) {
      placement = comboText(dlg);
      if (placement) {
        break;
      }
      await sleep(100);
    }
    if (!placement.includes("CEO")) {
      throw new Error(`issue-5964: placement is '${placement}', want the clicked field (CEO)`);
    }
    console.log(`  clicking the empty signature field opened Sign Document on '${placement}' ✓`);

    postMessage(dlg, WM_CLOSE, 0, 0);
    await sleep(200);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
