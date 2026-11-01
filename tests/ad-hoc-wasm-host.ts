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
const SECOND_NAME = "opened-from-document.pdf";
const THIRD_NAME = "opened-from-home.pdf";
const ESCAPE_VK = 27;
// the tab strip's first cell
const MENU_BUTTON = { x: 14, y: 12 };
const HOME_TAB = { x: 52, y: 12 };
const HOME_OPEN_BUTTON = { x: 612, y: 63 };
// the toolbar's first button
const OPEN_BUTTON_UNDER_TABS = { x: 20, y: 42 };
const BELOW_TABS_Y = 30;
// beside the page: hovers nothing
const NEUTRAL_POINT = { x: 100, y: 300 };
const SETTLE_MS = 1500;
const PALETTE_INPUT = { x: 300, y: 64 };
const OVERWRITE_YES = { x: 575, y: 437 };

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
export async function runFromPalette(cdp: Cdp, title: string): Promise<void> {
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
function hostScript(pdfB64: string): string {
  return `
    window.__calls = [];
    if (!location.search.includes("nohost")) window.sumatraHost = {
      apiVersion: 1,
      pickFile(done) {
        window.__calls.push("pickFile");
        const bytes = Uint8Array.from(atob(${JSON.stringify(pdfB64)}), (c) => c.charCodeAt(0));
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
    while (!existsSync(portFile) || readFileSync(portFile, "utf8").split("\n").length < 2) {
      if (Date.now() > deadline) throw new Error("ad-hoc-wasm-host: the browser didn't start");
      await Bun.sleep(100);
    }
    const [port, wsPath] = readFileSync(portFile, "utf8").split("\n");
    cdp = await Cdp.connect(`ws://127.0.0.1:${port}${wsPath}`);
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

    // download: Save As, then accepting the name the dialog offers
    await waitFor(cdp, `Module.FS.analyzePath("/uploads/${PICKED_NAME}").exists`, "the document");
    await Bun.sleep(1000);
    await runFromPalette(cdp, SAVE_AS_COMMAND);
    await Bun.sleep(600);
    await pressKey(cdp, "Enter", "Enter", ENTER_VK);
    await Bun.sleep(400);
    await clickAt(cdp, OVERWRITE_YES.x, OVERWRITE_YES.y);
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
