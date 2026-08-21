// Tests for https://github.com/sumatrapdfreader/sumatrapdf/issues/5792
// and https://github.com/sumatrapdfreader/sumatrapdf/issues/5793
//
// #5792: mismatched FB2/XML closers (e.g. <b><i>x</b></i>) used to abort the
// MuPDF XML parse and produce giant fonts / huge page counts. Lenient close_tag
// recovery keeps normal layout — title text and body still extract.
//
// #5793: reflow FB2 copy used one hard newline per visual line. Soft-join in
// FzTextPageToUtf8 merges wraps within a paragraph with a space, while keeping
// newlines between paragraphs. Ctrl+A + Ctrl+C uses rectangular selection →
// GetTextInRegion (not only -extract-text), which must keep soft-join spaces
// even though they have empty glyph rects.
//
// Fixture: tests/issue-5792.fb2 (mismatched title tags + multi-line paragraphs).
// Run:  bun tests/issue-5792.ts [--no-build]

import { existsSync } from "node:fs";
import { join } from "node:path";
import { cmdId, EXE, extractPageText, runStandalone } from "./util.ts";
import { launchControlled, sendCommandSync, killAndWait } from "./win-automation.ts";

const FB2 = join(import.meta.dir, "issue-5792.fb2");

function assertSoftJoinedParagraphs(text: string, label: string): void {
  if (!text.includes("Alpha paragraph") || !text.includes("Bravo paragraph")) {
    throw new Error(`#5793 ${label}: body missing:\n${JSON.stringify(text.slice(0, 400))}`);
  }
  const alphaIdx = text.indexOf("Alpha paragraph");
  const bravoIdx = text.indexOf("Bravo paragraph");
  const charlieIdx = text.indexOf("Charlie paragraph");
  if (bravoIdx <= alphaIdx || charlieIdx <= bravoIdx) {
    throw new Error(`#5793 ${label}: Alpha/Bravo/Charlie order wrong`);
  }
  const alphaBlock = text.slice(alphaIdx, bravoIdx).replace(/[\r\n]+$/, "");
  const bravoBlock = text.slice(bravoIdx, charlieIdx).replace(/[\r\n]+$/, "");
  if (/[\r\n]/.test(alphaBlock)) {
    throw new Error(`#5793 ${label}: Alpha still has hard line-breaks:\n${JSON.stringify(alphaBlock)}`);
  }
  if (/[\r\n]/.test(bravoBlock)) {
    throw new Error(`#5793 ${label}: Bravo still has hard line-breaks:\n${JSON.stringify(bravoBlock)}`);
  }
  // Soft-join spaces at wrap points (not "visuallines")
  if (!alphaBlock.includes("visual lines") || !alphaBlock.includes("hard newlines")) {
    throw new Error(`#5793 ${label}: missing soft-join spaces at wrap points:\n${JSON.stringify(alphaBlock)}`);
  }
  if (!bravoBlock.includes("paragraphs stay")) {
    throw new Error(`#5793 ${label}: missing soft-join space in Bravo:\n${JSON.stringify(bravoBlock)}`);
  }
  // Paragraph boundaries kept as newlines
  if (!/[\r\n]+\s*Bravo paragraph/.test(text) || !/[\r\n]+\s*Charlie paragraph/.test(text)) {
    throw new Error(`#5793 ${label}: expected hard newlines between paragraphs:\n${JSON.stringify(text)}`);
  }
}

async function copyAllViaUi(): Promise<string> {
  const { proc, client, frame } = await launchControlled([FB2]);
  try {
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId("CmdSelectAll"));
    sendCommandSync(frame, cmdId("CmdCopySelection"));
    const clip = Bun.spawnSync(["powershell", "-NoProfile", "-Command", "Get-Clipboard -Raw"], {
      stdout: "pipe",
      stderr: "pipe",
    });
    return clip.stdout.toString();
  } finally {
    client.close();
    try {
      await killAndWait(proc);
    } catch {
      // already exited
    }
  }
}

export async function testit(): Promise<void> {
  if (!existsSync(FB2)) {
    throw new Error(`fixture missing: ${FB2}`);
  }
  if (!existsSync(EXE)) {
    throw new Error(`exe missing: ${EXE}`);
  }

  const text = extractPageText(FB2, 1);
  console.log(`page 1 extract len=${text.length} newlines=${(text.match(/\n/g) || []).length}`);
  if (!text) {
    throw new Error("#5792: empty extract for fixture FB2");
  }

  // #5792: mismatched <b><i>…</b></i> must still yield the title (parse recovery)
  if (!text.includes("Chapter One")) {
    throw new Error(`#5792: missing title after mismatched tags:\n${JSON.stringify(text.slice(0, 400))}`);
  }

  assertSoftJoinedParagraphs(text, "extract-text");

  // #5792 sanity: short sample is one page — page 2 must be empty / fail extract
  let page2 = "";
  try {
    page2 = extractPageText(FB2, 2);
  } catch {
    // no "text on page 2" is fine for a one-page fixture
  }
  if (page2.includes("Alpha paragraph")) {
    throw new Error("#5792: body repeated on page 2 (unexpected giant-font pagination?)");
  }

  // #5793: real copy path (Ctrl+A rectangular selection → GetTextInRegion → clipboard)
  const copied = await copyAllViaUi();
  console.log(`clipboard len=${copied.length}`);
  if (!copied.includes("Chapter One")) {
    throw new Error(`#5793 clipboard: missing title:\n${JSON.stringify(copied.slice(0, 400))}`);
  }
  assertSoftJoinedParagraphs(copied, "clipboard");

  console.log("issue-5792/5793 ok");
}

if (import.meta.main) {
  await runStandalone(testit);
}
