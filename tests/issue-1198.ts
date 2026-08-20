// #1198: Altium schematic PDFs put component metadata on /Link annots whose
// action is Acrobat JavaScript (app.popUpMenu, often via a named
// ShowCompProps_* function). Those were not clickable. The engine now parses
// the menu strings without running JS.
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

function makePdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  // JS source keeps "\\u00B5" so the PDF string decode yields \u00B5, which
  // the menu parser turns into U+00B5 MICRO SIGN.
  const namedFnJs =
    'function ShowCompProps_ABC123(){var sChoice = app.popUpMenu("Comment: Chip Capacitor","Value: 10\\u00B5F","Datasheet: https://example.com/cap","-","Library Name: ");}';
  const callJs = "ShowCompProps_ABC123();";
  const inlineJs = 'app.popUpMenu("Comment: inline part","https://example.org/ds");';
  const beepJs = "app.beep();";

  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R /Names 8 0 R >>");
  body[2] = enc("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
  body[6] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");

  const stream = "BT /F1 12 Tf 72 710 Td (U4) Tj 72 610 Td (R1) Tj 72 510 Td (https://example.net/x) Tj ET";
  body[7] = enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);

  // named ShowCompProps call, inline popUpMenu, a URI link, and JS that is not a menu
  body[4] = enc(
    `<< /Type /Annot /Subtype /Link /Rect [72 700 172 740] /Border [0 0 1] /A << /S /JavaScript /JS ${pdfString(callJs)} >> >>`,
  );
  body[5] = enc(
    `<< /Type /Annot /Subtype /Link /Rect [72 600 172 640] /Border [0 0 1] /A << /S /JavaScript /JS ${pdfString(inlineJs)} >> >>`,
  );
  body[11] = enc(
    `<< /Type /Annot /Subtype /Link /Rect [72 500 272 540] /Border [0 0 1] /A << /S /URI /URI (https://example.net/x) >> >>`,
  );
  body[12] = enc(
    `<< /Type /Annot /Subtype /Link /Rect [72 400 172 440] /Border [0 0 1] /A << /S /JavaScript /JS ${pdfString(beepJs)} >> >>`,
  );

  body[3] = enc(
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R 5 0 R 11 0 R 12 0 R] ` +
      `/Resources << /Font << /F1 6 0 R >> >> /Contents 7 0 R >>`,
  );

  body[8] = enc("<< /JavaScript 9 0 R >>");
  body[9] = enc(`<< /Names [ ${pdfString("ShowCompProps_ABC123")} 10 0 R ] >>`);
  body[10] = enc(`<< /S /JavaScript /JS ${pdfString(namedFnJs)} >>`);

  const maxN = 12;
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
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
  const pdf = tmpPath("issue-1198.pdf");
  writeFileSync(pdf, makePdf());

  await withControlledSumatra(EXE, async (client: ControlClient) => {
    const res = await client.request(ControlCommand.TestPageLinks, [pdf, 1]);
    const dump = String(res[1] ?? "");
    if (res[0] !== 0) {
      throw new Error(`issue-1198: TestPageLinks failed:\n${dump}`);
    }

    const rows = [...dump.matchAll(/^kind=(\S+) page=(-?\d+).* value=(.*)$/gm)].map((m) => ({
      kind: m[1]!,
      page: m[2]!,
      value: m[3]!,
    }));

    const menus = rows.filter((r) => r.kind === "jsMenu");
    if (menus.length !== 2) {
      throw new Error(`issue-1198: expected 2 jsMenu links, got ${menus.length}:\n${dump}`);
    }

    const named = menus.find((r) => r.value.includes("Comment: Chip Capacitor"));
    if (!named) {
      throw new Error(`issue-1198: missing named ShowCompProps menu:\n${dump}`);
    }
    if (!named.value.includes("Value: 10µF")) {
      throw new Error(`issue-1198: named menu should decode \\u00B5 to µ:\n${named.value}`);
    }
    if (!named.value.includes("Datasheet: https://example.com/cap")) {
      throw new Error(`issue-1198: named menu should keep the datasheet URL:\n${named.value}`);
    }
    if (!named.value.includes("Library Name:")) {
      throw new Error(`issue-1198: named menu should keep empty-value fields:\n${named.value}`);
    }
    if (named.value.split("|").some((s) => s.trim() === "-")) {
      throw new Error(`issue-1198: tooltip should skip "-" separators:\n${named.value}`);
    }

    const inline = menus.find((r) => r.value.includes("Comment: inline part"));
    if (!inline) {
      throw new Error(`issue-1198: missing inline popUpMenu link:\n${dump}`);
    }
    if (!inline.value.includes("https://example.org/ds")) {
      throw new Error(`issue-1198: inline menu should keep the URL:\n${inline.value}`);
    }

    const urls = rows.filter((r) => r.kind === "launchURL" && r.value.includes("https://example.net/x"));
    if (urls.length !== 1) {
      throw new Error(`issue-1198: expected the URI link to remain, got:\n${dump}`);
    }

    if (dump.includes("app.beep")) {
      throw new Error(`issue-1198: non-menu JavaScript should not become a link:\n${dump}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
