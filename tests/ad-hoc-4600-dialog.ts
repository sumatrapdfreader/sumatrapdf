// Ad-hoc: the eBook Settings dialog (CmdChangeEbookSettings, #4600).
//
// Captures the dialog in both states (generated CSS read-only vs custom CSS
// editable), then drives it for real: tick "Ignore the document's own styling",
// press OK, and check what landed in the settings file -- once for "This file"
// (a FileStates -> EBookUI block holding only what differs from the global
// section) and once for "For all ebooks" (the global section, per-file block
// dropped).

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, EXE, tmpPath } from "./util.ts";
import { makeFixtures } from "./ad-hoc-issue-4600.ts";
import { captureWindowToPng, killAndWait, killProcessesNamed, waitForFrame } from "./win-automation.ts";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getControlText,
  getFocusedHwnd,
  getWindowLong,
  getWindowPid,
  getWindowText,
  moveWindow,
  postMessage,
  sendMessage,
  setForegroundWindow,
  showWindow,
  sleep,
  waitForTopWindow,
  SW_RESTORE,
  VK_ESCAPE,
  VK_RETURN,
  WM_COMMAND,
  WM_KEYDOWN,
  WM_KEYUP,
} from "./winapi.ts";

const OUT = tmpPath("epub-font");
const APPDATA = tmpPath("issue-4600-dialog-appdata");
const DLG_CLASS = "SumatraWgDefaultWinClass";
const BM_CLICK = 0x00f5;
const BM_GETCHECK = 0x00f0;
const CmdChangeEbookSettings = cmdId("CmdChangeEbookSettings");
const GWL_STYLE_IDX = -16;
const ES_READONLY = 0x0800;
const WS_DISABLED = 0x08000000;

// the multi-line Edit holding the CSS is the only Edit child that is multi-line
function cssEdit(dlg: number): number {
  const ES_MULTILINE = 0x0004;
  let found = 0;
  enumChildWindows(dlg, (hwnd) => {
    if (getClassName(hwnd) === "Edit" && (getWindowLong(hwnd, GWL_STYLE_IDX) & ES_MULTILINE) !== 0) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

// the dialog's Button children (checkboxes, radios), by their label
function buttons(dlg: number): Map<string, number> {
  const res = new Map<string, number>();
  enumChildWindows(dlg, (hwnd) => {
    if (getClassName(hwnd) === "Button") {
      res.set(getWindowText(hwnd).replace(/&/g, ""), hwnd);
    }
    return true;
  });
  return res;
}

function findByPrefix(map: Map<string, number>, prefix: string): number {
  for (const [k, v] of map) {
    if (k.startsWith(prefix)) {
      return v;
    }
  }
  throw new Error(`no button starting with "${prefix}"; have: ${[...map.keys()].join(" | ")}`);
}

// OK and Cancel are VirtButtons drawn inside the window, so there is no HWND
// to click: Enter activates the default one (WindowBase::ActivateOnEnter).
// It has to be posted -- the handler runs in PreTranslateMessage, off the queue
function pressOk(dlg: number): void {
  pressKey(dlg, VK_RETURN);
}

const VK_TAB = 0x09;

// posted, not sent: the handler runs in PreTranslateMessage, off the queue
function pressKey(dlg: number, vk: number): void {
  postMessage(dlg, WM_KEYDOWN, vk, 0);
  postMessage(dlg, WM_KEYUP, vk, 0);
}

// the ComboBox's inner Edit, which is what the font name is typed into
function fontEdit(dlg: number): number {
  let found = 0;
  enumChildWindows(dlg, (hwnd) => {
    if (getClassName(hwnd) === "ComboBox") {
      enumChildWindows(hwnd, (child) => {
        if (getClassName(child) === "Edit") {
          found = child;
          return false;
        }
        return true;
      });
      return false;
    }
    return true;
  });
  return found;
}

function writeSettings(extra: string[]): void {
  rmSync(APPDATA, { recursive: true, force: true });
  mkdirSync(APPDATA, { recursive: true });
  writeFileSync(
    join(APPDATA, "SumatraPDF-settings.txt"),
    [`EBookUI [`, `\tFontName = Georgia`, `\tLineSpacing = 1.4`, `]`, `RestoreSession = false`, ...extra, ``].join(
      "\n",
    ),
  );
}

type Launched = { proc: Bun.Subprocess; frame: number };

async function launch(epub: string): Promise<Launched> {
  // no -for-testing: the point is what gets written to the settings file
  const proc = Bun.spawn([EXE, "-appdata", APPDATA, epub], { stdout: "ignore", stderr: "ignore" });
  const frame = await waitForFrame(proc.pid!);
  if (!frame) {
    throw new Error("SumatraPDF window didn't open");
  }
  showWindow(frame, SW_RESTORE);
  moveWindow(frame, 40, 40, 1000, 800);
  setForegroundWindow(frame);
  await sleep(2000);
  return { proc, frame };
}

// notifications are top-level windows of the same class (the fixture EPUB
// raises an "Errors in document" one), so match the dialog by its title
function findDialog(pid: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) !== pid || getClassName(hwnd) !== DLG_CLASS) {
      return true;
    }
    if (getWindowText(hwnd) === "eBook Settings") {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

async function openDialog(l: Launched): Promise<number> {
  sendMessage(l.frame, WM_COMMAND, CmdChangeEbookSettings, 0);
  for (let i = 0; i < 20; i++) {
    await sleep(250);
    const dlg = findDialog(l.proc.pid!);
    if (dlg) {
      return dlg;
    }
  }
  throw new Error("the eBook Settings dialog did not open");
}

export async function testit(): Promise<void> {
  makeFixtures();
  const epub = join(OUT, "none.epub");
  await killProcessesNamed("SumatraPDF.exe");

  // --- both states of the CSS box ---------------------------------------
  writeSettings([]);
  {
    const l = await launch(epub);
    try {
      const dlg = await openDialog(l);
      captureWindowToPng(dlg, join(OUT, "dialog-generated.png"));
      const b = buttons(dlg);
      const custom = findByPrefix(b, "Custom CSS");
      const css = cssEdit(dlg);
      if (!css) {
        throw new Error("no multi-line CSS edit in the dialog");
      }
      if ((getWindowLong(css, GWL_STYLE_IDX) & ES_READONLY) === 0) {
        throw new Error("the generated CSS should be read-only");
      }
      sendMessage(custom, BM_CLICK, 0, 0);
      await sleep(400);
      captureWindowToPng(dlg, join(OUT, "dialog-custom.png"));
      console.log("  wrote dialog-generated.png and dialog-custom.png");
      if (sendMessage(custom, BM_GETCHECK, 0, 0) !== 1n) {
        throw new Error("the Custom CSS checkbox didn't turn on");
      }
      if ((getWindowLong(css, GWL_STYLE_IDX) & ES_READONLY) !== 0) {
        throw new Error("Custom CSS didn't make the edit editable");
      }
      // the two controls whose values are now baked into the text are disabled
      const fontCombo = (() => {
        let f = 0;
        enumChildWindows(dlg, (hwnd) => {
          if (getClassName(hwnd) === "ComboBox") {
            f = hwnd;
            return false;
          }
          return true;
        });
        return f;
      })();
      if (!fontCombo || (getWindowLong(fontCombo, GWL_STYLE_IDX) & WS_DISABLED) === 0) {
        throw new Error("the font dropdown should be disabled while Custom CSS is on");
      }
      sendMessage(custom, BM_CLICK, 0, 0);
      await sleep(400);
      if ((getWindowLong(css, GWL_STYLE_IDX) & ES_READONLY) === 0) {
        throw new Error("turning Custom CSS back off didn't restore the read-only preview");
      }
      console.log("  CSS box: read-only preview <-> editable custom CSS ✓");
    } finally {
      await killAndWait(l.proc);
    }
  }

  // --- keyboard: Tab ring, Esc, and the Reset button --------------------
  // this document asks for a different font than the global section, so Reset
  // (which drops back to what would be inherited) is visible in the combo
  writeSettings([
    `FileStates [`,
    `\t[`,
    `\t\tFilePath = ${epub}`,
    `\t\tEBookUI [`,
    `\t\t\tFontName = Courier New`,
    `\t\t]`,
    `\t]`,
    `]`,
  ]);
  {
    const l = await launch(epub);
    try {
      const dlg = await openDialog(l);
      setForegroundWindow(dlg);
      await sleep(400);

      // Tab visits every control, then the drawn buttons (which hold the focus
      // on the window itself), then wraps back to the first control
      const seen: number[] = [];
      const first = getFocusedHwnd(dlg);
      let wrapped = false;
      for (let i = 0; i < 20; i++) {
        const f = getFocusedHwnd(dlg);
        if (f && !seen.includes(f)) {
          seen.push(f);
        }
        pressKey(dlg, VK_TAB);
        await sleep(200);
        if (getFocusedHwnd(dlg) === first) {
          wrapped = true;
          break;
        }
      }
      // every control plus the window itself, which holds the focus for the
      // drawn Reset / Cancel / OK buttons
      if (seen.length < 8) {
        throw new Error(`Tab only reached ${seen.length} stops: ${seen.join(",")}`);
      }
      if (!wrapped) {
        throw new Error("Tab didn't wrap back around to the first control");
      }
      console.log(`  Tab: ${seen.length} distinct stops and back to the start ✓`);

      // Esc must not throw away what's being typed
      pressKey(dlg, VK_ESCAPE);
      await sleep(600);
      if (!(await waitForTopWindow(l.proc.pid!, DLG_CLASS, 1000))) {
        throw new Error("Esc closed the dialog");
      }
      console.log("  Esc leaves the dialog open ✓");

      // Reset: tab to it (the first drawn button after the last control) and
      // press Enter, which activates the focused button
      const combo = fontEdit(dlg);
      if (getControlText(combo) !== "Courier New") {
        throw new Error(`expected the per-document font, got "${getControlText(combo)}"`);
      }
      for (let i = 0; i < 12 && getFocusedHwnd(dlg) !== dlg; i++) {
        pressKey(dlg, VK_TAB);
        await sleep(150);
      }
      if (getFocusedHwnd(dlg) !== dlg) {
        throw new Error("never reached the drawn buttons");
      }
      pressKey(dlg, VK_RETURN);
      await sleep(600);
      if (getControlText(combo) !== "Georgia") {
        throw new Error(`Reset to defaults left the font at "${getControlText(combo)}"`);
      }
      console.log("  Reset to defaults: back to the inherited font ✓");
    } finally {
      await killAndWait(l.proc);
    }
  }

  // --- OK with "This file" writes a per-document block -------------------
  writeSettings([]);
  {
    const l = await launch(epub);
    try {
      const dlg = await openDialog(l);
      const b = buttons(dlg);
      sendMessage(findByPrefix(b, "Ignore the document"), BM_CLICK, 0, 0);
      await sleep(300);
      pressOk(dlg);
      await sleep(2500);
    } finally {
      await killAndWait(l.proc);
    }
    const saved = readFileSync(join(APPDATA, "SumatraPDF-settings.txt"), "utf8");
    const entry = saved.split("FilePath").find((s) => s.includes("none.epub")) ?? "";
    if (!entry.includes("IgnoreDocumentCSS = true")) {
      throw new Error("OK didn't write the per-document EBookUI block");
    }
    // only what differs from the global section is stored
    if (entry.includes("FontName = Georgia") || entry.includes("LineSpacing = 1.4")) {
      throw new Error("the per-document block copied values that match the global ones");
    }
    console.log("  This file: wrote IgnoreDocumentCSS = true and nothing that matches the global section ✓");
  }

  // --- OK with "For all ebooks" writes the global section ----------------
  {
    const l = await launch(epub);
    try {
      const dlg = await openDialog(l);
      const b = buttons(dlg);
      sendMessage(findByPrefix(b, "For all ebooks"), BM_CLICK, 0, 0);
      await sleep(400);
      sendMessage(findByPrefix(b, "Ignore the document"), BM_CLICK, 0, 0);
      await sleep(300);
      pressOk(dlg);
      await sleep(2500);
    } finally {
      await killAndWait(l.proc);
    }
    const saved = readFileSync(join(APPDATA, "SumatraPDF-settings.txt"), "utf8");
    const global = saved.slice(saved.indexOf("EBookUI ["), saved.indexOf("FileStates"));
    if (!global.includes("IgnoreDocumentCSS = true")) {
      throw new Error("OK didn't write the global EBookUI section");
    }
    const entry = saved.split("FilePath").find((s) => s.includes("none.epub")) ?? "";
    if (entry.includes("EBookUI [")) {
      throw new Error("applying to all ebooks left the per-document block behind");
    }
    console.log("  For all ebooks: wrote the global section and dropped the per-document block ✓");
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util.ts");
  await runStandalone(testit);
}
