// #4684 / #4116: select text with the keyboard. F7 (CmdSelectTextViaKeyboard)
// puts a caret in the page text; the arrow keys move it, Shift + arrows extend
// the selection, `v` turns on visual mode where plain arrows extend it, and
// `y` (or Ctrl + C) copies. The feature is unavailable for documents with no
// extractable text (images, comic books).
//
// Drives the app over -dbg-control (TestSelectTextKeyboard reports the caret
// position and the selected text) and posts real WM_COMMAND / key messages, so
// the command and key paths are exercised, not just the internals.
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, cmdId, runStandalone, tmpPath } from "./util";
import { FRAME_CLASS, sendCommandSync } from "./win-automation";
import { WM_CHAR, WM_KEYDOWN, WM_KEYUP, postMessage, sleep, waitForTopWindow } from "./winapi";

const VK_RIGHT = 0x27;
const VK_END = 0x23;
const VK_HOME = 0x24;

// 2 pages, one line of known text each. The line is long enough that Home/End
// and a few glyph steps are all within it.
const LINE1 = "The quick brown fox jumps over the lazy dog";
const LINE2 = "Second page has different words entirely";

function makeTextPdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R >>");
  body[2] = enc("<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>");
  body[6] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
  const lines = [LINE1, LINE2];
  for (let i = 0; i < 2; i++) {
    const pageObj = 3 + i;
    const contentObj = 10 + i;
    const stream = `BT /F1 18 Tf 72 700 Td (${lines[i]}) Tj ET`;
    body[contentObj] = enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);
    body[pageObj] = enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
        `/Resources << /Font << /F1 6 0 R >> >> /Contents ${contentObj} 0 R >>`,
    );
  }

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

// a 1x1 PNG: an image document, where the feature must stay unavailable
const PNG_1PX = Buffer.from(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==",
  "base64",
);

type State = {
  active: boolean;
  visual: boolean;
  canSelect: boolean;
  page: number;
  glyph: number;
  selRects: number;
  text: string;
  hasCaret: boolean;
};

function parseState(dump: string): State {
  const m = /active=(\d+) visual=(\d+) canSelect=(\d+) page=(-?\d+) glyph=(-?\d+) .* selRects=(\d+)/.exec(dump);
  if (!m) {
    throw new Error(`unexpected TestSelectTextKeyboard output:\n${dump}`);
  }
  const text = /^text=(.*)$/m.exec(dump);
  return {
    active: m[1] === "1",
    visual: m[2] === "1",
    canSelect: m[3] === "1",
    page: +m[4]!,
    glyph: +m[5]!,
    selRects: +m[6]!,
    text: text ? text[1]! : "",
    hasCaret: /^caret=/m.test(dump),
  };
}

async function getState(client: ControlClient): Promise<{ state: State; dump: string }> {
  const res = await client.request(ControlCommand.TestSelectTextKeyboard, []);
  const dump = String(res[1] ?? "");
  return { state: parseState(dump), dump };
}

async function waitForState(
  client: ControlClient,
  pred: (s: State) => boolean,
  timeoutMs = 4000,
): Promise<{ state: State; dump: string }> {
  const deadline = Date.now() + timeoutMs;
  let last = {
    state: {
      active: false,
      visual: false,
      canSelect: false,
      page: 0,
      glyph: 0,
      selRects: 0,
      text: "",
      hasCaret: false,
    },
    dump: "",
  };
  while (Date.now() < deadline) {
    last = await getState(client);
    if (pred(last.state)) {
      return last;
    }
    await sleep(25);
  }
  throw new Error(`keyboard-selection state did not match in time\n${last.dump}`);
}

function pressVKey(hwnd: number, vk: number): void {
  postMessage(hwnd, WM_KEYDOWN, vk, 0);
  postMessage(hwnd, WM_KEYUP, vk, 0);
}

async function testTextPdf(): Promise<void> {
  const pdf = tmpPath("issue-4684.pdf");
  writeFileSync(pdf, makeTextPdf());

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) {
        throw new Error("no frame window");
      }
      await client.waitForRenderIdle();

      const fail = (msg: string, dump: string) => {
        throw new Error(`${msg}\nstate dump:\n${dump}`);
      };

      let { state, dump } = await getState(client);
      if (!state.canSelect) {
        fail("keyboard selection should be available for a text PDF", dump);
      }
      if (state.active) {
        fail("mode should start off", dump);
      }

      // turn the mode on
      sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
      ({ state, dump } = await waitForState(client, (s) => s.active));
      if (state.page !== 1 || state.glyph !== 0) {
        fail("caret should start at the first glyph of page 1", dump);
      }
      if (!state.hasCaret) {
        fail("caret should have a screen rect", dump);
      }
      if (state.selRects !== 0) {
        fail("moving in without selecting should not select anything", dump);
      }

      // visual mode: plain arrows extend the selection
      postMessage(frame, WM_CHAR, "v".charCodeAt(0), 0);
      ({ state, dump } = await waitForState(client, (s) => s.visual));

      for (let i = 0; i < 9; i++) {
        pressVKey(frame, VK_RIGHT);
      }
      ({ state, dump } = await waitForState(client, (s) => s.glyph === 9 && s.selRects !== 0));
      if (state.glyph !== 9) {
        fail("9 right arrows should move the caret 9 glyphs", dump);
      }
      if (state.selRects === 0) {
        fail("arrows in visual mode should select text", dump);
      }
      if (state.text !== LINE1.slice(0, 9)) {
        fail(`selected text should be "${LINE1.slice(0, 9)}", is "${state.text}"`, dump);
      }

      // End extends to the end of the line
      pressVKey(frame, VK_END);
      ({ state, dump } = await waitForState(client, (s) => s.text === LINE1));

      // Home from there collapses back toward the line start
      pressVKey(frame, VK_HOME);
      ({ state, dump } = await waitForState(client, (s) => s.glyph === 0));

      // 'y' copies the selection and leaves the mode
      pressVKey(frame, VK_END); // select something again
      await waitForState(client, (s) => s.text === LINE1);
      postMessage(frame, WM_CHAR, "y".charCodeAt(0), 0);
      ({ state, dump } = await waitForState(client, (s) => !s.active));

      // and the command toggles the mode back off
      sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
      ({ state, dump } = await waitForState(client, (s) => s.active));
      sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
      ({ state, dump } = await waitForState(client, (s) => !s.active));
    },
    [pdf],
  );
}

async function testImageDoc(): Promise<void> {
  const png = tmpPath("issue-4684.png");
  writeFileSync(png, PNG_1PX);

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) {
        throw new Error("no frame window");
      }
      await client.waitForRenderIdle();
      let { state, dump } = await getState(client);
      if (state.canSelect) {
        throw new Error(`keyboard selection must be unavailable for an image\n${dump}`);
      }
      // and the command must not turn it on
      sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
      ({ state, dump } = await getState(client));
      if (state.active) {
        throw new Error(`mode must not turn on for an image\n${dump}`);
      }
    },
    [png],
  );
}

export async function testit(): Promise<void> {
  await testTextPdf();
  await testImageDoc();
  console.log("issue-4684: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
