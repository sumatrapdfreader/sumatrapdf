// The wasm build in Chrome, Firefox and WebKit (Playwright's builds: WebKit on
// Windows is the closest to Safari there is here, but it isn't Safari). Per
// browser: the app starts, a stored document comes back after a reload, search
// works, Ctrl+wheel and Safari's pinch gestures zoom the document and not the
// page, and without a host the file input and downloads work. Also checks the
// OPFS writer for browsers without createWritable (Safari before 26), in Chrome
// with createWritable taken away.
//
// Ad-hoc: it needs a wasm build (emsdk) and Playwright's browsers:
//   cd tests && bun install && bunx playwright install firefox webkit
// `bun tests/ad-hoc-wasm-browsers.ts firefox` runs one browser.

import { existsSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { chromium, firefox, webkit, type BrowserContext, type BrowserType, type Page } from "playwright";
import { assemblePdf, ROOT, tmpPath } from "./util.ts";

const OUT_DIR = join(ROOT, "out", "wasm", "rel");
const STEP_TIMEOUT_MS = 30_000;
const DOC_NAME = "browsers.pdf";
const PICKED_NAME = "picked.pdf";
const NEEDLE = "needle";
const NEEDLE_PAGES = [2, 4];
const PAGES = 5;
// the toolbar's first button, under the tab bar
const OPEN_BUTTON = { x: 20, y: 42 };
const PAGE_POINT = { x: 550, y: 450 };
const SETTLE_MS = 1200;
// CmdSendByEmail in src/shared/Commands.h
const CMD_SEND_BY_EMAIL = 216;

const HELVETICA = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";

function makePdf(label: string): Buffer {
  const objs = ["<< /Type /Catalog /Pages 2 0 R >>", "", HELVETICA];
  const kids: string[] = [];
  for (let page = 1; page <= PAGES; page++) {
    const pageNum = objs.length + 1;
    kids.push(`${pageNum} 0 R`);
    const extra = NEEDLE_PAGES.includes(page) ? ` ${NEEDLE}` : "";
    const content = `BT /F1 18 Tf 72 700 Td (${label} page ${page}${extra}) Tj ET`;
    objs.push(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
        `/Resources << /Font << /F1 3 0 R >> >> /Contents ${pageNum + 1} 0 R >>`,
    );
    objs.push(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);
  }
  objs[1] = `<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${PAGES} >>`;
  return Buffer.from(assemblePdf(objs, { header: "%PDF-1.5\n" }), "latin1");
}

async function waitFor(page: Page, fn: string, what: string): Promise<any> {
  const deadline = Date.now() + STEP_TIMEOUT_MS;
  for (;;) {
    const v = await page.evaluate(fn).catch(() => null);
    if (v) return v;
    if (Date.now() > deadline) throw new Error(`timed out waiting for ${what}`);
    await Bun.sleep(100);
  }
}

async function openApp(page: Page, base: string, query: string): Promise<void> {
  await page.goto(base + "?test-hooks" + query);
  await waitFor(
    page,
    `window.__sumatraStorage && !document.getElementById("gpui-loading") && !!window.Module?._sumatra_test_page`,
    "the app",
  );
  await page.evaluate(`document.getElementById("gpui-canvas").focus()`);
  await Bun.sleep(SETTLE_MS);
}

type Result = { name: string; notes: string[] };

// Stores a document. True when it reached OPFS; the WebKit build for Windows
// takes the write and keeps an empty file, which the app has to report.
async function storeDocument(page: Page, r: Result): Promise<boolean> {
  const b64 = makePdf("stored").toString("base64");
  await page.evaluate(async (data) => {
    const bytes = Uint8Array.from(atob(data), (c) => c.charCodeAt(0));
    await (window as any).Module.sumatraStoreUpload("/uploads", "browsers.pdf", bytes);
  }, b64);
  const err: string | undefined = await page.evaluate(`window.Module.__storageError`);
  if (!err) return true;
  if (r.name === "webkit" && /kept 0 of/.test(err)) {
    r.notes.push(`OPFS keeps nothing in this WebKit build; the app reports it ("${err}")`);
    return false;
  }
  throw new Error(`storing a document failed: ${err}`);
}

async function testSearch(page: Page): Promise<void> {
  await page.keyboard.press("Control+F");
  await Bun.sleep(500);
  await page.keyboard.type(NEEDLE, { delay: 30 });
  await Bun.sleep(600);
  const n = await waitFor(page, `Module._sumatra_test_find() >= 0 ? Module._sumatra_test_find() + 1 : 0`, "the count");
  if (n - 1 !== NEEDLE_PAGES.length) throw new Error(`search counted ${n - 1} matches`);
  await page.keyboard.press("Enter");
  await Bun.sleep(300);
  await waitFor(page, `Module._sumatra_test_find() >= 0`, "the find");
  const pageNo = await page.evaluate(`Module._sumatra_test_page()`);
  if (!NEEDLE_PAGES.includes(pageNo)) throw new Error(`the find went to page ${pageNo}`);
  await page.keyboard.press("Escape");
  await Bun.sleep(300);
}

async function zoom(page: Page): Promise<number> {
  return page.evaluate(`Module._sumatra_test_zoom()`);
}

// zooms the document, and the browser doesn't zoom the page
async function testCtrlWheel(page: Page): Promise<void> {
  await page.evaluate(`window.__wheels = [];
    window.addEventListener("wheel", (e) => window.__wheels.push(e.defaultPrevented));`);
  const before = await zoom(page);
  await page.mouse.move(PAGE_POINT.x, PAGE_POINT.y);
  await page.keyboard.down("Control");
  await page.mouse.wheel(0, -300);
  await page.keyboard.up("Control");
  await Bun.sleep(600);
  const after = await zoom(page);
  const wheels: boolean[] = await page.evaluate(`window.__wheels`);
  if (wheels.length === 0 || wheels.some((p) => !p)) throw new Error(`Ctrl+wheel reached the page: ${wheels}`);
  if (!(after > before)) throw new Error(`Ctrl+wheel didn't zoom in: ${before} -> ${after}`);
}

// Safari's trackpad pinch. The events are made up here (only Safari on a Mac
// sends them), with the properties the page reads.
async function testPinchGesture(page: Page): Promise<void> {
  const before = await zoom(page);
  const prevented = await page.evaluate(`(() => {
    const send = (type, scale) => {
      const e = new Event(type, { bubbles: true, cancelable: true });
      Object.defineProperty(e, "scale", { value: scale });
      Object.defineProperty(e, "clientX", { value: ${PAGE_POINT.x} });
      Object.defineProperty(e, "clientY", { value: ${PAGE_POINT.y} });
      document.getElementById("gpui-canvas").dispatchEvent(e);
      return e.defaultPrevented;
    };
    return [send("gesturestart", 1), send("gesturechange", 1.3), send("gesturechange", 1.6), send("gestureend", 1.6)];
  })()`);
  await Bun.sleep(600);
  const after = await zoom(page);
  if ((prevented as boolean[]).some((p) => !p)) throw new Error(`a gesture event reached the page: ${prevented}`);
  if (!(after > before)) throw new Error(`pinching didn't zoom in: ${before} -> ${after}`);
}

// no host: Open uses the browser's file input
async function testFileInput(page: Page): Promise<void> {
  const local = tmpPath(PICKED_NAME);
  writeFileSync(local, makePdf("picked"));
  await page.mouse.move(OPEN_BUTTON.x, OPEN_BUTTON.y);
  const chooser = page.waitForEvent("filechooser", { timeout: STEP_TIMEOUT_MS });
  await page.mouse.click(OPEN_BUTTON.x, OPEN_BUTTON.y);
  await (await chooser).setFiles(local);
  await waitFor(page, `Module.FS.analyzePath("/uploads/${PICKED_NAME}").exists`, "the chosen file");
  await waitFor(page, `Module._sumatra_test_page() === 1`, "the chosen file to open");
  await Bun.sleep(SETTLE_MS);
}

// no host: Save As downloads the file
async function testDownload(page: Page): Promise<void> {
  const download = page.waitForEvent("download", { timeout: STEP_TIMEOUT_MS });
  await page.keyboard.press("Control+S");
  await Bun.sleep(800);
  await page.keyboard.press("Enter");
  await Bun.sleep(500);
  // the name is taken: Enter again is Yes
  await page.keyboard.press("Enter");
  const d = await download;
  if (!d.suggestedFilename().endsWith(".pdf")) throw new Error(`downloaded ${d.suggestedFilename()}`);
}

// without a share sheet (Firefox), Send by email downloads the file to attach
async function testSendByEmail(page: Page): Promise<boolean> {
  const hasShare = await page.evaluate(`!!(navigator.share && navigator.canShare)`);
  if (hasShare) return false;
  const download = page.waitForEvent("download", { timeout: STEP_TIMEOUT_MS });
  await page.evaluate(`Module._sumatra_test_run(${CMD_SEND_BY_EMAIL})`);
  const d = await download;
  if (d.suggestedFilename() !== PICKED_NAME) throw new Error(`Send by email downloaded ${d.suggestedFilename()}`);
  return true;
}

async function runBrowser(name: string, type: BrowserType, base: string): Promise<Result> {
  const r: Result = { name, notes: [] };
  const profile = mkdtempSync(join(tmpdir(), `wasm-${name}-`));
  // WebKit has no OPFS in a throwaway (private) context
  const ctx: BrowserContext = await type.launchPersistentContext(profile, {
    ...(name === "chrome" ? { channel: "chrome" } : {}),
    viewport: { width: 1100, height: 800 },
    acceptDownloads: true,
  });
  const errors: string[] = [];
  try {
    const page = ctx.pages()[0] ?? (await ctx.newPage());
    page.on("pageerror", (e) => errors.push(e.message));
    await openApp(page, base, "");
    r.notes.push(`${type.name()} ${ctx.browser()?.version() ?? ""}`.trim());
    const stored = await storeDocument(page, r);
    // a stored document comes back after a reload, read when it's opened
    await openApp(page, base, stored ? `&file=/uploads/${DOC_NAME}` : "&file=/docs/zlib.3.pdf");
    await waitFor(page, `Module._sumatra_test_page() === 1`, "the document");
    if (stored) r.notes.push("a stored document comes back after a reload");
    if (stored) {
      await testSearch(page);
      r.notes.push("search");
    }
    await testCtrlWheel(page);
    r.notes.push("Ctrl+wheel zooms the document");
    await testPinchGesture(page);
    r.notes.push("pinch gestures zoom the document");
    await testFileInput(page);
    r.notes.push("the file input");
    await testDownload(page);
    r.notes.push("Save As downloads");
    if (await testSendByEmail(page)) r.notes.push("Send by email downloads (no share sheet)");
    if (errors.length > 0) throw new Error(`page errors: ${errors.join("; ")}`);
  } catch (e) {
    const page = ctx.pages()[0];
    if (page) await page.screenshot({ path: tmpPath(`ad-hoc-wasm-browsers-${name}.png`) }).catch(() => {});
    throw new Error(`${name}: ${(e as Error).message} (screenshot in ${tmpPath(`ad-hoc-wasm-browsers-${name}.png`)})`);
  } finally {
    await ctx.close();
    rmSync(profile, { recursive: true, force: true });
  }
  return r;
}

// Safari before 26 writes OPFS files from a worker; Chrome has that too
async function testWorkerWriter(base: string): Promise<void> {
  const profile = mkdtempSync(join(tmpdir(), "wasm-writer-"));
  const ctx = await chromium.launchPersistentContext(profile, { channel: "chrome" });
  try {
    await ctx.addInitScript(`delete FileSystemFileHandle.prototype.createWritable;`);
    const page = ctx.pages()[0] ?? (await ctx.newPage());
    await openApp(page, base, "");
    const r: Result = { name: "chrome", notes: [] };
    if (!(await storeDocument(page, r))) throw new Error("storing failed");
    await openApp(page, base, `&file=/uploads/${DOC_NAME}`);
    await waitFor(page, `Module._sumatra_test_page() === 1`, "the document");
  } finally {
    await ctx.close();
    rmSync(profile, { recursive: true, force: true });
  }
}

export async function testit(only = ""): Promise<void> {
  const build = spawnSync("bun", ["cmd/ng-build.ts", "-wasm", "-rel", "SumatraPDF"], { cwd: ROOT, stdio: "inherit" });
  if (build.status !== 0) throw new Error("ad-hoc-wasm-browsers: wasm build failed");
  const server = Bun.serve({
    port: 0,
    async fetch(req) {
      const path = new URL(req.url).pathname;
      const f = Bun.file(join(OUT_DIR, path === "/" ? "SumatraPDF.html" : path));
      return (await f.exists()) ? new Response(f) : new Response("not found", { status: 404 });
    },
  });
  const base = `http://localhost:${server.port}/`;
  const browsers: [string, BrowserType][] = [
    ["chrome", chromium],
    ["firefox", firefox],
    ["webkit", webkit],
  ];
  try {
    for (const [name, type] of browsers) {
      if (only && only !== name) continue;
      if (name !== "chrome" && !existsSync(type.executablePath())) {
        console.log(`ad-hoc-wasm-browsers: ${name}: not installed (bunx playwright install ${name}), skipping`);
        continue;
      }
      const r = await runBrowser(name, type, base);
      console.log(`ad-hoc-wasm-browsers: ok: ${r.notes.join(", ")}`);
    }
    if (!only || only === "chrome") {
      await testWorkerWriter(base);
      console.log("ad-hoc-wasm-browsers: ok: without createWritable, documents are written from a worker");
    }
  } finally {
    server.stop(true);
  }
}

if (import.meta.main) {
  await testit(process.argv[2] ?? "");
  console.log("✅ ad-hoc-wasm-browsers passed");
  process.exit(0);
}
