// #6503: KiCad schematic PDFs put the menu items in the link's call arguments,
// ShM([["Reference = U1"], ["Datasheet = <url>", "<url>"], ...]), and a
// document-level function ShM() passes them to app.popUpMenuEx. The #1198
// parser only looked for popUpMenu itself, so those links weren't clickable.
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, runStandalone, tmpPath } from "./util";

function pdfString(s: string): string {
  let out = "(";
  for (const ch of s) {
    if (ch === "\\" || ch === "(" || ch === ")") {
      out += "\\";
    }
    out += ch;
  }
  return out + ")";
}

// a shortened version of KiCad's JSInit
const SHM_JS =
  "function ShM(aEntries) { var aParams = []; for (var i = 0; i < aEntries.length; ++i) { " +
  "aParams.push({cName: aEntries[i][0], cReturn: aEntries[i].length > 1 ? aEntries[i][1] : ''}) } " +
  "var cChoice = app.popUpMenuEx.apply(app, aParams); " +
  "if (cChoice.substring(0, 1) == '#') { this.pageNum = parseInt(cChoice.slice(1)); return; } " +
  "app.launchURL(cChoice); }";

const MENU_JS =
  'ShM([\n["Reference = U1"],\n["Datasheet = https://example.com/ds.pdf", "https://example.com/ds.pdf"],\n' +
  '["Net = /Connectors/A1", "file:///Connectors/A1"],\n["Sheet = Power", "#1"],\n]);';
// same shape, but Other() isn't a document function that shows a menu
const NOT_MENU_JS = 'Other([["Reference = U2"]]);';

function makePdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R /Names 8 0 R >>");
  body[2] = enc("<< /Type /Pages /Kids [3 0 R 11 0 R] /Count 2 >>");
  body[6] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
  const stream = "BT /F1 12 Tf 72 710 Td (U1) Tj 0 -100 Td (U2) Tj ET";
  body[7] = enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);

  body[4] = enc(
    `<< /Type /Annot /Subtype /Link /Rect [72 700 172 740] /Border [0 0 1] /A << /Type /Action /S /JavaScript /JS ${pdfString(MENU_JS)} >> >>`,
  );
  body[5] = enc(
    `<< /Type /Annot /Subtype /Link /Rect [72 600 172 640] /Border [0 0 1] /A << /Type /Action /S /JavaScript /JS ${pdfString(NOT_MENU_JS)} >> >>`,
  );
  body[3] = enc(
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R 5 0 R] ` +
      `/Resources << /Font << /F1 6 0 R >> >> /Contents 7 0 R >>`,
  );
  body[11] = enc(
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 6 0 R >> >> /Contents 7 0 R >>`,
  );

  // KiCad names the script JSInit, not after the function it defines
  body[8] = enc("<< /JavaScript 9 0 R >>");
  body[9] = enc(`<< /Names [ (JSInit) 10 0 R ] >>`);
  body[10] = enc(`<< /Type /Action /S /JavaScript /JS ${pdfString(SHM_JS)} >>`);

  const maxN = 11;
  const parts: Buffer[] = [enc("%PDF-1.5\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0]!.length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n] ?? enc("null"), enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let n = 1; n <= maxN; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6503.pdf");
  writeFileSync(pdf, makePdf());

  await withControlledSumatra(EXE, async (client: ControlClient) => {
    const res = await client.request(ControlCommand.TestPageLinks, [pdf, 1]);
    const dump = String(res[1] ?? "");
    if (res[0] !== 0) {
      throw new Error(`issue-6503: TestPageLinks failed:\n${dump}`);
    }

    const menus = dump.split("\n").filter((l) => l.startsWith("kind=jsMenu "));
    if (menus.length !== 1) {
      throw new Error(`issue-6503: expected 1 jsMenu link, got ${menus.length}:\n${dump}`);
    }
    const menu = menus[0]!;
    for (const item of ["Reference = U1", "Datasheet = https://example.com/ds.pdf", "Net = /Connectors/A1"]) {
      if (!menu.includes(item)) {
        throw new Error(`issue-6503: menu should list '${item}':\n${menu}`);
      }
    }
    if (menu.includes("U2")) {
      throw new Error(`issue-6503: a call to a function that shows no menu became a menu:\n${dump}`);
    }

    // Datasheet opens its URL, "#1" (0-based pageNum) goes to page 2, file:/// names a net and does nothing
    const want = "actions=-;url:https://example.com/ds.pdf;-;page:2";
    if (!menu.includes(want)) {
      throw new Error(`issue-6503: expected '${want}':\n${menu}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
