// Regression test for issue #1315: AZW4 / Kindle Print Replica is a PDF
// wrapped in a MOBI/PDB container. Open it as the embedded PDF, not as a
// reflowable MOBI ebook.
//
// The fixture is built here (tiny 1-page PDF inside a 2-record BOOKMOBI).

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { waitForFrame } from "./win-automation.ts";
import { EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";

function buildPdf(): Buffer {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R] /Count 1 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /CropBox [36 36 576 756] /Resources << >> >>`,
  ];
  let pdf = "%PDF-1.4\n%\xe2\xe3\xcf\xd3\n";
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

// Minimal Print Replica: PDB header, MOBI type 8, one text record starting %MOP.
function buildAzw4(pdf: Buffer): Buffer {
  const mop = Buffer.alloc(20);
  mop.write("%MOP", 0, 4, "ascii");
  mop.writeUInt32BE(1, 4); // table count
  mop.writeUInt32BE(1, 8); // section count
  mop.writeUInt32BE(20, 12); // first section offset
  mop.writeUInt32BE(pdf.length, 16);
  const rec1 = Buffer.concat([mop, pdf]);

  const palm = Buffer.alloc(16);
  palm.writeUInt16BE(1, 0); // no compression
  palm.writeUInt32BE(rec1.length, 4);
  palm.writeUInt16BE(1, 8); // text record count
  palm.writeUInt16BE(4096, 10);

  const mobi = Buffer.alloc(116);
  mobi.write("MOBI", 0, 4, "ascii");
  mobi.writeUInt32BE(116, 4);
  mobi.writeUInt32BE(8, 8); // Print Replica
  const rec0 = Buffer.concat([palm, mobi]);

  const numRecords = 2;
  const rec0Off = 78 + numRecords * 8;
  const rec1Off = rec0Off + rec0.length;

  const hdr = Buffer.alloc(78);
  hdr.write("issue-1315", 0, 10, "ascii");
  hdr.write("BOOKMOBI", 0x3c, 8, "ascii");
  hdr.writeUInt16BE(numRecords, 76);

  const recTable = Buffer.alloc(numRecords * 8);
  recTable.writeUInt32BE(rec0Off, 0);
  recTable.writeUIntBE(0, 5, 3);
  recTable.writeUInt32BE(rec1Off, 8);
  recTable.writeUIntBE(1, 13, 3);

  return Buffer.concat([hdr, recTable, rec0, rec1]);
}

async function queryBoxes(client: ControlClient, pageNo: number): Promise<string> {
  const deadline = Date.now() + 20_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const res = await client.request(ControlCommand.TestPageBoxes, [pageNo]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      return raw;
    }
    if (exitCode !== 2) {
      throw new Error(`issue-1315: TestPageBoxes failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-1315: document never became ready: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  const azw4Path = tmpPath("issue-1315.azw4");
  writeFileSync(azw4Path, buildAzw4(buildPdf()));

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      if (!frame) {
        throw new Error("issue-1315: no frame");
      }
      await client.waitForRenderIdle();

      const raw = await queryBoxes(client, 1);
      if (!/\bmedia=/.test(raw)) {
        throw new Error(`issue-1315: expected PDF page boxes, got: ${raw}`);
      }
      if (!/\bcrop=/.test(raw)) {
        throw new Error(`issue-1315: expected CropBox from the wrapped PDF, got: ${raw}`);
      }
      console.log(`issue-1315: ${raw}`);
    },
    [azw4Path],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
