// Ad-hoc test for the Sign Document dialog (issue #5962).
//
// Signs a PDF with a throw-away self-signed certificate and checks the
// signature. Ad-hoc (not in run-almost-all) because it drives the dialog and
// the modal "Save As" that follows it.
//
// Run:  bun tests/ad-hoc-sign-document.ts [--no-build]

import { copyFileSync, existsSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { pdfSigFieldObjects, verifyPdfSignature, writeTestPfx, type TestCert } from "./pki.ts";
import { ROOT, cmdId, runStandalone, tmpPath, writePdfWithEmptySigField } from "./util.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, pressEnter, sendCommand } from "./win-automation.ts";
import {
  captureWindowToPng,
  enumChildWindows,
  enumWindows,
  getClassName,
  getClientRect,
  getControlText,
  getWindowPid,
  getWindowText,
  postMessage,
  sendText,
  sleep,
  WM_CLOSE,
} from "./winapi.ts";

const kPassword = "sumatra-test-pw";
const kCertName = "SumatraPDF SignTest";
const kDlgTitle = "Sign Document";
const kSaveTimeoutMs = 20000;

// How the test reaches the dialog: win32 windows on Windows, the control
// channel's view of the gpui ones on macOS.
type SignUi = {
  // opens the dialog, fills it in and presses Sign; returns the placement offered
  sign(pfx: string, password: string): Promise<string>;
  // closes the error box; false if there was none
  dismissError(): Promise<boolean>;
  isOpen(): Promise<boolean>;
  close(): Promise<void>;
  // a new signature is placed by clicking the page (issue #5967)
  clickPage(): Promise<void>;
  // accepts "Save As" with the name it suggests
  acceptSave(): Promise<void>;
};

async function waitFor<T>(what: string, fn: () => T | Promise<T>, timeoutMs = 12000): Promise<NonNullable<T>> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const v = await fn();
    if (v) {
      return v;
    }
    if (Date.now() > deadline) {
      throw new Error(`timed out waiting for ${what}`);
    }
    await sleep(200);
  }
}

// --- Windows ---------------------------------------------------------------

function findTopWindow(pid: number, className: string, title?: string): number {
  let found = 0;
  enumWindows((h) => {
    if (getWindowPid(h) !== pid || getClassName(h) !== className) {
      return true;
    }
    if (title !== undefined && getWindowText(h) !== title) {
      return true;
    }
    found = h;
    return false;
  });
  return found;
}

function childrenOfClass(parent: number, className: string): number[] {
  const res: number[] = [];
  enumChildWindows(parent, (h) => {
    if (getClassName(h) === className) {
      res.push(h);
    }
    return true;
  });
  return res;
}

function winSignUi(pid: number, frame: number): SignUi {
  const findDlg = () => findTopWindow(pid, "SumatraWgDefaultWinClass", kDlgTitle);
  return {
    async sign(pfx, password) {
      sendCommand(frame, cmdId("CmdSignDocument"));
      const dlg = await waitFor("Sign Document dialog", findDlg);
      await sleep(600);
      const edits = childrenOfClass(dlg, "Edit");
      if (edits.length < 4) {
        throw new Error(`Sign dialog: expected 4 edit fields, got ${edits.length}`);
      }
      const combos = childrenOfClass(dlg, "ComboBox");
      const placement =
        combos.map(getControlText).find((t) => /signature|CEO|page/i.test(t)) ??
        (combos.length ? getControlText(combos[combos.length - 1]!) : "");
      sendText(edits[0]!, pfx);
      sendText(edits[1]!, password);
      await sleep(300);
      // Enter activates the default button
      await pressEnter(dlg);
      await sleep(2500);
      return placement;
    },
    async dismissError() {
      const msgBox = findTopWindow(pid, "#32770");
      if (!msgBox) {
        return false;
      }
      captureWindowToPng(msgBox, tmpPath("sign-document-badpwd.png"));
      postMessage(msgBox, WM_CLOSE, 0, 0);
      await sleep(800);
      return true;
    },
    async isOpen() {
      return findDlg() !== 0;
    },
    async close() {
      postMessage(findDlg(), WM_CLOSE, 0, 0);
      await sleep(800);
    },
    async clickPage() {
      const canvas = findCanvas(frame);
      if (!canvas) {
        throw new Error("could not find the canvas to place the signature");
      }
      const cr = getClientRect(canvas);
      await clickAt(canvas, Math.floor(cr.right / 2), Math.floor(cr.bottom / 2), 400);
    },
    async acceptSave() {
      const save = await waitFor("Save As dialog", () => findTopWindow(pid, "#32770", "Save As"));
      const edits = childrenOfClass(save, "Edit");
      await pressEnter(edits.length ? edits[0]! : save);
    },
  };
}

// --- macOS -----------------------------------------------------------------

// gpui roles in a TestToolWindow layout dump
const kRoleEdit = 17;
const kRoleButton = 18;
const kRolePassword = 40;
const kToolWin = "signdocument";

type UiNode = { x: number; y: number; role: number; label: string };

// nodes of a layout dump, with x,y their middle
function parseNodes(raw: string): UiNode[] {
  const re = /node rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) role=(\d+) label='(.*?)' value=/g;
  return [...raw.matchAll(re)].map((m) => ({
    x: Math.round(Number(m[1]) + Number(m[3]) / 2),
    y: Math.round(Number(m[2]) + Number(m[4]) / 2),
    role: Number(m[5]),
    label: m[6]!,
  }));
}

function macSignUi(client: ControlClient, frame: number): SignUi {
  const toolWin = async (...args: (string | number)[]) =>
    String((await client.request(ControlCommand.TestToolWindow, args))[1] ?? "");
  const dlgNodes = async () => parseNodes(await toolWin("layout", kToolWin));
  const frameNodes = async () => parseNodes(await toolWin("layout", "frame"));
  const button = (nodes: UiNode[], label: string) => nodes.find((n) => n.role === kRoleButton && n.label === label);
  const clickDlg = (n: UiNode) => toolWin("input", kToolWin, "click", n.x, n.y, 0, 0);
  const clickFrame = (n: UiNode) => client.request(ControlCommand.TestInput, ["click", n.x, n.y, 0, 0]);

  async function typeInto(n: UiNode, text: string): Promise<void> {
    await clickDlg(n);
    for (const ch of text) {
      await toolWin("input", kToolWin, "char", ch.codePointAt(0)!, 0, 0, 0);
    }
  }

  return {
    async sign(pfx, password) {
      sendCommand(frame, cmdId("CmdSignDocument"));
      const nodes = await waitFor("Sign Document dialog", async () => {
        const ns = await dlgNodes();
        return button(ns, "Sign") ? ns : null;
      });
      const placement = (await toolWin("sign-placement")).trim();
      await typeInto(
        nodes.find((n) => n.role === kRoleEdit)!,
        pfx,
      );
      await typeInto(
        nodes.find((n) => n.role === kRolePassword)!,
        password,
      );
      await clickDlg(button(nodes, "Sign")!);
      await sleep(500);
      return placement;
    },
    async dismissError() {
      const ok = button(await dlgNodes(), "OK");
      if (!ok) {
        return false;
      }
      await clickDlg(ok);
      await sleep(300);
      return true;
    },
    async isOpen() {
      return button(await dlgNodes(), "Sign") !== undefined;
    },
    async close() {
      await toolWin("close", kToolWin);
      await sleep(300);
    },
    async clickPage() {
      const layout = (await client.layout()).raw;
      const scale = Number(/scale=([0-9.]+)/.exec(layout)?.[1] ?? "1") || 1;
      const page = /page n=1 shown=1 .*screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(layout);
      if (!page) {
        throw new Error("page 1 is not on screen");
      }
      // TestInput takes dips; the page rect is in canvas pixels
      const x = Math.round(scale * (Number(page[1]) + Number(page[3]) / 2));
      const y = Math.round(scale * (Number(page[2]) + Number(page[4]) / 2));
      await client.request(ControlCommand.TestInput, ["click", x, y, 0, 0]);
    },
    async acceptSave() {
      const save = await waitFor("Save As dialog", async () => button(await frameNodes(), "Save"));
      await clickFrame(save);
    },
  };
}

// --- the checks ------------------------------------------------------------

async function withSignUi(pdf: string, fn: (ui: SignUi) => Promise<void>): Promise<void> {
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await fn(IS_MAC ? macSignUi(client, frame) : winSignUi(proc.pid!, frame));
  } finally {
    client.close();
    postMessage(frame, WM_CLOSE, 0, 0);
    await killAndWait(proc);
  }
}

// The file shows up before the signature is in it.
async function waitForSigned(path: string, cert: TestCert): Promise<void> {
  await waitFor("signed file", () => existsSync(path), kSaveTimeoutMs);
  let lastErr: unknown;
  for (const deadline = Date.now() + kSaveTimeoutMs; Date.now() < deadline;) {
    try {
      verifyPdfSignature(path, cert);
      return;
    } catch (e) {
      lastErr = e;
    }
    await sleep(200);
  }
  throw new Error(`signed file did not verify: ${lastErr}`);
}

export async function testit(): Promise<void> {
  const pfx = tmpPath("sign-document.pfx");
  const cert = writeTestPfx(pfx, kCertName, kPassword);
  try {
    await signExistingField(pfx, cert);
    await signPlainPdf(pfx, cert);
  } finally {
    rmSync(pfx, { force: true });
  }
}

// An empty signature field gets filled in rather than a second one added.
async function signExistingField(pfx: string, cert: TestCert): Promise<void> {
  const fieldIn = tmpPath("sign-document-field.pdf");
  const fieldOut = tmpPath("sign-document-field Copy.pdf");
  writePdfWithEmptySigField(fieldIn);
  rmSync(fieldOut, { force: true });
  const fieldObjs = pdfSigFieldObjects(fieldIn);

  await withSignUi(fieldIn, async (ui) => {
    // a wrong password must not sign anything
    await ui.sign(pfx, "definitely-not-the-password");
    if (!(await ui.dismissError())) {
      throw new Error("wrong password: expected an error message box");
    }
    if (existsSync(fieldOut)) {
      throw new Error("wrong password: must not have written a file");
    }
    if (!(await ui.isOpen())) {
      throw new Error("wrong password: the dialog should stay open so the password can be fixed");
    }
    await ui.close();

    const placement = await ui.sign(pfx, kPassword);
    if (!placement.includes("CEO")) {
      throw new Error(`expected the document's empty 'CEO' field to be offered, got '${placement}'`);
    }
    await ui.acceptSave();
    await waitForSigned(fieldOut, cert);
  });

  const signedObjs = pdfSigFieldObjects(fieldOut);
  if (signedObjs.join() !== fieldObjs.join()) {
    throw new Error(`expected only field ${fieldObjs} to be signed, the file has ${signedObjs}`);
  }
}

// A PDF without a signature field gets a new one where the page is clicked.
async function signPlainPdf(pfx: string, cert: TestCert): Promise<void> {
  const plainIn = tmpPath("sign-document-plain.pdf");
  const plainOut = tmpPath("sign-document-plain Copy.pdf");
  copyFileSync(join(ROOT, "ext", "a-zlib", "zlib.3.pdf"), plainIn);
  rmSync(plainOut, { force: true });

  await withSignUi(plainIn, async (ui) => {
    const placement = await ui.sign(pfx, kPassword);
    if (!placement.includes("New signature")) {
      throw new Error(`expected a "new signature" placement, got '${placement}'`);
    }
    await ui.clickPage();
    await ui.acceptSave();
    await waitForSigned(plainOut, cert);
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
