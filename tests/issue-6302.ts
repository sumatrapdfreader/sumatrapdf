// #6302: a highlight made from a keyboard selection must leave caret mode on,
// with the caret at the free end of that highlight. The highlighter brush
// must do the same, and a selection finished while it is on becomes a highlight.
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import { FRAME_CLASS, sendCommandSync } from "./win-automation";
import { WM_KEYDOWN, WM_KEYUP, postMessage, sleep, waitForTopWindow, postChar } from "./winapi";

const VK_RIGHT = 0x27;
const LINE = "The quick brown fox jumps over the lazy dog";

function makeTextPdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const stream = `BT /F1 18 Tf 72 700 Td (${LINE}) Tj ET`;
  const body: Record<number, Buffer> = {
    1: enc("<< /Type /Catalog /Pages 2 0 R >>"),
    2: enc("<< /Type /Pages /Kids [3 0 R] /Count 1 >>"),
    3: enc(
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] " +
        "/Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
    ),
    4: enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>"),
    5: enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`),
  };
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0]!.length;
  for (let n = 1; n <= 5; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n]!, enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = "xref\n0 6\n0000000000 65535 f \n";
  for (let n = 1; n <= 5; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

type State = {
  active: boolean;
  visual: boolean;
  page: number;
  glyph: number;
  selRects: number;
  text: string;
};

function parseState(dump: string): State {
  const m = /active=(\d+) visual=(\d+) canSelect=\d+ page=(-?\d+) glyph=(-?\d+) .* selRects=(\d+)/.exec(dump);
  if (!m) {
    throw new Error(`unexpected TestSelectTextKeyboard output:\n${dump}`);
  }
  const text = /^text=(.*)$/m.exec(dump);
  return {
    active: m[1] === "1",
    visual: m[2] === "1",
    page: +m[3]!,
    glyph: +m[4]!,
    selRects: +m[5]!,
    text: text ? text[1]! : "",
  };
}

async function getState(client: ControlClient): Promise<{ state: State; dump: string }> {
  const res = await client.request(ControlCommand.TestSelectTextKeyboard, []);
  const dump = String(res[1] ?? "");
  return { state: parseState(dump), dump };
}

async function waitForState(client: ControlClient, pred: (s: State) => boolean): Promise<State> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let last = "";
  while (Date.now() < deadline) {
    const got = await getState(client);
    last = got.dump;
    if (pred(got.state)) {
      return got.state;
    }
    await sleep(25);
  }
  throw new Error(`keyboard-selection state did not match in time\n${last}`);
}

function press(hwnd: number, vk: number): void {
  postMessage(hwnd, WM_KEYDOWN, vk, 0);
  postMessage(hwnd, WM_KEYUP, vk, 0);
}

async function annotCount(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const dump = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`could not read annotations:\n${dump}`);
  }
  return [...dump.matchAll(/type=Highlight /g)].length;
}

async function waitAnnots(client: ControlClient, n: number): Promise<void> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let last = -1;
  while (Date.now() < deadline) {
    last = await annotCount(client);
    if (last === n) {
      return;
    }
    await sleep(25);
  }
  throw new Error(`expected ${n} annotations, have ${last}`);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6302.pdf");
  writeFileSync(pdf, makeTextPdf());

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) {
        throw new Error("no frame window");
      }
      const tab = await client.request(ControlCommand.TestCurrentTab, []);
      if (tab[0] !== 0) {
        throw new Error(`document did not open (${pdf}): ${String(tab[1] ?? "")}`);
      }
      await client.waitForRenderIdle();

      sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
      await waitForState(client, (s) => s.active && s.glyph === 0);

      await postChar(frame, "v");
      for (let i = 0; i < 9; i++) {
        press(frame, VK_RIGHT);
      }
      const selected = await waitForState(client, (s) => s.text === LINE.slice(0, 9));

      sendCommandSync(frame, cmdId("CmdCreateAnnotHighlight"));
      const afterA = await waitForState(
        client,
        (s) => s.active && !s.visual && s.selRects === 0 && s.glyph === selected.glyph,
      );
      if (afterA.page !== selected.page) {
        throw new Error(`caret page changed after highlight: ${afterA.page}`);
      }
      await waitAnnots(client, 1);

      // the next selection starts where the highlight ended
      await postChar(frame, "v");
      for (let i = 0; i < 5; i++) {
        press(frame, VK_RIGHT);
      }
      const again = await waitForState(client, (s) => s.selRects > 0 && s.glyph === selected.glyph + 5);
      if (again.text !== LINE.slice(selected.glyph, selected.glyph + 5)) {
        throw new Error(`next selection should start after the highlight, is "${again.text}"`);
      }
      await postChar(frame, "v");
      press(frame, VK_RIGHT);
      const parked = await waitForState(client, (s) => s.active && !s.visual && s.selRects === 0);

      // the highlighter stays usable with the caret, and finishing a selection highlights it
      sendCommandSync(frame, cmdId("CmdAnnotationHighlightBrush"));
      await waitForState(client, (s) => s.active && s.selRects === 0);
      await postChar(frame, "v");
      for (let i = 0; i < 4; i++) {
        press(frame, VK_RIGHT);
      }
      await waitForState(client, (s) => s.glyph === parked.glyph + 4 && s.selRects > 0);
      await postChar(frame, "v");
      const afterBrush = await waitForState(
        client,
        (s) => s.active && s.selRects === 0 && s.glyph === parked.glyph + 4,
      );
      if (!afterBrush.active) {
        throw new Error("highlighter commit left caret mode");
      }
      await waitAnnots(client, 2);
    },
    [pdf],
  );
  console.log("issue-6302: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
