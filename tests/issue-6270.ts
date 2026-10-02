// #6270: Book view. Clicking the right-hand page of a spread shows that
// spread, but the thumbnail highlight followed the row's first page.
// A second click was what made the clicked thumbnail stick.
//
// Run: bun tests/issue-6270.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, makePdf, pollUntil, runStandalone, tmpPath } from "./util.ts";
import {
  clientToScreen,
  MK_LBUTTON,
  packCoords,
  sendMessage,
  setCursorPos,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

type Rect = { x: number; y: number; dx: number; dy: number };

type Sidebar = {
  hwnd: number;
  thumbnails: boolean;
  current: number;
  rects: Map<number, Rect>;
  raw: string;
};

function parseRect(s: string): Rect {
  const [x, y, dx, dy] = s.split(",").map(Number);
  return { x: x!, y: y!, dx: dx!, dy: dy! };
}

async function sidebar(client: ControlClient): Promise<Sidebar> {
  const res = await client.request(ControlCommand.TestSidebarThumbnails, []);
  const raw = String(res[1] ?? "");
  const m = /hwnd=(\d+) thumbnails=(\d) count=\d+ current=(\d+).* rects=(\S*)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`issue-6270: TestSidebarThumbnails: ${raw}`);
  }
  const rects = new Map<number, Rect>();
  for (const part of m[4]!.split(";").filter(Boolean)) {
    const [page, coords] = part.split(":");
    rects.set(+page!, parseRect(coords!));
  }
  return { hwnd: +m[1]!, thumbnails: m[2] === "1", current: +m[3]!, rects, raw };
}

async function displayMode(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestDisplayMode, ["get"]);
  const raw = String(res[1] ?? "");
  const m = /mode=(.*) presentation=/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`issue-6270: TestDisplayMode: ${raw}`);
  }
  return m[1]!;
}

function click(s: Sidebar, page: number) {
  const r = s.rects.get(page);
  if (!r || r.dx <= 0) {
    throw new Error(`issue-6270: page ${page}'s thumbnail isn't visible: ${s.raw}`);
  }
  const pt = { x: Math.round(r.x + r.dx / 2), y: Math.round(r.y + r.dy / 2) };
  const screen = clientToScreen(s.hwnd, pt.x, pt.y);
  setCursorPos(screen.x, screen.y);
  const packed = packCoords(pt.x, pt.y);
  sendMessage(s.hwnd, WM_LBUTTONDOWN, MK_LBUTTON, packed);
  sendMessage(s.hwnd, WM_LBUTTONUP, 0, packed);
}

// The clicked thumbnail stays highlighted. Book view reports the row's first page.
async function clickHighlights(client: ControlClient, page: number) {
  const before = await sidebar(client);
  click(before, page);
  const after = await pollUntil(
    () => sidebar(client),
    (s) => s.current === page,
    {
      error: (s) => `issue-6270: click on page ${page} highlighted ${s.current}: ${s.raw}`,
    },
  );
  if (after.current !== page) {
    throw new Error(`issue-6270: click on page ${page} highlighted ${after.current}`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6270");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(5, 400, 0, 0), "latin1");

  const { proc, client, frame } = await launchControlled(["-view", "book view", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    const mode = await displayMode(client);
    if (!mode.includes("book")) {
      throw new Error(`issue-6270: not in book view: ${mode}`);
    }
    sendCommandSync(frame, cmdId("CmdToggleThumbnails"));
    await pollUntil(
      () => sidebar(client),
      (s) => s.thumbnails && (s.rects.get(3)?.dx ?? 0) > 0,
      { error: (s) => `issue-6270: thumbnails didn't show: ${s.raw}` },
    );

    // Book view rows are (1), (2, 3), (4, 5). Page 3 is the right-hand page.
    await clickHighlights(client, 3);
    // The left-hand page of a spread still highlights.
    await clickHighlights(client, 2);

    sendCommandSync(frame, cmdId("CmdFacingView"));
    await pollUntil(
      () => displayMode(client),
      (m) => m.includes("facing"),
      { error: (m) => `issue-6270: not in facing view: ${m}` },
    );
    // Facing rows are (1, 2), (3, 4), (5). Page 2 is the right-hand page.
    await clickHighlights(client, 2);

    console.log("issue-6270: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
