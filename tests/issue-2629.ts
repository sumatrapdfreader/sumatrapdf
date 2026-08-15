// #2629: Zathura-style keyboard link following. `f`
// (CmdToggleKeyboardLinkFollowing) numbers the links visible on screen 1..9 and
// pressing that digit follows the link. The numbering is recalculated shortly
// after scrolling stops, and the whole feature is unavailable for formats whose
// pages can't have links (comic books, image folders, images).
//
// Drives the app over -dbg-control (TestKeyboardLinkFollow reports the mode and
// the numbered targets) and posts the real WM_COMMAND / WM_CHAR messages, so the
// accelerator and key paths are exercised, not just the internals.
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, cmdId, runStandalone, tmpPath } from "./util";
import { FRAME_CLASS, sendCommandSync } from "./win-automation";
import { VK_ESCAPE, WM_CHAR, WM_KEYDOWN, WM_KEYUP, postMessage, sleep, waitForTopWindow } from "./winapi";

const VK_F = 0x46;

// An unmodified key press. Modified shortcuts (Shift + F) can't be driven this
// way: TranslateAccelerator asks GetKeyState() for the modifiers and posted key
// messages don't set it, so a posted Shift + F arrives as a plain F. The
// Shift + F binding is checked through the accel= field of the state dump.
function pressVKey(hwnd: number, vk: number): void {
  postMessage(hwnd, WM_KEYDOWN, vk, 0);
  postMessage(hwnd, WM_KEYUP, vk, 0);
}

// 3 pages. Page 1 carries 3 link annotations (2 internal + 1 URL) in the top
// half and 1 more near the bottom, so scrolling changes what is on screen.
// Page height is 2000 so a single page doesn't fit in the window.
function makeLinkedPdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R >>");
  body[2] = enc("<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R] /Count 3 >>");

  // link rects are in PDF space (origin bottom-left); page is 612x2000
  // top-of-page links (visible when scrolled to the top)
  const links = [
    { rect: "[72 1900 300 1930]", dest: "/Dest [4 0 R /Fit]" }, // -> page 2
    { rect: "[72 1850 300 1880]", dest: "/Dest [5 0 R /Fit]" }, // -> page 3
    { rect: "[72 1800 300 1830]", dest: "/A << /S /URI /URI (https://example.com/) >>" },
    // bottom-of-page link, only visible after scrolling down
    { rect: "[72 100 300 130]", dest: "/Dest [5 0 R /Fit]" },
  ];
  const linkObjs: number[] = [];
  let objNo = 20;
  for (const l of links) {
    body[objNo] = enc(`<< /Type /Annot /Subtype /Link /Rect ${l.rect} /Border [0 0 0] ${l.dest} >>`);
    linkObjs.push(objNo);
    objNo++;
  }
  const annots = linkObjs.map((n) => `${n} 0 R`).join(" ");

  for (let i = 0; i < 3; i++) {
    const pageObj = 3 + i;
    const contentObj = 10 + i;
    const stream = `BT /F1 24 Tf 72 1000 Td (Page ${i + 1}) Tj ET`;
    body[contentObj] = enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);
    const annotsEntry = i === 0 ? ` /Annots [${annots}]` : "";
    body[pageObj] = enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 2000] ` +
        `/Resources << /Font << /F1 6 0 R >> >> /Contents ${contentObj} 0 R${annotsEntry} >>`,
    );
  }
  body[6] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");

  const maxN = 24;
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

type State = { active: boolean; canFollow: boolean; page: number; count: number; accel: string };

function parseState(dump: string): State {
  const m = /active=(\d+) canFollow=(\d+) page=(-?\d+) count=(\d+) accel=(.*)/.exec(dump);
  if (!m) {
    throw new Error(`unexpected TestKeyboardLinkFollow output:\n${dump}`);
  }
  return { active: m[1] === "1", canFollow: m[2] === "1", page: +m[3]!, count: +m[4]!, accel: m[5]!.trim() };
}

async function getState(client: ControlClient): Promise<{ state: State; dump: string }> {
  const res = await client.request(ControlCommand.TestKeyboardLinkFollow, []);
  const dump = String(res[1] ?? "");
  return { state: parseState(dump), dump };
}

async function waitForState(
  client: ControlClient,
  pred: (s: State) => boolean,
  timeoutMs = 4000,
): Promise<{ state: State; dump: string }> {
  const deadline = Date.now() + timeoutMs;
  let last = { state: { active: false, canFollow: false, page: 0, count: 0, accel: "" }, dump: "" };
  while (Date.now() < deadline) {
    last = await getState(client);
    if (pred(last.state)) {
      return last;
    }
    await sleep(25);
  }
  throw new Error(`keyboard-link state did not match in time\n${last.dump}`);
}

async function setupPdfView(client: ControlClient, proc: Bun.Subprocess): Promise<number> {
  const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
  if (!frame) {
    throw new Error("no frame window");
  }
  await client.waitForRenderIdle();
  sendCommandSync(frame, cmdId("CmdZoomActualSize"));
  sendCommandSync(frame, cmdId("CmdGoToFirstPage"));
  await client.waitForRenderIdle();
  return frame;
}

async function testLinkedPdf(): Promise<void> {
  const pdf = tmpPath("issue-2629-links.pdf");
  writeFileSync(pdf, makeLinkedPdf());

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      // 100% zoom, so the 2000pt-tall page doesn't fit the window and only
      // the links near its top are on screen. Zooming keeps the view
      // centered, so go back to the top of page 1.
      const frame = await setupPdfView(client, proc);

      const fail = (msg: string, dump: string) => {
        throw new Error(`${msg}\nstate dump:\n${dump}`);
      };

      let { state, dump } = await getState(client);
      if (!state.canFollow) {
        fail("keyboard link following should be available for a PDF", dump);
      }
      if (state.active) {
        fail("mode should start off", dump);
      }
      // the shortcut itself can't be pressed from here (posted key messages
      // carry no modifier state), so check what it is bound to
      if (state.accel !== "Shift + F") {
        fail(`expected the Shift + F shortcut, got '${state.accel}'`, dump);
      }

      // turn the mode on
      sendCommandSync(frame, cmdId("CmdToggleKeyboardLinkFollowing"));
      ({ state, dump } = await waitForState(client, (s) => s.active));
      // 3 links are near the top of page 1; the 4th is far below the fold
      if (state.count !== 3) {
        fail(`expected 3 numbered links at the top of page 1, got ${state.count}`, dump);
      }

      // the command toggles: a second one leaves the mode
      sendCommandSync(frame, cmdId("CmdToggleKeyboardLinkFollowing"));
      ({ state, dump } = await waitForState(client, (s) => !s.active && s.count === 0));

      // plain 'f' still belongs to fullscreen: it must not turn the mode on
      // (pressed twice so the window doesn't stay fullscreen)
      pressVKey(frame, VK_F);
      ({ state, dump } = await getState(client));
      const wrongKey = state.active;
      pressVKey(frame, VK_F);
      if (wrongKey) {
        fail("plain 'f' must toggle fullscreen, not keyboard link following", dump);
      }

      // Esc leaves the mode
      sendCommandSync(frame, cmdId("CmdToggleKeyboardLinkFollowing"));
      ({ state, dump } = await waitForState(client, (s) => s.active));
      pressVKey(frame, VK_ESCAPE);
      ({ state, dump } = await waitForState(client, (s) => !s.active));

      // back on, then follow a link. Links are numbered in reading order,
      // so #2 is the second one from the top, which points at page 3
      sendCommandSync(frame, cmdId("CmdToggleKeyboardLinkFollowing"));
      ({ state, dump } = await waitForState(client, (s) => s.active && s.count === 3));
      postMessage(frame, WM_CHAR, 0x32, 0);
      ({ state, dump } = await waitForState(client, (s) => s.page === 3 && !s.active));
    },
    [pdf],
  );
}

// the numbering must be recalculated after scrolling brings other links into
// view (debounced by 300ms, so wait longer than that)
async function testRecomputeAfterScroll(): Promise<void> {
  const pdf = tmpPath("issue-2629-links.pdf");
  writeFileSync(pdf, makeLinkedPdf());

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await setupPdfView(client, proc);
      sendCommandSync(frame, cmdId("CmdToggleKeyboardLinkFollowing"));
      let { state, dump } = await waitForState(client, (s) => s.active && s.count === 3);
      const atTop = state.count;

      // scroll to the bottom of page 1, where only the 4th link is visible.
      // numbering is recalculated 300ms after scrolling stops
      sendCommandSync(frame, cmdId("CmdScrollDownPage"));
      sendCommandSync(frame, cmdId("CmdScrollDownPage"));
      await sleep(350);
      ({ state, dump } = await waitForState(client, (s) => s.active && s.count !== atTop));
      if (!state.active) {
        throw new Error(`scrolling must not leave the mode\n${dump}`);
      }
      if (state.count === atTop) {
        throw new Error(`link numbering was not recalculated after scrolling (still ${state.count})\n${dump}`);
      }
    },
    [pdf],
  );
}

// images can't have links: the command must report itself unavailable and
// toggling it must do nothing
async function testImageUnavailable(): Promise<void> {
  const png = tmpPath("issue-2629.png");
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
      if (state.canFollow) {
        throw new Error(`keyboard link following must not be available for an image\n${dump}`);
      }
      sendCommandSync(frame, cmdId("CmdToggleKeyboardLinkFollowing"));
      ({ state, dump } = await getState(client));
      if (state.active) {
        throw new Error(`the mode must not turn on for an image\n${dump}`);
      }
    },
    [png],
  );
}

export async function testit(): Promise<void> {
  await testLinkedPdf();
  await testRecomputeAfterScroll();
  await testImageUnavailable();
}

if (import.meta.main) {
  await runStandalone(testit);
}
