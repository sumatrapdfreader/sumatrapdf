// Crash rendering a form whose JavaScript fails (crashes 2026-10-10-01-02-32df
// and five more): mupdf printed the JS error to stderr, which a GUI app started
// from Explorer doesn't have. The failed write threw on the JS engine's
// fz_context, which has no fz_try on the render thread, and aborted the process.
//
// The app is started through `cmd /c start` so that, like from Explorer, it
// gets no standard handles; a directly spawned child always has a stderr.
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, uniquePipeName } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { testWindowPos } from "./winapi.ts";

const enc = (s: string) => Buffer.from(s, "latin1");

// a text field with no appearance stream and a format action that throws
function makePdf(): Buffer {
  const body: Record<number, Buffer> = {};
  body[1] = enc(
    "<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [4 0 R] /NeedAppearances true " +
      "/DA (/Helv 12 Tf 0 g) /DR << /Font << /Helv 5 0 R >> >> >> >>",
  );
  body[2] = enc("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
  body[3] = enc("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>");
  body[4] = enc(
    "<< /Type /Annot /Subtype /Widget /FT /Tx /T (total) /V (12) /Rect [72 700 272 730] /P 3 0 R " +
      "/DA (/Helv 12 Tf 0 g) /AA << /F << /S /JavaScript /JS (noSuchFunction\\(\\);) >> >> >>",
  );
  body[5] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");

  const maxN = 5;
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0]!.length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n]!, enc("\nendobj\n")]);
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
  const pdf = tmpPath("form-js-no-stderr.pdf");
  writeFileSync(pdf, makePdf());

  const pipeName = uniquePipeName();
  const p = testWindowPos();
  const winPos = `${p.dx}x${p.dy}@${p.x}x${p.y}`;
  const args = ["-for-testing", "-window-pos", winPos, "-dbg-control", pipeName, pdf];
  Bun.spawn(["cmd.exe", "/c", "start", "", EXE, ...args], { stdout: "ignore", stderr: "ignore" });

  // the control pipe opens after the document loads, so a crash on the first
  // paint shows up as a failed connect
  let client: ControlClient;
  try {
    client = await ControlClient.connect(pipeName);
    await client.waitForRenderIdle();
    await client.request(ControlCommand.Ping);
  } catch (e) {
    throw new Error(`form-js-no-stderr: app died rendering a form with failing JavaScript: ${e}`);
  }
  await client.quit();
  client.close();
}

if (import.meta.main) {
  await runStandalone(testit);
}
