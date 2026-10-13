// The barcode tool and OCR output of the mupdf tools (#5446): a QR code
// written by `barcode -c` reads back, and `draw -F ocr.txt` recognizes the
// text of a rendered page. OCR needs tesseract's eng.traineddata, fetched
// once into .work/tessdata; without a network the OCR half is skipped.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT, makeTextPdf, runStandalone, tmpPath } from "./util.ts";

const TESSDATA_DIR = join(ROOT, ".work", "tessdata");
const TESSDATA_URL = "https://github.com/tesseract-ocr/tessdata_fast/raw/main/eng.traineddata";

function run(args: string[]): string {
  const proc = Bun.spawnSync([EXE, ...args], { cwd: ROOT });
  if (proc.exitCode !== 0) {
    throw new Error(`issue-5446: ${args.join(" ")} failed (exit ${proc.exitCode}):\n${proc.stdout}\n${proc.stderr}`);
  }
  return proc.stdout.toString();
}

async function ensureTessdata(): Promise<boolean> {
  const path = join(TESSDATA_DIR, "eng.traineddata");
  if (existsSync(path)) {
    return true;
  }
  try {
    const res = await fetch(TESSDATA_URL);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    mkdirSync(TESSDATA_DIR, { recursive: true });
    writeFileSync(path, new Uint8Array(await res.arrayBuffer()));
    return true;
  } catch (e) {
    console.log(`issue-5446: can't download ${TESSDATA_URL} (${e}); skipping the OCR check`);
    return false;
  }
}

export async function testit(): Promise<void> {
  const text = "Hello SumatraPDF 5446";
  const qr = tmpPath("issue-5446-qr.png");
  run(["barcode", "-c", "-e", "4", "-q", "-o", qr, text]);
  const decoded = run(["barcode", "-d", qr]).trim();
  if (decoded !== `qrcode: ${text}`) {
    throw new Error(`issue-5446: decoded '${decoded}', expected 'qrcode: ${text}'`);
  }

  if (!(await ensureTessdata())) {
    return;
  }
  const pdf = tmpPath("issue-5446.pdf");
  writeFileSync(pdf, makeTextPdf("Hello OCR world"));
  const page = tmpPath("issue-5446-page.png");
  run(["draw", "-r", "150", "-o", page, pdf, "1"]);
  const ocr = tmpPath("issue-5446-page.ocr.txt");
  run(["draw", "-F", "ocr.txt", "-d", TESSDATA_DIR, "-o", ocr, page]);
  const got = readFileSync(ocr, "utf8").replace(/\s+/g, " ").trim();
  if (got !== "Hello OCR world") {
    throw new Error(`issue-5446: OCR read '${got}', the page says 'Hello OCR world'`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
