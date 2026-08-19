// #1914: a PDF can navigate from a pushbutton form field - a /Widget annotation
// whose /A is a GoTo action - not just from a /Link annotation. mupdf's
// fz_load_links() only returns /Link annots, so those buttons did nothing from
// 3.2 on (they worked in 3.1.2, and work in Acrobat and browsers).
//
// Checks the engine really reports the button as a link destination, and that
// clicking it in the app navigates.
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import { FRAME_CLASS, clickAt, findCanvas, sendCommandSync } from "./win-automation";
import { sleep, waitForTopWindow } from "./winapi";

// 2 pages. Page 1 carries a pushbutton widget (/FT /Btn with the pushbutton
// flag) whose action jumps to page 2; there is no /Link annotation anywhere.
function makePushButtonPdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [7 0 R] >> >>");
  body[2] = enc("<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>");
  body[6] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");

  // the button: /Ff 65536 is the pushbutton flag, /A a GoTo to page 2
  body[7] = enc(
    `<< /Type /Annot /Subtype /Widget /FT /Btn /Ff 65536 /T (Goto) /P 3 0 R ` +
      `/Rect [72 600 172 660] /F 4 /MK << /BC [0 0 0] >> /A << /S /GoTo /D [4 0 R /Fit] >> >>`,
  );

  const stream1 = "BT /F1 18 Tf 72 700 Td (page one) Tj ET";
  body[10] = enc(`<< /Length ${stream1.length} >>\nstream\n${stream1}\nendstream`);
  const stream2 = "BT /F1 18 Tf 72 700 Td (page two) Tj ET";
  body[11] = enc(`<< /Length ${stream2.length} >>\nstream\n${stream2}\nendstream`);

  body[3] = enc(
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [7 0 R] ` +
      `/Resources << /Font << /F1 6 0 R >> >> /Contents 10 0 R >>`,
  );
  body[4] = enc(
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
      `/Resources << /Font << /F1 6 0 R >> >> /Contents 11 0 R >>`,
  );

  const maxN = 12;
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
  for (let n = 1; n <= maxN; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

type LinkState = { page: number; count: number; rects: { x: number; y: number; dx: number; dy: number }[] };

async function getLinkState(client: ControlClient): Promise<{ st: LinkState; dump: string }> {
  const res = await client.request(ControlCommand.TestKeyboardLinkFollow, []);
  const dump = String(res[1] ?? "");
  const head = /page=(\d+) count=(\d+)/.exec(dump);
  if (!head) {
    throw new Error(`unexpected TestKeyboardLinkFollow output:\n${dump}`);
  }
  const rects: LinkState["rects"] = [];
  for (const m of dump.matchAll(/^link=\d+ page=\d+ rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) hint=[A-Z]+$/gm)) {
    rects.push({ x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! });
  }
  return { st: { page: +head[1]!, count: +head[2]!, rects }, dump };
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-1914.pdf");
  writeFileSync(pdf, makePushButtonPdf());

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      // engine level: the pushbutton must show up as a link to page 2
      const res = await client.request(ControlCommand.TestPageLinks, [pdf, 1]);
      const links = String(res[1] ?? "");
      if (res[0] !== 0) {
        throw new Error(`page 1 should have a link (the pushbutton), got:\n${links}`);
      }
      const dests = [...links.matchAll(/^kind=(\S+) page=(-?\d+)/gm)];
      if (dests.length !== 1 || dests[0]![2] !== "2") {
        throw new Error(`page 1 should have exactly one link to page 2, got:\n${links}`);
      }

      // app level: clicking the button navigates there
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) {
        throw new Error("no frame window");
      }
      await client.waitForRenderIdle();
      const canvas = findCanvas(frame);
      if (!canvas) {
        throw new Error("no canvas window");
      }

      // ask where the link is on screen (link-following labels every link it
      // finds, so this also proves the button is in the app's link list)
      sendCommandSync(frame, cmdId("CmdToggleKeyboardLinkFollowing"));
      const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
      let st: LinkState = { page: 0, count: 0, rects: [] };
      let dump = "";
      while (Date.now() < deadline) {
        ({ st, dump } = await getLinkState(client));
        if (st.count === 1 && st.rects.length === 1) {
          break;
        }
        await sleep(25);
      }
      if (st.page !== 1) {
        throw new Error(`should start on page 1\n${dump}`);
      }
      if (st.count !== 1 || st.rects.length !== 1) {
        throw new Error(`the pushbutton should be a followable link\n${dump}`);
      }
      const r = st.rects[0]!;

      // leave the mode and click the button like a user would
      sendCommandSync(frame, cmdId("CmdToggleKeyboardLinkFollowing"));
      await clickAt(canvas, r.x + r.dx / 2, r.y + r.dy / 2, 200);
      const deadline2 = Date.now() + 4000 * SLOW_BUILD_FACTOR;
      while (Date.now() < deadline2) {
        ({ st, dump } = await getLinkState(client));
        if (st.page === 2) {
          break;
        }
        await sleep(25);
      }
      if (st.page !== 2) {
        throw new Error(`clicking the pushbutton should go to page 2, on page ${st.page}\n${dump}`);
      }
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
