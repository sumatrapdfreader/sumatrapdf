// Ad-hoc test for the UI Automation provider (issue #321, "not accessible to
// screen readers").
//
// Narrator / NVDA read a document through UIA: they look for the Document
// element in the *control* / *content* view, get its TextPattern, and then walk
// a text range with ExpandToEnclosingUnit() + Move() to read character by
// character, word by word, line by line. This test does exactly that from a
// real UIA client (tests/ad-hoc-uia.cpp, compiled on demand with cl.exe) and
// checks that:
//   - the Document element is in the control and content views
//   - its TextPattern returns the document text
//   - walking by character / word / line advances instead of repeating
//   - reading the whole document line by line terminates
//
// It is ad-hoc (not in tests/run-almost-all.ts) because it needs the Visual Studio
// command-line tools to build the client; without cl.exe it skips.

import { existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT, tmpPath } from "./util";
import {
  moveWindow,
  showWindow,
  sleep,
  waitForChildWindow,
  waitForTopWindow,
  SW_RESTORE,
  killAndWait,
  killProcessesNamed,
} from "./winapi";

const PAGE_LINES = [
  ["Alpha line one", "Bravo line two", "Charlie line three", "Delta line four"],
  ["Echo line five", "Foxtrot line six", "Golf line seven", "Hotel line eight"],
];

// a PDF with the given lines of real text, one page per group
function makeTextPdf(pages: string[][]): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const objs: Buffer[] = [];
  const add = (s: string) => {
    objs.push(enc(s));
    return objs.length; // 1-based object number
  };
  add("<< /Type /Catalog /Pages 2 0 R >>"); // 1
  add("PAGES_PLACEHOLDER"); // 2, patched below
  const fontNo = add("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");

  const pageNos: number[] = [];
  for (const lines of pages) {
    let stream = "BT /F1 18 Tf 72 700 Td 24 TL\n";
    for (const line of lines) {
      stream += `(${line}) Tj T*\n`;
    }
    stream += "ET";
    const contentNo = add(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);
    pageNos.push(
      add(
        `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
          `/Resources << /Font << /F1 ${fontNo} 0 R >> >> /Contents ${contentNo} 0 R >>`,
      ),
    );
  }
  const kids = pageNos.map((n) => `${n} 0 R`).join(" ");
  objs[1] = enc(`<< /Type /Pages /Kids [${kids}] /Count ${pageNos.length} >>`);

  const parts: Buffer[] = [enc("%PDF-1.7\n")];
  const offsets: number[] = [];
  let pos = parts[0]!.length;
  objs.forEach((body, i) => {
    offsets.push(pos);
    const obj = Buffer.concat([enc(`${i + 1} 0 obj\n`), body, enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  });
  let xref = `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    xref += `${String(off).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

// compile the UIA client; null when the VS command-line tools aren't available
function buildClient(): string | null {
  const src = join(ROOT, "tests", "ad-hoc-uia.cpp");
  const exe = tmpPath("ad-hoc-uia-client.exe");
  const which = Bun.spawnSync(["where", "cl.exe"]);
  if (which.exitCode !== 0) {
    return null;
  }
  const r = Bun.spawnSync(
    [
      "cl.exe",
      "/nologo",
      "/EHsc",
      "/std:c++17",
      src,
      `/Fe${exe}`,
      `/Fo${tmpPath("ad-hoc-uia-client.obj")}`,
      "/link",
      "ole32.lib",
      "oleaut32.lib",
      "user32.lib",
      "uiautomationcore.lib",
    ],
    { cwd: tmpPath("") },
  );
  if (!existsSync(exe)) {
    throw new Error(`could not build the UIA client:\n${r.stdout.toString()}\n${r.stderr.toString()}`);
  }
  return exe;
}

function parseKeyValues(out: string): Map<string, string> {
  const res = new Map<string, string>();
  for (const line of out.split(/\r?\n/)) {
    const i = line.indexOf("=");
    if (i > 0) {
      res.set(
        line.slice(0, i),
        line
          .slice(i + 1)
          .replace(/\\n/g, "\n")
          .replace(/\\p/g, "|"),
      );
    }
  }
  return res;
}

const SETTINGS = [`RestoreSession = false`, `ShowStartPage = false`, `CheckForUpdates = false`, ``].join("\n");

export async function testit(): Promise<void> {
  const client = buildClient();
  if (!client) {
    console.log("  cl.exe not in PATH, skipping the UI Automation test");
    return;
  }

  const pdf = tmpPath("ad-hoc-uia.pdf");
  writeFileSync(pdf, makeTextPdf(PAGE_LINES));
  const appdata = tmpPath("ad-hoc-uia-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  await killProcessesNamed("SumatraPDF.exe");
  const proc = Bun.spawn([EXE, "-for-testing", "-appdata", appdata, pdf], { stdout: "ignore", stderr: "ignore" });
  try {
    const frame = await waitForTopWindow(proc.pid, "SUMATRA_PDF_FRAME");
    if (!frame) {
      throw new Error("SumatraPDF main window did not appear");
    }
    showWindow(frame, SW_RESTORE);
    moveWindow(frame, 60, 60, 1100, 850);
    const canvas = await waitForChildWindow(frame, "SUMATRA_PDF_CANVAS");
    if (!canvas) {
      throw new Error("could not find the canvas window");
    }
    await sleep(1500);

    const r = Bun.spawnSync([client, String(canvas)]);
    const out = r.stdout.toString() + r.stderr.toString();
    const kv = parseKeyValues(out);
    const need = (k: string) => {
      const v = kv.get(k);
      if (v === undefined) {
        throw new Error(`UIA client did not report ${k}:\n${out}`);
      }
      return v;
    };

    if (need("doc.found") !== "1") {
      throw new Error(`no Document element under the canvas:\n${out}`);
    }
    if (need("doc.hasTextPattern") !== "1") {
      throw new Error(`the Document element has no TextPattern:\n${out}`);
    }
    // a Document outside the control / content views is invisible to screen
    // readers even though it holds all the text
    if (need("doc.isControlElement") !== "1" || need("doc.isContentElement") !== "1") {
      throw new Error(
        `Document element is not in the control/content view ` +
          `(isControlElement=${kv.get("doc.isControlElement")}, isContentElement=${kv.get("doc.isContentElement")})`,
      );
    }
    if (need("canvas.kbFocusable") !== "1") {
      throw new Error("the canvas does not report itself as keyboard focusable");
    }

    const firstLine = PAGE_LINES[0]![0]!;
    if (!need("doc.text").startsWith(firstLine)) {
      throw new Error(`document text does not start with '${firstLine}': '${kv.get("doc.text")}'`);
    }

    // each Move() must land on new text, or a screen reader repeats one unit
    // forever (that was the bug: Move() reported success without moving)
    for (const [key, steps] of [
      ["walk.char", 4],
      ["walk.word", 3],
      ["walk.line", 3],
    ] as const) {
      const seen: string[] = [];
      for (let i = 0; i <= steps; i++) {
        seen.push(need(`${key}${i}`));
      }
      const distinct = new Set(seen).size;
      if (distinct <= 1) {
        throw new Error(`walking by ${key} never advanced: ${JSON.stringify(seen)}`);
      }
      if (distinct < seen.length) {
        throw new Error(`walking by ${key} repeated itself: ${JSON.stringify(seen)}`);
      }
    }
    // the second page must be reachable by page
    if (!need("walk.page1").startsWith(PAGE_LINES[1]![0]!)) {
      throw new Error(`moving one page did not land on page 2: '${kv.get("walk.page1")}'`);
    }

    // Reading what is under the mouse / finger (Narrator's mouse mode, touch
    // exploration) needs two things: GetBoundingRectangles to say where the
    // text is on screen, and RangeFromPoint to say what text is at a point.
    // Both used to be E_NOTIMPL.
    if (need("point.haveRect") !== "1") {
      throw new Error(
        `no on-screen rectangle for the first word: GetBoundingRectangles gives screen readers ` +
          `nowhere to point (Narrator can't highlight what it reads)`,
      );
    }
    if (need("point.hr") !== "0x00000000") {
      throw new Error(`RangeFromPoint failed with ${kv.get("point.hr")}: reading text under the mouse can't work`);
    }
    const want = need("point.word").trim();
    const got = need("point.wordAtPoint").trim();
    if (!got.includes(want) && !want.includes(got)) {
      throw new Error(`RangeFromPoint over the word '${want}' returned '${got}'`);
    }

    if (need("sayall.terminated") !== "1") {
      throw new Error("reading the document line by line never reached the end");
    }
    const nLines = parseInt(need("sayall.lines"), 10);
    const totalLines = PAGE_LINES.reduce((n, p) => n + p.length, 0);
    if (nLines < totalLines - 1) {
      throw new Error(`read only ${nLines} lines, expected about ${totalLines}`);
    }
    console.log(`  UIA: document readable, ${nLines} lines walked, page/word/char navigation advances ✓`);
    console.log(`  UIA: word under the mouse: '${kv.get("point.word")}' -> '${kv.get("point.wordAtPoint")}' ✓`);
  } finally {
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
