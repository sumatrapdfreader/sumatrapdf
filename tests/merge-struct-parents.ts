// Merge PDF on a tagged PDF: the saved document's structure ParentTree must
// still map each page's /StructParents to its structure elements.
//
// Run: bun tests/merge-struct-parents.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, EXE, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const kPages = 2;

// each page has one marked paragraph, found through /StructTreeRoot /ParentTree
function taggedPdf(): string {
  const pageObj = (i: number) => 3 + i;
  const contentObj = (i: number) => 3 + kPages + i;
  const elemObj = (i: number) => 3 + 2 * kPages + i;
  const structRootObj = 3 + 3 * kPages;
  const content = "/P << /MCID 0 >> BDC BT /F1 24 Tf 72 100 Td (Hello) Tj ET EMC\n";

  const objs: string[] = [];
  objs.push(`<< /Type /Catalog /Pages 2 0 R /StructTreeRoot ${structRootObj} 0 R /MarkInfo << /Marked true >> >>`);
  const kids = Array.from({ length: kPages }, (_, i) => `${pageObj(i)} 0 R`).join(" ");
  objs.push(`<< /Type /Pages /Kids [${kids}] /Count ${kPages} >>`);
  for (let i = 0; i < kPages; i++) {
    const box = `[0 0 ${200 + i} 200]`;
    objs.push(`<< /Type /Page /Parent 2 0 R /MediaBox ${box} /Contents ${contentObj(i)} 0 R /StructParents ${i} >>`);
  }
  for (let i = 0; i < kPages; i++) {
    objs.push(`<< /Length ${content.length} >>\nstream\n${content}endstream`);
  }
  for (let i = 0; i < kPages; i++) {
    objs.push(`<< /Type /StructElem /S /P /P ${structRootObj} 0 R /Pg ${pageObj(i)} 0 R /K 0 >>`);
  }
  const elems = Array.from({ length: kPages }, (_, i) => `${elemObj(i)} 0 R`).join(" ");
  const nums = Array.from({ length: kPages }, (_, i) => `${i} [${elemObj(i)} 0 R]`).join(" ");
  // a direct dict: nothing else keeps it alive once the merge replaces it
  const parentTree = `<< /Nums [${nums}] >>`;
  objs.push(`<< /Type /StructTreeRoot /K [${elems}] /ParentTree ${parentTree} /ParentTreeNextKey ${kPages} >>`);

  let out = "%PDF-1.7\n";
  const offsets: number[] = [];
  objs.forEach((o, i) => {
    offsets.push(out.length);
    out += `${i + 1} 0 obj\n${o}\nendobj\n`;
  });
  const xref = out.length;
  out += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    out += `${String(off).padStart(10, "0")} 00000 n \n`;
  }
  out += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return out;
}

async function merge(client: ControlClient, action: string, arg = ""): Promise<string> {
  const res = await client.request(ControlCommand.TestMergePdf, [action, arg, 0]);
  return String(res[1] ?? "");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("merge-struct-parents");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "tagged.pdf");
  writeFileSync(pdf, taggedPdf(), "latin1");
  const merged = join(dir, "merged.pdf");

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdMergePDF"));

    const deadline = Date.now() + 5000;
    while (!(await merge(client, "")).includes("items=0:1")) {
      if (Date.now() > deadline) {
        throw new Error("merge-struct-parents: the dialog didn't open");
      }
      await sleep(100);
    }

    const res = await merge(client, "saveas", merged);
    if (res !== "OK saved=1") {
      throw new Error(`merge-struct-parents: saveas: ${res}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  const p = Bun.spawnSync([EXE, "show", merged, "trailer/Root/StructTreeRoot/ParentTree/Nums"]);
  const nums = p.stdout.toString().replace(/\s+/g, " ").trim();
  // e.g. "[ 0 [ 7 0 R ] 1 [ 8 0 R ] ]"; a lost parent is "null"
  if (!/^\[ ?0 \[ ?\d+ 0 R ?\] 1 \[ ?\d+ 0 R ?\] ?\]$/.test(nums)) {
    throw new Error(`merge-struct-parents: want both pages' parents in ParentTree /Nums, got '${nums}'`);
  }
  console.log("merge-struct-parents: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
