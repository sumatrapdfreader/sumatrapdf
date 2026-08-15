// The canvas context menu's Document submenu offered the PDF-only commands
// (Show PDF Info, Encrypt PDF, Compress PDF, Extract/Delete Pages From PDF, ...)
// for every document the mupdf engine renders, so an epub got the whole list.
// The gate used CouldBePDFDoc(), which is true for epub/mobi/fb2/xps/svg too;
// it now asks the engine whether there is really a pdf_document behind the tab.

import { findCanvas, findSubMenu, launchControlled, readContextMenuTree, killAndWait } from "./win-automation.ts";
import { setForegroundWindow } from "./winapi.ts";
import { runStandalone } from "./util.ts";

const kDocumentSubMenu = "Document";

async function documentMenuItems(file: string): Promise<string[]> {
  const { proc, client, frame } = await launchControlled([file]);
  try {
    await client.waitForRenderIdle();
    setForegroundWindow(frame);
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error(`pdf-only-menu-items: no canvas for ${file}`);
    }
    const tree = await readContextMenuTree(canvas);
    if (tree.length === 0) {
      throw new Error(`pdf-only-menu-items: no context menu for ${file}`);
    }
    const doc = findSubMenu(tree, kDocumentSubMenu);
    if (!doc || !doc.items) {
      throw new Error(`pdf-only-menu-items: no '${kDocumentSubMenu}' submenu for ${file}`);
    }
    return doc.items.map((it) => it.text);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const epub = await documentMenuItems("tests/issue-5846.epub");
  const pdfOnEpub = epub.filter((s) => s.includes("PDF"));
  if (pdfOnEpub.length > 0) {
    throw new Error(`pdf-only-menu-items: epub Document menu offers PDF commands: ${JSON.stringify(pdfOnEpub)}`);
  }

  // the other half: don't hide them from actual PDFs
  const pdf = await documentMenuItems("tests/issue-1189.pdf");
  const pdfOnPdf = pdf.filter((s) => s.includes("PDF"));
  if (pdfOnPdf.length === 0) {
    throw new Error(`pdf-only-menu-items: pdf Document menu lost its PDF commands: ${JSON.stringify(pdf)}`);
  }

  console.log(`pdf-only-menu-items: epub ${JSON.stringify(epub)}; pdf keeps ${pdfOnPdf.length} PDF commands`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
