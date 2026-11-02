// window.sumatraHost hooks of the wasm build (docs/wasm-host-api.md), in a real
// browser: headless Chrome or Edge driven over the DevTools protocol, with real
// key presses. Ad-hoc: it needs a wasm build (emsdk) and a browser.

import { existsSync, readFileSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { makeTextPdf, ROOT, tmpPath } from "./util.ts";

const OUT_DIR = join(ROOT, "out", "wasm", "rel");
const BROWSERS = [
  "C:/Program Files/Google/Chrome/Application/chrome.exe",
  "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe",
];
const STEP_TIMEOUT_MS = 30_000;
const PICKED_NAME = "picked-by-host.pdf";
const LOCAL_NAME = "picked-in-chooser.pdf";
const HOME_MARKER = "embedder home page";
const CTRL_MODIFIER = 2;
const ENTER_VK = 13;
const HELP_COMMAND = "help manual";
export const EXIT_COMMAND = "exit application";
const SAVE_AS_COMMAND = "save file as";
const NEW_WINDOW_COMMAND = "new window";
const SECOND_NAME = "opened-from-document.pdf";
const THIRD_NAME = "opened-from-home.pdf";
const ESCAPE_VK = 27;
// the tab strip's first cell
const MENU_BUTTON = { x: 14, y: 12 };
const HOME_TAB = { x: 52, y: 12 };
const SECOND_DOC_TAB = { x: 376, y: 12 };
const MENU_FILE_ITEM = { x: 40, y: 36 };
const HOME_OPEN_BUTTON = { x: 612, y: 63 };
// the toolbar's first button
const OPEN_BUTTON_UNDER_TABS = { x: 20, y: 42 };
const BELOW_TABS_Y = 30;
// beside the page: hovers nothing
const NEUTRAL_POINT = { x: 100, y: 300 };
const SETTLE_MS = 1500;
const PALETTE_INPUT = { x: 300, y: 64 };
const OTHER_TAB_NAME = "from-another-tab.pdf";
const DROPPED_NAME = "dropped-on-page.pdf";
const CLOSED_MARKER = "SumatraPDF was closed";

type Pending = { resolve: (v: any) => void; reject: (e: Error) => void };

export class Cdp {
  private id = 0;
  private pending = new Map<number, Pending>();
  events: { method: string; params: any }[] = [];
  private constructor(
    private ws: WebSocket,
    private sessionId = "",
  ) {
    ws.onmessage = (ev) => {
      const msg = JSON.parse(String(ev.data));
      if (msg.method) this.events.push(msg);
      const p = this.pending.get(msg.id);
      if (!p) return;
      this.pending.delete(msg.id);
      if (msg.error) p.reject(new Error(JSON.stringify(msg.error)));
      else p.resolve(msg.result);
    };
  }

  static async connect(url: string): Promise<Cdp> {
    const ws = new WebSocket(url);
    await new Promise((resolve, reject) => {
      ws.onopen = resolve;
      ws.onerror = reject;
    });
    return new Cdp(ws);
  }

  attach(sessionId: string): void {
    this.sessionId = sessionId;
  }

  send(method: string, params: object = {}): Promise<any> {
    const id = ++this.id;
    const msg: any = { id, method, params };
    if (this.sessionId) msg.sessionId = this.sessionId;
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
      this.ws.send(JSON.stringify(msg));
    });
  }

  async eval(expr: string): Promise<any> {
    const r = await this.send("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true });
    if (r.exceptionDetails) throw new Error(`${expr}: ${JSON.stringify(r.exceptionDetails)}`);
    return r.result.value;
  }

  close(): void {
    this.ws.close();
  }
}

export async function waitFor(cdp: Cdp, expr: string, what: string): Promise<any> {
  const deadline = Date.now() + STEP_TIMEOUT_MS;
  for (;;) {
    const v = await cdp.eval(expr).catch(() => null);
    if (v) return v;
    if (Date.now() > deadline) throw new Error(`ad-hoc-wasm-host: timed out waiting for ${what}`);
    await Bun.sleep(100);
  }
}

async function pressKey(cdp: Cdp, key: string, code: string, vk: number, modifiers = 0): Promise<void> {
  const base = { key, code, windowsVirtualKeyCode: vk, nativeVirtualKeyCode: vk, modifiers };
  await cdp.send("Input.dispatchKeyEvent", { type: "rawKeyDown", ...base });
  await cdp.send("Input.dispatchKeyEvent", { type: "keyUp", ...base });
}

export async function clickAt(cdp: Cdp, x: number, y: number): Promise<void> {
  await cdp.send("Input.dispatchMouseEvent", { type: "mouseMoved", x, y });
  await Bun.sleep(100);
  const press = { x, y, button: "left", clickCount: 1 };
  await cdp.send("Input.dispatchMouseEvent", { type: "mousePressed", ...press });
  await cdp.send("Input.dispatchMouseEvent", { type: "mouseReleased", ...press });
}

async function pressCtrl(cdp: Cdp, letter: string): Promise<void> {
  await pressKey(cdp, letter.toLowerCase(), `Key${letter}`, letter.charCodeAt(0), CTRL_MODIFIER);
}

// the browser keeps Ctrl+Q and F1, so go the way a user of the page does
async function typeInPalette(cdp: Cdp, title: string): Promise<void> {
  await pressCtrl(cdp, "K");
  await Bun.sleep(400);
  await clickAt(cdp, PALETTE_INPUT.x, PALETTE_INPUT.y);
  await Bun.sleep(200);
  for (const ch of title) {
    const upper = ch.toUpperCase();
    const key = {
      key: ch,
      text: ch,
      code: ch === " " ? "Space" : `Key${upper}`,
      windowsVirtualKeyCode: upper.charCodeAt(0),
    };
    await cdp.send("Input.dispatchKeyEvent", { type: "keyDown", ...key });
    await cdp.send("Input.dispatchKeyEvent", { type: "keyUp", ...key });
    await Bun.sleep(30);
  }
  await Bun.sleep(600);
}

export async function runFromPalette(cdp: Cdp, title: string): Promise<void> {
  await typeInPalette(cdp, title);
  await pressKey(cdp, "Enter", "Enter", ENTER_VK);
}

async function saveShot(cdp: Cdp, name: string): Promise<void> {
  const shot = await cdp.send("Page.captureScreenshot");
  writeFileSync(tmpPath(`ad-hoc-wasm-host-${name}.png`), Buffer.from(shot.data, "base64"));
}

// the page under the tab strip: toolbar and document, not the tab titles
async function belowTabs(cdp: Cdp): Promise<string> {
  await cdp.send("Input.dispatchMouseEvent", { type: "mouseMoved", ...NEUTRAL_POINT });
  await Bun.sleep(SETTLE_MS);
  const [width, height] = await cdp.eval(`[innerWidth, innerHeight]`);
  const clip = { x: 0, y: BELOW_TABS_Y, width, height: height - BELOW_TABS_Y, scale: 1 };
  return (await cdp.send("Page.captureScreenshot", { clip })).data;
}

async function pickCount(cdp: Cdp): Promise<number> {
  return cdp.eval(`window.__calls.filter((c) => c === "pickFile").length`);
}

// Home has the tab strip with the menu button instead of the toolbar; documents
// keep the toolbar, sized the same whether they were opened from Home or not
async function testHomeTab(cdp: Cdp): Promise<void> {
  await saveShot(cdp, "home");
  await clickAt(cdp, MENU_BUTTON.x, MENU_BUTTON.y);
  await Bun.sleep(400);
  await saveShot(cdp, "home-menu");
  await pressKey(cdp, "Escape", "Escape", ESCAPE_VK);
  await Bun.sleep(200);
  await clickAt(cdp, OPEN_BUTTON_UNDER_TABS.x, OPEN_BUTTON_UNDER_TABS.y);
  await Bun.sleep(400);
  if ((await pickCount(cdp)) !== 0) throw new Error("ad-hoc-wasm-host: Home has a toolbar");

  await clickAt(cdp, HOME_OPEN_BUTTON.x, HOME_OPEN_BUTTON.y);
  await waitFor(cdp, `window.__calls.includes("pickFile")`, "the pickFile hook");
  await waitFor(cdp, `Module.FS.analyzePath("/uploads/${PICKED_NAME}").exists`, "the picked file in /uploads");
  await Bun.sleep(SETTLE_MS);
  await saveShot(cdp, "document");

  await cdp.eval(`window.__pickName = "${SECOND_NAME}"`);
  await clickAt(cdp, OPEN_BUTTON_UNDER_TABS.x, OPEN_BUTTON_UNDER_TABS.y);
  await waitFor(cdp, `Module.FS.analyzePath("/uploads/${SECOND_NAME}").exists`, "a document's toolbar");
  await Bun.sleep(SETTLE_MS);
  const fromDocument = await belowTabs(cdp);

  await clickAt(cdp, HOME_TAB.x, HOME_TAB.y);
  await Bun.sleep(SETTLE_MS);
  await saveShot(cdp, "home-with-tabs");
  const picks = await pickCount(cdp);
  await clickAt(cdp, OPEN_BUTTON_UNDER_TABS.x, OPEN_BUTTON_UNDER_TABS.y);
  await Bun.sleep(400);
  if ((await pickCount(cdp)) !== picks) throw new Error("ad-hoc-wasm-host: Home has a toolbar next to documents");

  await cdp.eval(`window.__pickName = "${THIRD_NAME}"`);
  await clickAt(cdp, HOME_OPEN_BUTTON.x, HOME_OPEN_BUTTON.y);
  await waitFor(cdp, `Module.FS.analyzePath("/uploads/${THIRD_NAME}").exists`, "the document opened from Home");
  await Bun.sleep(SETTLE_MS);
  await saveShot(cdp, "opened-from-home");
  if ((await belowTabs(cdp)) !== fromDocument) {
    throw new Error("ad-hoc-wasm-host: a document opened from Home is laid out differently");
  }
  console.log("ad-hoc-wasm-host: ok: Home has tabs instead of the toolbar");
}

// records every call, and answers the way the page under test wants
// drops a tab below the strip, where it would move to a window of its own
async function dragTabOut(cdp: Cdp): Promise<void> {
  const { x, y } = SECOND_DOC_TAB;
  await cdp.send("Input.dispatchMouseEvent", { type: "mouseMoved", x, y });
  await Bun.sleep(100);
  const held = { button: "left", buttons: 1 };
  await cdp.send("Input.dispatchMouseEvent", { type: "mousePressed", x, y, clickCount: 1, ...held });
  for (const dy of [10, 40, 120, 250]) {
    await cdp.send("Input.dispatchMouseEvent", { type: "mouseMoved", x, y: y + dy, ...held });
    await Bun.sleep(100);
  }
  await cdp.send("Input.dispatchMouseEvent", { type: "mouseReleased", x, y: y + 250, clickCount: 1, ...held });
}

async function wholePage(cdp: Cdp): Promise<string> {
  await cdp.send("Input.dispatchMouseEvent", { type: "mouseMoved", ...NEUTRAL_POINT });
  await Bun.sleep(SETTLE_MS);
  return (await cdp.send("Page.captureScreenshot")).data;
}

// a browser tab has one canvas, so one window: nothing offers another, and
// what would make one leaves the tabs alone
async function testNoSecondWindow(cdp: Cdp): Promise<void> {
  cdp.events.length = 0;
  await typeInPalette(cdp, NEW_WINDOW_COMMAND);
  await saveShot(cdp, "palette-new-window");
  await pressKey(cdp, "Escape", "Escape", ESCAPE_VK);
  await Bun.sleep(200);
  await pressCtrl(cdp, "N");
  await Bun.sleep(SETTLE_MS);

  await clickAt(cdp, SECOND_DOC_TAB.x, SECOND_DOC_TAB.y);
  const beforeDrag = await wholePage(cdp);
  await dragTabOut(cdp);
  if ((await wholePage(cdp)) !== beforeDrag) {
    await saveShot(cdp, "tab-dragged-out");
    throw new Error("ad-hoc-wasm-host: dragging a tab out of the strip changed the tabs");
  }

  await clickAt(cdp, MENU_BUTTON.x, MENU_BUTTON.y);
  await Bun.sleep(400);
  await clickAt(cdp, MENU_FILE_ITEM.x, MENU_FILE_ITEM.y);
  await Bun.sleep(400);
  await saveShot(cdp, "file-menu");
  await pressKey(cdp, "Escape", "Escape", ESCAPE_VK);
  await pressKey(cdp, "Escape", "Escape", ESCAPE_VK);
  await Bun.sleep(200);

  const crash = cdp.events.find((e) => e.method === "Runtime.exceptionThrown");
  if (crash) {
    const { exception, text } = crash.params.exceptionDetails;
    throw new Error(`ad-hoc-wasm-host: asking for a new window crashed: ${exception?.description ?? text}`);
  }
  console.log("ad-hoc-wasm-host: ok: asking for a new window doesn't crash");
}

function opfsHas(dir: string, name: string): string {
  return `navigator.storage.getDirectory()
    .then((r) => r.getDirectoryHandle(${JSON.stringify(dir)}))
    .then((d) => d.getFileHandle(${JSON.stringify(name)}))
    .then(() => true, () => false)`;
}

// A second tab of the same origin stores a document, then this tab saves.
// The save must not delete what this tab never saw (it used to).
async function testOtherTabKeepsItsDocument(cdp: Cdp, wsUrl: string, pageUrl: string): Promise<void> {
  const other = await Cdp.connect(wsUrl);
  try {
    const { targetId } = await other.send("Target.createTarget", { url: "about:blank" });
    const { sessionId } = await other.send("Target.attachToTarget", { targetId, flatten: true });
    other.attach(sessionId);
    await other.send("Runtime.enable");
    await other.send("Page.navigate", { url: `${pageUrl}?nohost` });
    await waitFor(other, `window.__sumatraStorage === "ready" && !!Module._sumatra_wasm_flush`, "the second tab");
    const stored = await other.eval(`Module.sumatraStoreUpload("/uploads", ${JSON.stringify(OTHER_TAB_NAME)},
      new TextEncoder().encode("%PDF-1.4 another tab"))`);
    if (stored !== `/uploads/${OTHER_TAB_NAME}`) throw new Error(`ad-hoc-wasm-host: second tab stored ${stored}`);
    await waitFor(cdp, opfsHas("uploads", OTHER_TAB_NAME), "the second tab's document in OPFS");
    await other.send("Target.closeTarget", { targetId });
  } finally {
    other.close();
  }
  const err = await cdp.eval(`new Promise((r) => Module.sumatraStorageSave((e) => r(e ? String(e) : "")))`);
  if (err) throw new Error(`ad-hoc-wasm-host: saving failed: ${err}`);
  if (!(await cdp.eval(opfsHas("uploads", OTHER_TAB_NAME)))) {
    throw new Error("ad-hoc-wasm-host: this tab's save deleted a document another tab stored");
  }
  console.log("ad-hoc-wasm-host: ok: a save keeps documents another tab stored");
}

// A different file under a name already stored gets a numbered name; the same
// bytes reuse the stored file.
async function testSameNameUpload(cdp: Cdp): Promise<void> {
  const other = makeTextPdf("a different file, same name").toString("base64");
  // the File menu from testNoSecondWindow can still be up
  await clickAt(cdp, NEUTRAL_POINT.x, NEUTRAL_POINT.y);
  await Bun.sleep(400);
  await cdp.eval(`window.__pickName = "${PICKED_NAME}"; window.__pickB64 = "${other}"`);
  await clickAt(cdp, OPEN_BUTTON_UNDER_TABS.x, OPEN_BUTTON_UNDER_TABS.y);
  const renamed = PICKED_NAME.replace(".pdf", " (2).pdf");
  await waitFor(cdp, `Module.FS.analyzePath("/uploads/${renamed}").exists`, "the renamed upload");
  await waitFor(cdp, opfsHas("uploads", renamed), "the renamed upload in OPFS");
  await cdp.eval(`window.__pickB64 = ""`);
  const again = await cdp.eval(`Module.sumatraStoreUpload("/uploads", "${PICKED_NAME}",
    Module.FS.readFile("/uploads/${PICKED_NAME}"))`);
  if (again !== `/uploads/${PICKED_NAME}`) throw new Error(`ad-hoc-wasm-host: the same bytes went to ${again}`);
  await Bun.sleep(SETTLE_MS);
  console.log("ad-hoc-wasm-host: ok: a same-named upload doesn't overwrite");
}

// gpui passes only the names of dropped files; the page stores the bytes
async function testDrop(cdp: Cdp): Promise<void> {
  const local = tmpPath(DROPPED_NAME);
  writeFileSync(local, makeTextPdf("dropped on the page"));
  const at = { x: 400, y: 300 };
  const data = { items: [], files: [local], dragOperationsMask: 1 };
  for (const type of ["dragEnter", "dragOver", "drop"]) {
    await cdp.send("Input.dispatchDragEvent", { type, ...at, data });
  }
  await waitFor(cdp, `Module.FS.analyzePath("/uploads/${DROPPED_NAME}").exists`, "the dropped file in /uploads");
  await Bun.sleep(SETTLE_MS);
  await saveShot(cdp, "dropped");
  console.log("ad-hoc-wasm-host: ok: a dropped file is stored");
}

// Chords gpui leaves with the browser go to the app when bound; the ones the
// browser has to keep stay with it
async function testKeyRouting(cdp: Cdp): Promise<void> {
  await cdp.eval(`window.__keysSeen = [];
    window.addEventListener("keydown", (e) => window.__keysSeen.push(e.code + ":" + e.defaultPrevented));`);
  await pressKey(cdp, "g", "KeyG", 71, CTRL_MODIFIER);
  await Bun.sleep(400);
  await saveShot(cdp, "ctrl-g");
  await pressKey(cdp, "Escape", "Escape", ESCAPE_VK);
  // Ctrl+L only: of the keys the browser keeps, F5 reloads the page and F12
  // opens developer tools, which shrinks the page even when headless
  await pressKey(cdp, "l", "KeyL", 76, CTRL_MODIFIER);
  const seen: string[] = await cdp.eval(`window.__keysSeen`);
  if (seen.some((k) => k.startsWith("KeyG:"))) throw new Error(`ad-hoc-wasm-host: Ctrl+G reached the browser: ${seen}`);
  if (!seen.includes("KeyL:false")) {
    throw new Error(`ad-hoc-wasm-host: Ctrl+L didn't stay with the browser: ${seen}`);
  }
  console.log("ad-hoc-wasm-host: ok: bound chords go to the app, Ctrl+L to the browser");
}

// Stored documents are listed at startup and read when opened: the restored
// session's document is read, the one another tab stored isn't, and it still
// has its size and its bytes when something reads it.
async function testLazyDocuments(cdp: Cdp): Promise<void> {
  const loaded: string[] = await cdp.eval(`Object.keys(Module.sumatraLazyLoaded)`);
  const other = `/uploads/${OTHER_TAB_NAME}`;
  if (loaded.length === 0) throw new Error("ad-hoc-wasm-host: the restored session read no document");
  if (loaded.includes(other)) throw new Error(`ad-hoc-wasm-host: startup read a document nobody opened: ${loaded}`);
  const text = await cdp.eval(`new TextDecoder().decode(Module.FS.readFile("${other}"))`);
  if (text !== "%PDF-1.4 another tab") throw new Error(`ad-hoc-wasm-host: lazy read gave ${JSON.stringify(text)}`);
  console.log(`ad-hoc-wasm-host: ok: startup read only ${loaded.join(", ")}`);
}

function hostScript(pdfB64: string): string {
  return `
    window.__calls = [];
    if (!location.search.includes("nohost")) window.sumatraHost = {
      apiVersion: 1,
      pickFile(done) {
        window.__calls.push("pickFile");
        const b64 = window.__pickB64 || ${JSON.stringify(pdfB64)};
        const bytes = Uint8Array.from(atob(b64), (c) => c.charCodeAt(0));
        const name = "dir/" + (window.__pickName || "${PICKED_NAME}");
        setTimeout(() => done({ name, bytes }), 0);
        return true;
      },
      download({ name, path, bytes }) {
        window.__calls.push("download " + name + " " + path + " " + (bytes().length > 0));
        return true;
      },
      openUrl({ url }) {
        window.__calls.push("openUrl " + url);
        return true;
      },
      exit({ saved }) {
        window.__calls.push("exit saved=" + saved);
        sessionStorage.setItem("exitCalls", JSON.stringify(window.__calls));
        navigator.storage.getDirectory()
          .then((r) => r.getDirectoryHandle("settings"))
          .then((d) => d.getFileHandle("SumatraPDF-settings.txt"))
          .then((f) => f.getFile())
          .then((f) => f.text())
          .then((t) => sessionStorage.setItem("settingsAtExit", t))
          .catch((e) => sessionStorage.setItem("settingsAtExit", "ERROR " + e))
          .finally(() => { location.href = "/home"; });
        return true;
      },
    };`;
}

export async function testit(): Promise<void> {
  const browser = BROWSERS.find((b) => existsSync(b));
  if (!browser) {
    console.log("ad-hoc-wasm-host: no Chrome or Edge, skipping");
    return;
  }
  const build = spawnSync("bun", ["cmd/ng-build.ts", "-wasm", "-rel", "SumatraPDF"], { cwd: ROOT, stdio: "inherit" });
  if (build.status !== 0) throw new Error("ad-hoc-wasm-host: wasm build failed");

  // OPFS needs a secure context, which localhost is
  const server = Bun.serve({
    port: 0,
    async fetch(req) {
      const path = new URL(req.url).pathname;
      if (path === "/home") {
        return new Response(`<title>${HOME_MARKER}</title>`, { headers: { "Content-Type": "text/html" } });
      }
      const f = Bun.file(join(OUT_DIR, path === "/" ? "SumatraPDF.html" : path));
      return (await f.exists()) ? new Response(f) : new Response("not found", { status: 404 });
    },
  });

  const profile = mkdtempSync(join(tmpdir(), "wasm-host-"));
  const proc = Bun.spawn(
    [
      browser,
      "--headless=new",
      "--remote-debugging-port=0",
      `--user-data-dir=${profile}`,
      "--no-first-run",
      "about:blank",
    ],
    { stdout: "ignore", stderr: "pipe" },
  );
  let cdp: Cdp | null = null;
  try {
    const portFile = join(profile, "DevToolsActivePort");
    const deadline = Date.now() + STEP_TIMEOUT_MS;
    // the browser may still hold the file open (EBUSY on Windows)
    const portLines = () => {
      try {
        return readFileSync(portFile, "utf8").split("\n").length;
      } catch {
        return 0;
      }
    };
    while (portLines() < 2) {
      if (Date.now() > deadline) throw new Error("ad-hoc-wasm-host: the browser didn't start");
      await Bun.sleep(100);
    }
    const [port, wsPath] = readFileSync(portFile, "utf8").split("\n");
    const wsUrl = `ws://127.0.0.1:${port}${wsPath}`;
    cdp = await Cdp.connect(wsUrl);
    const { targetId } = await cdp.send("Target.createTarget", { url: "about:blank" });
    const { sessionId } = await cdp.send("Target.attachToTarget", { targetId, flatten: true });
    cdp.attach(sessionId);
    await cdp.send("Page.enable");
    await cdp.send("Runtime.enable");
    // headless: without this the page never counts as focused, and keys are dropped
    await cdp.send("Emulation.setFocusEmulationEnabled", { enabled: true });

    const pdfB64 = makeTextPdf("picked by the host").toString("base64");
    await cdp.send("Page.addScriptToEvaluateOnNewDocument", { source: hostScript(pdfB64) });
    await cdp.send("Page.navigate", { url: `http://localhost:${server.port}/` });

    await waitFor(cdp, `window.__sumatraStorage === "ready" && !!document.querySelector("#gpui-canvas")`, "storage");
    await waitFor(cdp, `!document.getElementById("gpui-loading")`, "the first frame");
    await cdp.eval(`document.getElementById("gpui-canvas").focus()`);
    await Bun.sleep(500);

    // also pickFile: the bytes land in /uploads, without the directory part
    await testHomeTab(cdp);
    await testNoSecondWindow(cdp);
    await testOtherTabKeepsItsDocument(cdp, wsUrl, `http://localhost:${server.port}/`);
    await testSameNameUpload(cdp);
    await testDrop(cdp);
    await testKeyRouting(cdp);

    // download: Save As, then accepting the name the dialog offers
    await waitFor(cdp, `Module.FS.analyzePath("/uploads/${PICKED_NAME}").exists`, "the document");
    await Bun.sleep(1000);
    await runFromPalette(cdp, SAVE_AS_COMMAND);
    await Bun.sleep(600);
    await pressKey(cdp, "Enter", "Enter", ENTER_VK);
    await Bun.sleep(400);
    // the name is taken: Enter again is Yes
    await pressKey(cdp, "Enter", "Enter", ENTER_VK);
    await waitFor(
      cdp,
      `window.__calls.some((c) => c.startsWith("download ") && c.endsWith(" true"))`,
      "the download hook",
    );

    // openUrl: Help opens the website, and no new tab appears
    await runFromPalette(cdp, HELP_COMMAND);
    await waitFor(cdp, `window.__calls.some((c) => c.startsWith("openUrl https://"))`, "the openUrl hook");
    await Bun.sleep(500);
    const targets = (await cdp.send("Target.getTargets")).targetInfos as { type: string; url: string }[];
    if (targets.some((t) => t.type === "page" && t.url.startsWith("https://"))) {
      throw new Error("ad-hoc-wasm-host: openUrl also opened a tab");
    }

    // exit: runs after the state reached OPFS, and may navigate away
    await runFromPalette(cdp, EXIT_COMMAND);
    await waitFor(cdp, `document.title === ${JSON.stringify(HOME_MARKER)}`, "navigating to the home page");
    const calls = JSON.parse(await cdp.eval(`sessionStorage.getItem("exitCalls")`));
    if (!calls.includes("exit saved=true")) throw new Error(`ad-hoc-wasm-host: exit calls: ${calls}`);
    const settings = String(await cdp.eval(`sessionStorage.getItem("settingsAtExit")`));
    if (!settings.includes(PICKED_NAME)) {
      throw new Error(`ad-hoc-wasm-host: the opened document isn't in the saved settings:\n${settings.slice(0, 400)}`);
    }
    console.log(`ad-hoc-wasm-host: ok: ${calls.join(", ")}`);

    // no host: the browser's own file input still works
    await cdp.send("Page.setInterceptFileChooserDialog", { enabled: true });
    await cdp.send("DOM.enable");
    await cdp.send("Page.navigate", { url: `http://localhost:${server.port}/?nohost` });
    await waitFor(
      cdp,
      `window.__sumatraStorage === "ready" && !document.getElementById("gpui-loading")`,
      "the page without a host",
    );
    await Bun.sleep(1500);
    await testLazyDocuments(cdp);
    cdp.events.length = 0;
    // the session brought the document back, so the tab bar pushed the toolbar down
    await clickAt(cdp, OPEN_BUTTON_UNDER_TABS.x, OPEN_BUTTON_UNDER_TABS.y);
    const deadline2 = Date.now() + STEP_TIMEOUT_MS;
    let chooser;
    while (!(chooser = cdp.events.find((e) => e.method === "Page.fileChooserOpened"))) {
      if (Date.now() > deadline2) throw new Error("ad-hoc-wasm-host: no file chooser without a host");
      await Bun.sleep(100);
    }
    const local = tmpPath(LOCAL_NAME);
    writeFileSync(local, makeTextPdf("picked in the file chooser"));
    await cdp.send("DOM.setFileInputFiles", { files: [local], backendNodeId: chooser.params.backendNodeId });
    await waitFor(cdp, `Module.FS.analyzePath("/uploads/${LOCAL_NAME}").exists`, "the chosen file in /uploads");
    console.log("ad-hoc-wasm-host: ok: the file input works without a host");

    // exit without a host: nothing navigates away, so the page says so
    await Bun.sleep(SETTLE_MS);
    await runFromPalette(cdp, EXIT_COMMAND);
    await waitFor(cdp, `document.body.innerText.includes(${JSON.stringify(CLOSED_MARKER)})`, "the closed page");
    console.log("ad-hoc-wasm-host: ok: exit without a host shows the closed page");
  } catch (e) {
    console.log("calls:", await cdp?.eval("JSON.stringify(window.__calls)").catch(() => "?"));
    const shot = await cdp?.send("Page.captureScreenshot").catch(() => null);
    if (shot) {
      const png = tmpPath("ad-hoc-wasm-host.png");
      writeFileSync(png, Buffer.from(shot.data, "base64"));
      console.log(`ad-hoc-wasm-host: screenshot in ${png}`);
    }
    throw e;
  } finally {
    cdp?.close();
    proc.kill();
    await proc.exited;
    server.stop(true);
    rmSync(profile, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await testit();
  console.log("✅ ad-hoc-wasm-host passed");
  process.exit(0);
}
