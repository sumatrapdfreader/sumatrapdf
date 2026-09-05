// #2629: Vimium-style keyboard link following. Shift + F
// (CmdToggleKeyboardLinkFollowing) labels the links visible on screen with
// letters and pressing a complete hint follows the link. The labels are
// recalculated shortly after scrolling stops, and the whole feature is
// unavailable for formats whose pages can't have links (comic books, image
// folders, images).
//
// Hint letters and Esc go through TestKeyboardLinkFollow actions; posted keys miss.
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import { FRAME_CLASS, sendCommandSync } from "./win-automation";
import { WM_KEYDOWN, WM_KEYUP, postMessage, sleep, waitForTopWindow } from "./winapi";

const VK_F = 0x46;

// An unmodified key press. Modified shortcuts (Shift + F) can't be driven this
// way: TranslateAccelerator asks GetKeyState() for the modifiers and posted key
// messages don't set it, so a posted Shift + F arrives as a plain F. The
// Shift + F binding is checked through the accel= field of the state dump.
// lParam matters: a key-up whose transition / previous-state bits (31, 30) are
// clear looks like a press to TranslateMessage, which then produces an extra
// WM_CHAR. That stray 'f' typed itself into the link hints when it landed after
// the mode was turned on again - it matches the "F" hint exactly, so the app
// followed that link and left the mode, and the test timed out waiting for it.
function pressVKey(hwnd: number, vk: number): void {
  postMessage(hwnd, WM_KEYDOWN, vk, 0x00000001);
  postMessage(hwnd, WM_KEYUP, vk, 0xc0000001);
}

// 3 pages. Page 1 carries 20 link annotations in a stack (enough to require
// multi-letter hints); pages 2 and 3 have none.
function makeLinkedPdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R >>");
  body[2] = enc("<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R] /Count 3 >>");

  // Link rects are in PDF space (origin bottom-left); page is 612x2000.
  const links: { rect: string; dest: string }[] = [];
  for (let i = 0; i < 20; i++) {
    const y2 = 1950 - i * 70;
    const pageObj = i % 2 === 0 ? 4 : 5;
    links.push({ rect: `[72 ${y2 - 30} 300 ${y2}]`, dest: `/Dest [${pageObj} 0 R /Fit]` });
  }
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

  const maxN = objNo - 1;
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

type State = { active: boolean; canFollow: boolean; page: number; count: number; accel: string; hints: string[] };

function parseState(dump: string): State {
  const m = /active=(\d+) canFollow=(\d+) page=(-?\d+) count=(\d+) accel=(.*)/.exec(dump);
  if (!m) {
    throw new Error(`unexpected TestKeyboardLinkFollow output:\n${dump}`);
  }
  const hints = [...dump.matchAll(/ hint=([A-Z]+)$/gm)].map((match) => match[1]!);
  return {
    active: m[1] === "1",
    canFollow: m[2] === "1",
    page: +m[3]!,
    count: +m[4]!,
    accel: m[5]!.trim(),
    hints,
  };
}

async function getState(
  client: ControlClient,
  args: (string | number)[] = [],
): Promise<{ state: State; dump: string }> {
  const res = await client.request(ControlCommand.TestKeyboardLinkFollow, args);
  const dump = String(res[1] ?? "");
  return { state: parseState(dump), dump };
}

async function waitForState(
  client: ControlClient,
  pred: (s: State) => boolean,
  timeoutMs = 4000 * SLOW_BUILD_FACTOR,
): Promise<{ state: State; dump: string }> {
  const deadline = Date.now() + timeoutMs;
  let last = { state: { active: false, canFollow: false, page: 0, count: 0, accel: "", hints: [] }, dump: "" };
  while (Date.now() < deadline) {
    last = await getState(client);
    if (pred(last.state)) {
      return last;
    }
    await sleep(25);
  }
  throw new Error(`keyboard-link state did not match in time\n${last.dump}`);
}

async function startLinkFollowing(client: ControlClient, frame: number): Promise<{ state: State; dump: string }> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let last = await getState(client);
  while (Date.now() < deadline) {
    if (!last.state.active && last.state.page === 1) {
      // Link enumeration uses a nonblocking engine lock and can briefly decline
      // activation while an ASan render thread still owns the page.
      sendCommandSync(frame, cmdId("CmdToggleKeyboardLinkFollowing"));
    }
    last = await getState(client);
    if (last.state.active && last.state.page === 1 && last.state.count === 20) {
      return last;
    }
    await sleep(25);
  }
  throw new Error(`keyboard-link mode did not start in time\n${last.dump}`);
}

async function drainPostedKeys(client: ControlClient): Promise<void> {
  // first request sees the key; second eats a trailing WM_CHAR
  await getState(client);
  await getState(client);
}

async function exitFullscreenIfOn(client: ControlClient, frame: number): Promise<void> {
  const res = await client.request(ControlCommand.TestDisplayMode, ["get"]);
  if (!/fullscreen=1/.test(String(res[1] ?? ""))) {
    return;
  }
  sendCommandSync(frame, cmdId("CmdToggleFullscreen"));
}

async function setupPdfView(client: ControlClient, proc: Bun.Subprocess): Promise<number> {
  const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
  if (!frame) {
    throw new Error("no frame window");
  }
  await client.waitForRenderIdle();
  sendCommandSync(frame, cmdId("CmdZoomFitPage"));
  await client.waitForRenderIdle();
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
      // Fit the whole first page so all 20 link hints are visible.
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
      ({ state, dump } = await startLinkFollowing(client, frame));
      // All 20 links on page 1 are visible.
      if (state.count !== 20) {
        fail(`expected 20 labeled links at the top of page 1, got ${state.count}`, dump);
      }
      const expectedHints = "A SA C D SD E F SF G H J SJ K SK L SL M P SS W".split(" ");
      if (state.hints.join(" ") !== expectedHints.join(" ")) {
        fail(`unexpected Vimium-style hints: ${state.hints.join(" ")}`, dump);
      }

      // the command toggles: a second one leaves the mode
      sendCommandSync(frame, cmdId("CmdToggleKeyboardLinkFollowing"));
      ({ state, dump } = await waitForState(client, (s) => !s.active && s.count === 0));

      // plain F must not start link following. Posted F often misses the
      // fullscreen accelerator; leave fullscreen if it did fire.
      pressVKey(frame, VK_F);
      await drainPostedKeys(client);
      ({ state, dump } = await getState(client));
      if (state.active) {
        fail("plain 'f' must not turn on keyboard link following", dump);
      }
      await exitFullscreenIfOn(client, frame);

      ({ state, dump } = await startLinkFollowing(client, frame));
      ({ state, dump } = await getState(client, ["stop"]));
      if (state.active) {
        fail("stop should leave the mode", dump);
      }

      // B is not in Vimium's ergonomic hint alphabet. It cancels the mode.
      ({ state, dump } = await startLinkFollowing(client, frame));
      ({ state, dump } = await getState(client, ["char", "b"]));
      if (state.active || state.page !== 1) {
        fail("B should cancel the mode and stay on page 1", dump);
      }

      // A is a complete single-letter hint for the first link.
      ({ state, dump } = await startLinkFollowing(client, frame));
      ({ state, dump } = await getState(client, ["char", "a"]));
      if (state.page === 1 || state.active) {
        fail("A should follow the first link off page 1", dump);
      }
      const singleHintPage = state.page;

      // Uppercase A is the same hint (issue #6019: must follow, not highlight).
      sendCommandSync(frame, cmdId("CmdGoToFirstPage"));
      await client.waitForRenderIdle();
      ({ state, dump } = await startLinkFollowing(client, frame));
      ({ state, dump } = await getState(client, ["char", "A"]));
      if (state.page !== singleHintPage || state.active) {
        fail(`'A' should follow the same hint as 'a' (page ${singleHintPage}), went to ${state.page}`, dump);
      }

      // SA is a two-letter hint for the second link. S alone must keep the mode
      // active instead of following a different link.
      sendCommandSync(frame, cmdId("CmdGoToFirstPage"));
      await client.waitForRenderIdle();
      ({ state, dump } = await startLinkFollowing(client, frame));
      ({ state, dump } = await getState(client, ["char", "s"]));
      if (!state.active || state.page !== 1) {
        fail("S alone should keep the mode on page 1", dump);
      }
      ({ state, dump } = await getState(client, ["char", "a"]));
      if (state.active || state.page === 1 || state.page === singleHintPage) {
        fail(`A and SA should follow different adjacent links, but SA went to page ${state.page}`, dump);
      }
    },
    [pdf],
  );
}

// The labels must be recalculated after navigating to a page with no links
// (debounced by 300ms, so wait longer than that).
async function testRecomputeAfterNavigation(): Promise<void> {
  const pdf = tmpPath("issue-2629-links.pdf");
  writeFileSync(pdf, makeLinkedPdf());

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await setupPdfView(client, proc);
      let { state, dump } = await startLinkFollowing(client, frame);
      const atTop = state.count;

      // Page 2 has no links. Labels are recalculated 300ms after navigation.
      sendCommandSync(frame, cmdId("CmdGoToNextPage"));
      await sleep(350);
      ({ state, dump } = await waitForState(client, (s) => s.active && s.count !== atTop));
      if (!state.active) {
        throw new Error(`page navigation must not leave the mode\n${dump}`);
      }
      if (state.count === atTop) {
        throw new Error(`link labels were not recalculated after page navigation (still ${state.count})\n${dump}`);
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
  await testRecomputeAfterNavigation();
  await testImageUnavailable();
}

if (import.meta.main) {
  await runStandalone(testit);
}
