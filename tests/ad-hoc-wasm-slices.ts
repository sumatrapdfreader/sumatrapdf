// The wasm build has no threads: search, the match count and thumbnails run on
// the page's only thread, in slices, so the page keeps painting and answering
// input. This loads a long PDF in headless Chrome or Edge, searches it and shows
// thumbnails, and fails when the page stalls for long. Ad-hoc: it needs a wasm
// build (emsdk) and a browser.

import { existsSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { Cdp, waitFor } from "./ad-hoc-wasm-host.ts";
import { assemblePdf, ROOT } from "./util.ts";

const OUT_DIR = join(ROOT, "out", "wasm", "rel");
const BROWSERS = [
  "C:/Program Files/Google/Chrome/Application/chrome.exe",
  "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe",
];
const STEP_TIMEOUT_MS = 60_000;
const DOC_NAME = "long.pdf";
const PAGES = 2000;
const LINES_PER_PAGE = 45;
const NEEDLE = "needle";
const NEEDLE_PAGES = [1500, 1900];
const SETTLE_MS = 1500;
const CTRL_MODIFIER = 2;
// one slice is 20 ms; the rest is room for a page render and a busy machine
const MAX_STALL_MS = 250;
const WATCHDOG_MS = 5 * 60_000;

const HELVETICA = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";

function makeLongPdf(): Buffer {
  const objs = ["<< /Type /Catalog /Pages 2 0 R >>", "", HELVETICA];
  const kids: string[] = [];
  for (let page = 1; page <= PAGES; page++) {
    const pageNum = objs.length + 1;
    kids.push(`${pageNum} 0 R`);
    let content = "BT /F1 11 Tf 13 TL 50 760 Td ";
    for (let line = 0; line < LINES_PER_PAGE; line++) {
      const extra = NEEDLE_PAGES.includes(page) && line === 20 ? ` ${NEEDLE}` : "";
      content += `(page ${page} line ${line} lorem ipsum dolor sit amet consectetur adipiscing${extra}) ' `;
    }
    content += "ET";
    objs.push(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
        `/Resources << /Font << /F1 3 0 R >> >> /Contents ${pageNum + 1} 0 R >>`,
    );
    objs.push(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);
  }
  objs[1] = `<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${PAGES} >>`;
  return Buffer.from(assemblePdf(objs, { header: "%PDF-1.5\n" }), "latin1");
}

// the longest the page went without running a 5 ms timer, since the last call
const STALL_SCRIPT = `
  window.__stall = { last: performance.now(), max: 0 };
  setInterval(() => {
    const now = performance.now();
    window.__stall.max = Math.max(window.__stall.max, now - window.__stall.last);
    window.__stall.last = now;
  }, 5);`;

async function resetStall(cdp: Cdp): Promise<void> {
  await cdp.eval(`window.__stall.max = 0; window.__stall.last = performance.now()`);
}

async function stall(cdp: Cdp): Promise<number> {
  return Math.round(await cdp.eval(`window.__stall.max`));
}

async function pressKey(cdp: Cdp, key: string, code: string, vk: number, modifiers = 0): Promise<void> {
  const base = { key, code, windowsVirtualKeyCode: vk, nativeVirtualKeyCode: vk, modifiers };
  await cdp.send("Input.dispatchKeyEvent", { type: "rawKeyDown", ...base });
  await cdp.send("Input.dispatchKeyEvent", { type: "keyUp", ...base });
}

async function typeText(cdp: Cdp, s: string): Promise<void> {
  for (const ch of s) {
    const key = {
      key: ch,
      text: ch,
      code: `Key${ch.toUpperCase()}`,
      windowsVirtualKeyCode: ch.toUpperCase().charCodeAt(0),
    };
    if (ch === "&") Object.assign(key, { code: "Digit7", windowsVirtualKeyCode: 55, modifiers: 8 });
    await cdp.send("Input.dispatchKeyEvent", { type: "keyDown", ...key });
    await cdp.send("Input.dispatchKeyEvent", { type: "keyUp", ...key });
    await Bun.sleep(30);
  }
}

async function commandId(cdp: Cdp, name: string): Promise<number> {
  await cdp.eval(`Module._sumatra_test_visibility()`);
  const text: string = await cdp.eval(`window.__testVisibility`);
  for (const line of text.split("\n")) {
    const [id, n] = line.split("\t");
    if (n === name) return Number(id);
  }
  throw new Error(`ad-hoc-wasm-slices: no command ${name}`);
}

function checkStall(what: string, ms: number): void {
  if (ms > MAX_STALL_MS) throw new Error(`ad-hoc-wasm-slices: the page stalled ${ms} ms during ${what}`);
  console.log(`ad-hoc-wasm-slices: ok: ${what}, longest stall ${ms} ms`);
}

async function testSearch(cdp: Cdp): Promise<void> {
  await pressKey(cdp, "f", "KeyF", 70, CTRL_MODIFIER);
  await Bun.sleep(600);
  await resetStall(cdp);
  const start = Date.now();
  // typing searches as you type, and counts the matches in the whole document
  await typeText(cdp, NEEDLE);
  await Bun.sleep(600);
  const count = await waitFor(
    cdp,
    `Module._sumatra_test_find() >= 0 ? Module._sumatra_test_find() + 1 : 0`,
    "the match count",
  );
  // Find Next: as you type gives up on a match that isn't near; this doesn't
  await pressKey(cdp, "Enter", "Enter", 13);
  await Bun.sleep(300);
  await waitFor(cdp, `Module._sumatra_test_find() >= 0`, "the find");
  const ms = Date.now() - start;
  const page = await cdp.eval(`Module._sumatra_test_page()`);
  if (count - 1 !== NEEDLE_PAGES.length) {
    throw new Error(`ad-hoc-wasm-slices: counted ${count - 1} matches, expected ${NEEDLE_PAGES.length}`);
  }
  if (page !== NEEDLE_PAGES[0]) throw new Error(`ad-hoc-wasm-slices: the find went to page ${page}`);
  checkStall(`search of ${PAGES} pages (${ms} ms, at page ${page})`, await stall(cdp));
  await pressKey(cdp, "Escape", "Escape", 27);
  await Bun.sleep(400);
}

async function testSidebarThumbnails(cdp: Cdp): Promise<void> {
  const id = await commandId(cdp, "CmdToggleThumbnails");
  await resetStall(cdp);
  await cdp.eval(`Module._sumatra_test_run(${id})`);
  await Bun.sleep(4000);
  checkStall("sidebar thumbnails", await stall(cdp));
  await cdp.eval(`Module._sumatra_test_run(${id})`);
  await Bun.sleep(SETTLE_MS);
}

async function testPaletteThumbnails(cdp: Cdp): Promise<void> {
  await pressKey(cdp, "k", "KeyK", 75, CTRL_MODIFIER);
  await Bun.sleep(600);
  await resetStall(cdp);
  await typeText(cdp, "&");
  await Bun.sleep(4000);
  checkStall("command palette thumbnails", await stall(cdp));
  await pressKey(cdp, "Escape", "Escape", 27);
  await Bun.sleep(400);
}

export async function testit(): Promise<void> {
  const browser = BROWSERS.find((b) => existsSync(b));
  if (!browser) {
    console.log("ad-hoc-wasm-slices: no Chrome or Edge, skipping");
    return;
  }
  const build = spawnSync("bun", ["cmd/ng-build.ts", "-wasm", "-rel", "SumatraPDF"], { cwd: ROOT, stdio: "inherit" });
  if (build.status !== 0) throw new Error("ad-hoc-wasm-slices: wasm build failed");

  const pdf = makeLongPdf();
  const server = Bun.serve({
    port: 0,
    async fetch(req) {
      const path = new URL(req.url).pathname;
      if (path === "/blank") {
        return new Response("<title>blank</title>", { headers: { "Content-Type": "text/html" } });
      }
      if (path === "/long.pdf") return new Response(pdf);
      const f = Bun.file(join(OUT_DIR, path === "/" ? "SumatraPDF.html" : path));
      return (await f.exists()) ? new Response(f) : new Response("not found", { status: 404 });
    },
  });
  const base = `http://localhost:${server.port}/`;
  const profile = mkdtempSync(join(tmpdir(), "wasm-slices-"));
  const proc = Bun.spawn(
    [browser, "--headless=new", "--remote-debugging-port=0", `--user-data-dir=${profile}`, "--no-first-run"],
    { stdout: "ignore", stderr: "pipe" },
  );
  let cdp: Cdp | null = null;
  try {
    const portFile = join(profile, "DevToolsActivePort");
    const deadline = Date.now() + STEP_TIMEOUT_MS;
    let lines: string[] = [];
    while (lines.length < 2) {
      if (Date.now() > deadline) throw new Error("ad-hoc-wasm-slices: the browser didn't start");
      try {
        lines = readFileSync(portFile, "utf8").split("\n");
      } catch {
        // not written yet, or still open (EBUSY on Windows)
      }
      await Bun.sleep(100);
    }
    cdp = await Cdp.connect(`ws://127.0.0.1:${lines[0]}${lines[1]}`);
    const { targetId } = await cdp.send("Target.createTarget", { url: "about:blank" });
    const { sessionId } = await cdp.send("Target.attachToTarget", { targetId, flatten: true });
    cdp.attach(sessionId);
    await cdp.send("Page.enable");
    await cdp.send("Runtime.enable");
    await cdp.send("Emulation.setFocusEmulationEnabled", { enabled: true });
    await cdp.send("Emulation.setDeviceMetricsOverride", {
      width: 1100,
      height: 800,
      deviceScaleFactor: 1,
      mobile: false,
    });
    await cdp.send("Page.addScriptToEvaluateOnNewDocument", { source: STALL_SCRIPT });

    // the PDF goes to OPFS before the app starts, as a stored upload would
    await cdp.send("Page.navigate", { url: `${base}blank` });
    await waitFor(cdp, `document.title === "blank"`, "the blank page");
    await cdp.eval(`(async () => {
      const r = await navigator.storage.getDirectory();
      for (const n of ["settings", "uploads"]) { try { await r.removeEntry(n, { recursive: true }); } catch {} }
      const bytes = new Uint8Array(await (await fetch("/long.pdf")).arrayBuffer());
      const d = await r.getDirectoryHandle("uploads", { create: true });
      const w = await (await d.getFileHandle("${DOC_NAME}", { create: true })).createWritable();
      await w.write(bytes);
      await w.close();
    })()`);
    await cdp.send("Page.navigate", { url: `${base}?test-hooks&file=/uploads/${DOC_NAME}` });
    await waitFor(
      cdp,
      `window.__sumatraStorage === "ready" && !document.getElementById("gpui-loading") && !!Module._sumatra_test_find`,
      "the app",
    );
    await waitFor(cdp, `Module._sumatra_test_page() === 1`, "the document");
    await cdp.eval(`document.getElementById("gpui-canvas").focus()`);
    await Bun.sleep(SETTLE_MS);

    await testSearch(cdp);
    await testSidebarThumbnails(cdp);
    await testPaletteThumbnails(cdp);
  } finally {
    cdp?.close();
    proc.kill();
    await proc.exited;
    server.stop(true);
    rmSync(profile, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  // a page that never yields again blocks every DevTools call, so no step's
  // own timeout fires
  const watchdog = setTimeout(() => {
    console.log("ad-hoc-wasm-slices: the page stopped responding");
    process.exit(1);
  }, WATCHDOG_MS);
  await testit();
  clearTimeout(watchdog);
  console.log("✅ ad-hoc-wasm-slices passed");
  process.exit(0);
}
