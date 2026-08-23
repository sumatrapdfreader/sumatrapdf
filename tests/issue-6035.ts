// #6035: clicking Find Next/Prev quickly must not drop the second click.
// CS_DBLCLKS turns a fast second press into WM_LBUTTONDBLCLK; that used to be
// ignored on onClick-only buttons until the double-click timeout elapsed.
//
// Run: bun tests/issue-6035.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { clickAt, killAndWait, launchControlled } from "./win-automation.ts";
import {
  enumWindows,
  getWindowPid,
  getWindowText,
  MK_LBUTTON,
  packCoords,
  sendMessage,
  sleep,
  WM_LBUTTONDBLCLK,
  WM_LBUTTONUP,
} from "./winapi.ts";

const query = "needle";
const matchPages = [1, 2, 3];
const pageCount = 3;

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
    const text = `Page ${page}: ${query}`;
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

type FindState = {
  selected: number;
  pages: number[];
  next: { x: number; y: number; dx: number; dy: number };
  raw: string;
};

function parseRect(s: string): { x: number; y: number; dx: number; dy: number } {
  const p = s.split(",").map((n) => parseInt(n, 10));
  return { x: p[0]!, y: p[1]!, dx: p[2]!, dy: p[3]! };
}

async function findState(client: ControlClient): Promise<FindState | null> {
  const response = await client.request(ControlCommand.TestFindResultsOrder, [query, matchPages[0]]);
  const exitCode = Number(response[0]);
  const raw = String(response[1] ?? "").trim();
  if (raw.includes("NOTREADY")) {
    return null;
  }
  if (exitCode !== 0) {
    throw new Error(`issue-6035: ${raw}`);
  }
  const match = /sel=(-?\d+) pages=([\d,]*) prev=[-\d,]+ next=(-?\d+,-?\d+,-?\d+,-?\d+)/.exec(raw);
  if (!match) {
    throw new Error(`issue-6035: can't parse Find state: ${raw}`);
  }
  const pages = match[2] ? match[2].split(",").map(Number) : [];
  return { selected: Number(match[1]), pages, next: parseRect(match[3]!), raw };
}

async function waitForState(client: ControlClient, timeoutMs = 20_000): Promise<FindState> {
  const deadline = Date.now() + timeoutMs;
  let state: FindState | null = null;
  while (Date.now() < deadline) {
    state = await findState(client);
    if (state && state.pages.length >= 3 && state.next.dx > 0) {
      return state;
    }
    await sleep(50);
  }
  throw new Error(`issue-6035: Find results not ready (${state?.raw ?? "null"})`);
}

async function waitForSel(client: ControlClient, expected: number, timeoutMs = 8000): Promise<FindState> {
  const deadline = Date.now() + timeoutMs;
  let state: FindState | null = null;
  while (Date.now() < deadline) {
    state = await findState(client);
    if (state?.selected === expected) {
      return state;
    }
    await sleep(40);
  }
  throw new Error(`issue-6035: selected is ${state?.selected ?? "not ready"}, want ${expected}`);
}

function findFindWindow(pid: number, frame: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (hwnd === frame || getWindowPid(hwnd) !== pid) {
      return true;
    }
    if (getWindowText(hwnd) === "Find") {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6035.pdf");
  writeFileSync(pdf, buildPdf());

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    let state = await waitForState(client);
    if (state.pages.join(",") !== matchPages.join(",")) {
      throw new Error(`issue-6035: result pages are ${state.pages.join(",")}, want ${matchPages.join(",")}`);
    }

    const findWin = findFindWindow(proc.pid!, frame);
    if (!findWin) {
      throw new Error("issue-6035: Find window not found");
    }

    const nx = state.next.x + Math.floor(state.next.dx / 2);
    const ny = state.next.y + Math.floor(state.next.dy / 2);
    await clickAt(findWin, nx, ny, 50);
    state = await waitForSel(client, 1);

    // second physical click, as Windows would send it (DBLCLK, not DOWN)
    const lp = packCoords(nx, ny);
    sendMessage(findWin, WM_LBUTTONDBLCLK, MK_LBUTTON, lp);
    sendMessage(findWin, WM_LBUTTONUP, 0, lp);
    state = await waitForSel(client, 2);
    console.log(`issue-6035: ${state.raw}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
