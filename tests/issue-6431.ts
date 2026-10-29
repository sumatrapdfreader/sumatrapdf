// #6431: EPUB 3 footnotes (<a epub:type="noteref"> to <aside epub:type="footnote">).
//  - the note is not part of the reading flow: it moves to the end of its
//    chapter instead of following the paragraph that cites it
//  - clicking the noteref shows the note in the citation popup instead of
//    jumping to it, even with hover previews off (the default)
// Run: bun tests/issue-6431.ts [--no-build]
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, extractPageText, makeZip, runStandalone, tmpPath, USE_NG, type ZipEntry } from "./util";
import { MK_LBUTTON, packCoords, sendMessage, sleep, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MOUSEMOVE } from "./winapi";
import { findCanvas, FRAME_CLASS, ngInput } from "./win-automation";
import { waitForTopWindow } from "./winapi";

function makeEpub(): Buffer {
  const enc = new TextEncoder();
  const container =
    `<?xml version="1.0"?>\n<container version="1.0" ` +
    `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
    `<rootfile full-path="OPS/content.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
    `<dc:identifier id="id">issue-6431</dc:identifier>` +
    `<dc:title>Notes</dc:title><dc:language>en</dc:language></metadata>` +
    `<manifest><item id="c1" href="c1.xhtml" media-type="application/xhtml+xml"/></manifest>` +
    `<spine><itemref idref="c1"/></spine></package>`;
  const body =
    `<p>FIRSTPARA cites a note<a epub:type="noteref" href="#n1" id="r1"><sup>[1]</sup></a> here.</p>` +
    `<aside epub:type="footnote" id="n1"><a href="#r1"></a><p>NOTETEXT explains it.</p></aside>` +
    `<p>LASTPARA ends the chapter.</p>`;
  const page =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">` +
    `<head><title>Notes</title></head><body>${body}</body></html>`;
  const entries: ZipEntry[] = [
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OPS/content.opf", data: enc.encode(opf) },
    { name: "OPS/c1.xhtml", data: enc.encode(page) },
  ];
  return makeZip(entries);
}

async function popupVisible(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 8000;
  let raw = "";
  while (Date.now() < deadline) {
    raw = String((await client.request(ControlCommand.TestRefHover, []))[1] ?? "");
    if (/visible=1/.test(raw)) {
      return raw;
    }
    await sleep(50);
  }
  throw new Error(`#6431: clicking the noteref did not show the popup: ${raw}`);
}

export async function testit(): Promise<void> {
  const epub = tmpPath("issue-6431.epub");
  writeFileSync(epub, makeEpub());

  const text = extractPageText(epub, 1);
  const last = text.indexOf("LASTPARA");
  const note = text.indexOf("NOTETEXT");
  if (last < 0 || note < 0 || note < last) {
    throw new Error(`#6431: the footnote should come after the last paragraph, got:\n${JSON.stringify(text)}`);
  }

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      await client.waitForRenderIdle();
      await client.setNotificationsEnabled(false);
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      const canvas = findCanvas(frame);

      const res = await client.request(ControlCommand.TestRefHover, ["noteref", 0, 0]);
      const raw = String(res[1] ?? "");
      const m = /noteref=(-?\d+),(-?\d+)/.exec(raw);
      if (res[0] !== 0 || !m) {
        throw new Error(`#6431: no noteref link on the page: ${raw}`);
      }
      // ng: a frame point in pixels, clicked in dips (px * scale)
      if (USE_NG) {
        const scale = +(/ scale=([\d.]+)/.exec(raw)?.[1] ?? 1);
        await ngInput(client, ["click", Math.round(+m[1]! * scale), Math.round(+m[2]! * scale), 0, 0]);
      } else {
        const lp = packCoords(+m[1]!, +m[2]!);
        sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, lp);
        sendMessage(canvas, WM_LBUTTONUP, 0, lp);
        // Windows sends a move without the mouse moving (a window appearing
        // under the cursor); off the link, it must not cancel the popup
        sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(5, 5));
      }
      await popupVisible(client);
    },
    [epub],
  );
  console.log("#6431: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
