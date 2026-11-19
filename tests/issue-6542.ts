// #6542: moving the keyboard selection (F7) caret past the edge of the view
// scrolls just enough to show it (about one line), and in single page mode
// switches to the caret's page.
import { writeFileSync } from "node:fs";
import type { ControlClient } from "./control";
import { ControlCommand, withControlledSumatra } from "./control";
import { getState, waitForState } from "./issue-4684";
import { EXE, cmdId, runStandalone, tmpPath } from "./util";
import { FRAME_CLASS, sendCommandSync } from "./win-automation";
import { waitForTopWindow } from "./winapi";

const kLines = 40;
const kLineGap = 17; // pt between baselines
const kPages = 2;

function makePdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  const kids = Array.from({ length: kPages }, (_, i) => `${3 + i} 0 R`).join(" ");
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R >>");
  body[2] = enc(`<< /Type /Pages /Kids [${kids}] /Count ${kPages} >>`);
  body[6] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
  for (let p = 0; p < kPages; p++) {
    let stream = `BT /F1 14 Tf 72 750 Td (Page ${p + 1} line 1 of the text) Tj`;
    for (let l = 2; l <= kLines; l++) stream += ` 0 -${kLineGap} Td (Page ${p + 1} line ${l} of the text) Tj`;
    stream += " ET";
    body[10 + p] = enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);
    body[3 + p] = enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
        `/Resources << /Font << /F1 6 0 R >> >> /Contents ${10 + p} 0 R >>`,
    );
  }

  const maxN = 10 + kPages - 1;
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0]!.length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n] ?? enc("null"), enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let n = 1; n <= maxN; n++) xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

// view position in page units, across pages
async function viewY(client: ControlClient): Promise<{ page: number; y: number }> {
  await client.waitForRenderIdle();
  const res = await client.request(ControlCommand.TestScrollState, []);
  const raw = String(res[1] ?? "");
  const m = /^OK page=(\d+) x=[-\d.e]+ y=([-\d.e]+)/.exec(raw);
  if (!m) throw new Error(`issue-6542: TestScrollState: ${raw}`);
  return { page: +m[1]!, y: (+m[1]! - 1) * 792 + +m[2]! };
}

async function caretDown(client: ControlClient, frame: number, name = "CmdCaretDown"): Promise<void> {
  const { state } = await getState(client);
  sendCommandSync(frame, cmdId(name));
  await waitForState(client, (s) => s.page !== state.page || s.glyph !== state.glyph);
}

async function run(client: ControlClient, frame: number): Promise<void> {
  await client.setNotificationsEnabled(false);
  const cmd = (name: string) => sendCommandSync(frame, cmdId(name));

  // continuous, zoomed in: only a few lines fit
  cmd("CmdZoom400");
  await client.waitForRenderIdle();
  cmd("CmdSelectTextViaKeyboard");
  await waitForState(client, (s) => s.active);
  cmd("CmdCaretDocStart");
  await waitForState(client, (s) => s.page === 1 && s.glyph === 0);

  // walk down; once the view starts scrolling each line scrolls it about a line
  let prev = await viewY(client);
  let scrolled = 0;
  for (let i = 0; i < kLines - 2 && scrolled < 8; i++) {
    await caretDown(client, frame);
    const cur = await viewY(client);
    const dy = cur.y - prev.y;
    if (dy > 2 * kLineGap) {
      const { dump } = await getState(client);
      throw new Error(`issue-6542: one line down scrolled ${dy.toFixed(1)}pt (line is ${kLineGap}pt)\n${dump}`);
    }
    if (dy > 0) scrolled++;
    prev = cur;
  }
  if (scrolled === 0) throw new Error("issue-6542: the view never scrolled");

  // single page, whole page shown: stepping past the last line shows the next page
  cmd("CmdSinglePageView");
  cmd("CmdToggleContinuousView"); // on by default
  cmd("CmdZoomFitPage");
  await client.waitForRenderIdle();
  cmd("CmdCaretDocStart");
  await waitForState(client, (s) => s.page === 1 && s.glyph === 0);
  for (let i = 0; i < kLines; i++) await caretDown(client, frame);
  await waitForState(client, (s) => s.page === 2);
  let v = await viewY(client);
  if (v.page !== 2) throw new Error(`issue-6542: caret on page 2, view on page ${v.page}`);

  await caretDown(client, frame, "CmdCaretUp");
  await waitForState(client, (s) => s.page === 1);
  v = await viewY(client);
  if (v.page !== 1) throw new Error(`issue-6542: caret on page 1, view on page ${v.page}`);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6542.pdf");
  writeFileSync(pdf, makePdf());
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) throw new Error("issue-6542: no frame window");
      await run(client, frame);
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
