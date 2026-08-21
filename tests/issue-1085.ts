// #1085 / #5945: following an internal destination flashes a highlight at
// the landing spot so a bibliography / figure dest is visible. Page-level
// /Fit dests (no position) do not highlight. Off by default;
// HighlightLinkDestination = true turns it on.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
RememberStatePerDocument = false
`;

function makePdf(): Buffer {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R /Outlines 5 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Outlines /First 6 0 R /Last 7 0 R /Count 2 >>`,
    `<< /Title (XYZ dest) /Parent 5 0 R /Next 7 0 R /Dest [4 0 R /XYZ 72 700 null] >>`,
    `<< /Title (Fit page) /Parent 5 0 R /Prev 6 0 R /Dest [4 0 R /Fit] >>`,
  ];
  let pdf = "%PDF-1.7\n%\xe2\xe3\xcf\xd3\n";
  const off: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    off[i] = Buffer.byteLength(pdf, "latin1");
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xref = Buffer.byteLength(pdf, "latin1");
  const n = objs.length + 1;
  pdf += `xref\n0 ${n}\n0000000000 65535 f \n`;
  for (let i = 0; i < objs.length; i++) {
    pdf += off[i]!.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${n} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

type Mark = { show: boolean; page: number; nrects: number; enabled: boolean; raw: string };

async function highlight(client: ControlClient): Promise<Mark> {
  const deadline = Date.now() + 8000;
  let last = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestLinkDestHighlight, []);
    const raw = String(res[1] ?? "").trim();
    last = raw;
    if (res[0] === 0) {
      const m = /show=(\d+) page=(\d+) nrects=(\d+) enabled=(\d)/.exec(raw);
      if (!m) {
        throw new Error(`issue-1085: could not parse: ${raw}`);
      }
      return { show: m[1] === "1", page: +m[2]!, nrects: +m[3]!, enabled: m[4] === "1", raw };
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-1085: highlight never ready (last: ${last})`);
    }
    await new Promise((r) => setTimeout(r, 50));
  }
}

async function followDest(client: ControlClient, destNo: number): Promise<void> {
  const deadline = Date.now() + 15_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestDestZoomNav, [destNo, 0]);
    const raw = String(res[1] ?? "").trim();
    if (res[0] === 0 && raw.startsWith("OK")) {
      return;
    }
    if (res[0] !== 0 && !raw.includes("NOTREADY")) {
      throw new Error(`issue-1085: follow dest ${destNo}: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-1085: follow dest ${destNo} timed out: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 50));
  }
}

async function run(highlightOn: boolean, destNo: number): Promise<Mark> {
  const dir = tmpPath(`issue-1085-${highlightOn ? "on" : "off"}-d${destNo}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), `${SETTINGS}HighlightLinkDestination = ${highlightOn}\n`);
  const pdf = join(dir, "dests.pdf");
  writeFileSync(pdf, makePdf());
  return withControlledSumatra(
    EXE,
    async (client) => {
      await followDest(client, destNo);
      return highlight(client);
    },
    ["-appdata", dir, pdf],
  );
}

export async function testit(): Promise<void> {
  const xyz = await run(true, 1);
  if (!xyz.enabled) {
    throw new Error(`issue-1085: setting should be on: ${xyz.raw}`);
  }
  if (!xyz.show || xyz.nrects < 1 || xyz.page !== 2) {
    throw new Error(`issue-1085: XYZ dest should flash a highlight on page 2: ${xyz.raw}`);
  }

  const fit = await run(true, 2);
  if (fit.show || fit.nrects !== 0) {
    throw new Error(`issue-1085: page-level /Fit dest should not highlight: ${fit.raw}`);
  }

  const off = await run(false, 1);
  if (off.enabled) {
    throw new Error(`issue-1085: setting should be off: ${off.raw}`);
  }
  if (off.show || off.nrects !== 0) {
    throw new Error(`issue-1085: HighlightLinkDestination = false still highlighted: ${off.raw}`);
  }
  console.log("issue-1085: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
