// #5982: Find Next/Previous from the main window must keep the unfocused
// floating Find window's selected result in sync with the document.
import { writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { getClientRect, getFocusedHwnd } from "./winapi.ts";

const query = "needle";
const matchPages = [2, 4, 6];
const pageCount = 6;

function buildPdf(): Buffer {
  const objects: string[] = [];
  const fontObject = 3;
  objects[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objects[fontObject] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";

  const pages: number[] = [];
  let objectNumber = 4;
  for (let page = 1; page <= pageCount; page++) {
    const pageObject = objectNumber++;
    const contentObject = objectNumber++;
    pages.push(pageObject);
    const text = matchPages.includes(page) ? `Page ${page}: ${query}` : `Page ${page}: no match`;
    const content = `BT /F1 24 Tf 72 720 Td (${text}) Tj ET`;
    objects[pageObject] =
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
      `/Resources << /Font << /F1 ${fontObject} 0 R >> >> /Contents ${contentObject} 0 R >>`;
    objects[contentObject] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
  }
  objects[2] = `<< /Type /Pages /Kids [${pages.map((page) => `${page} 0 R`).join(" ")}] /Count ${pages.length} >>`;

  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  for (let i = 1; i < objectNumber; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i} 0 obj\n${objects[i]}\nendobj\n`;
  }
  const xrefOffset = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objectNumber}\n0000000000 65535 f \n`;
  for (const offset of offsets) {
    pdf += `${offset.toString().padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objectNumber} /Root 1 0 R >>\nstartxref\n${xrefOffset}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

type FindState = { selected: number; pages: number[]; raw: string };

async function findState(client: ControlClient): Promise<FindState | null> {
  const response = await client.request(ControlCommand.TestFindResultsOrder, [query, matchPages[0]]);
  const exitCode = Number(response[0]);
  const raw = String(response[1] ?? "").trim();
  if (raw.includes("NOTREADY")) {
    return null;
  }
  if (exitCode !== 0) {
    throw new Error(`issue-5982: ${raw}`);
  }
  const match = /sel=(-?\d+) pages=([\d,]*)/.exec(raw);
  if (!match) {
    throw new Error(`issue-5982: can't parse Find state: ${raw}`);
  }
  const pages = match[2] ? match[2].split(",").map(Number) : [];
  return { selected: Number(match[1]), pages, raw };
}

async function waitForSelection(client: ControlClient, expected: number, timeoutMs = 8000): Promise<FindState> {
  const deadline = Date.now() + timeoutMs;
  let state: FindState | null = null;
  while (Date.now() < deadline) {
    state = await findState(client);
    if (state?.selected === expected) {
      return state;
    }
    await Bun.sleep(50);
  }
  throw new Error(`issue-5982: selected result is ${state?.selected ?? "not ready"}, want ${expected}`);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5982.pdf");
  writeFileSync(pdf, buildPdf());

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    let state = await waitForSelection(client, 0, 20_000);
    if (state.pages.join(",") !== matchPages.join(",")) {
      throw new Error(`issue-5982: result pages are ${state.pages.join(",")}, want ${matchPages.join(",")}`);
    }

    const canvas = findCanvas(frame);
    const canvasRect = getClientRect(canvas);
    await clickAt(canvas, Math.floor(canvasRect.dx / 2), Math.floor(canvasRect.dy / 2), 50);
    if (getFocusedHwnd(frame) !== frame) {
      throw new Error("issue-5982: document frame did not receive focus");
    }

    sendCommand(frame, cmdId("CmdFindNext"));
    state = await waitForSelection(client, 1);

    sendCommand(frame, cmdId("CmdFindPrev"));
    state = await waitForSelection(client, 0);
    console.log(`issue-5982: ${state.raw}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
