// Polyline annotations from the PDF toolbar and Command Palette collect page
// vertices with a live preview. Double-click, right-click, Enter, and Space
// finish the path; off-page clicks and Esc cancel it.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { cmdId, runStandalone, tmpPath, USE_NG, makeBlankPdf, SLOW_BUILD_FACTOR } from "./util.ts";
import {
  captureWindowPixels,
  clientToScreen,
  getClientRect,
  MK_CONTROL,
  MK_LBUTTON,
  MK_RBUTTON,
  MK_SHIFT,
  packCoords,
  sendMessage,
  setCursorPos,
  sleep,
  VK_ESCAPE,
  VK_RETURN,
  VK_SPACE,
  WM_COMMAND,
  WM_LBUTTONDBLCLK,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
  WM_RBUTTONDOWN,
  WM_RBUTTONUP,
} from "./winapi.ts";
import {
  clickAt,
  findChildByClass,
  findCanvas,
  killAndWait,
  launchControlled,
  pressKey,
  sendCommand,
  countPreviewBlue,
  paletteRun,
  ngInput,
} from "./win-automation.ts";

type Point = { x: number; y: number };

type PlacementState = {
  active: boolean;
  notification: boolean;
  cursor: boolean;
  points: number;
  command: number;
  page: number;
  annotations: number;
  message: string;
  raw: string;
};

const notification =
  "Place polyline annotation. **Double-click**, **right-click**, **Space**, or **Enter** to finish, " +
  "**Ctrl+click** to close it. **Shift** to snap to multiples of 45 degrees. **Esc** to cancel.";

async function placementState(client: ControlClient): Promise<PlacementState> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const count = /annotations=(\d+)/.exec(raw);
  const state =
    /polyLinePlacement active=(\d+) notification=(\d+) cursor=(\d+) points=(\d+) cmd=(\d+) page=(-?\d+) end=[^ ]+ message=(.*)/.exec(
      raw,
    );
  if (res[0] !== 0 || !count || !state) {
    throw new Error(`polyline-annotation-placement: could not read state\n${raw}`);
  }
  return {
    active: state[1] === "1",
    notification: state[2] === "1",
    cursor: state[3] === "1",
    points: +state[4]!,
    command: +state[5]!,
    page: +state[6]!,
    annotations: +count[1]!,
    message: state[7]!,
    raw,
  };
}

const TOOLBAR_CLICK_TIMEOUT_MS = 5_000;
const TOOLBAR_CLICK_SETTLE_MS = 300;

async function waitForPlacement(client: ControlClient, active: boolean): Promise<PlacementState> {
  const deadline = Date.now() + 5_000;
  let state: PlacementState;
  for (;;) {
    state = await placementState(client);
    if (state.active === active) {
      return state;
    }
    if (Date.now() > deadline) {
      throw new Error(`polyline-annotation-placement: active did not become ${active}\n${state.raw}`);
    }
    await sleep(40);
  }
}

function placementEnd(raw: string): Point | null {
  const end = /polyLinePlacement [^\n]*end=(-?\d+),(-?\d+)/.exec(raw);
  if (!end) {
    return null;
  }
  return { x: +end[1]!, y: +end[2]! };
}

function toolbarButtonRect(dump: string): { x: number; y: number; dx: number; dy: number } {
  const id = cmdId("CmdCreateAnnotPolyLine");
  const re = new RegExp(`annotation-idx=\\d+ cmd=${id} hidden=0 enabled=1 rect=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)`);
  const m = re.exec(dump);
  if (!m) {
    throw new Error(`polyline-annotation-placement: toolbar button not found\n${dump}`);
  }
  const x = +m[1]!;
  const y = +m[2]!;
  return { x, y, dx: +m[3]! - x, dy: +m[4]! - y };
}

// ng's palette is a gpui field, not an Edit. Type through the control pipe.
function ngMods(key: number): number {
  let mods = 0;
  if (key & MK_SHIFT) {
    mods |= 2;
  }
  // On macOS bit 1 is Control, which is the context click. Command is bit 8.
  if (key & MK_CONTROL) {
    mods |= IS_MAC ? 8 : 1;
  }
  return mods;
}

function ngMouse(client: ControlClient, kind: string, point: Point, button: number, key: number): Promise<void> {
  return ngInput(client, [kind, point.x, point.y, button, ngMods(key)]);
}

async function moveMouse(client: ControlClient, canvas: number, point: Point, key = 0): Promise<void> {
  // ng: a posted move is hit-tested at the real cursor, which a locked
  // desktop keeps at 0,0, so the preview never follows the point.
  if (USE_NG) {
    await ngMouse(client, "move", point, 0, key);
    return;
  }
  const screen = clientToScreen(canvas, point.x, point.y);
  setCursorPos(screen.x, screen.y);
  sendMessage(canvas, WM_MOUSEMOVE, key, packCoords(point.x, point.y));
}

// Posted MK_SHIFT / MK_CONTROL never reach gpui; it reads its own modifiers.
async function clickCanvas(client: ControlClient, canvas: number, point: Point, key = 0): Promise<void> {
  if (USE_NG && key !== 0) {
    await ngMouse(client, "click", point, 0, key);
    return;
  }
  await clickAt(canvas, point.x, point.y, 0, key);
}

async function clickPoints(canvas: number, points: Point[]): Promise<void> {
  for (const point of points) {
    await clickAt(canvas, point.x, point.y, 0);
  }
}

async function expectFinished(client: ControlClient, annotations: number, gesture: string): Promise<PlacementState> {
  const state = await waitForPlacement(client, false);
  if (state.notification || state.annotations !== annotations || !state.raw.includes("type=PolyLine page=1")) {
    throw new Error(`polyline-annotation-placement: ${gesture} did not finish cleanly\n${state.raw}`);
  }
  return state;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("polyline-annotation-placement");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "blank.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makeBlankPdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
  );

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);

    sendMessage(frame, WM_COMMAND, cmdId("CmdToggleEditPDF"), 0);
    // The annotation row is visible before gpui writes button bounds.
    const buttonDeadline = Date.now() + 5_000;
    let button = { x: 0, y: 0, dx: 0, dy: 0 };
    let toolbarDump = "";
    for (;;) {
      toolbarDump = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
      try {
        button = toolbarButtonRect(toolbarDump);
      } catch {
        button = { x: 0, y: 0, dx: 0, dy: 0 };
      }
      if (button.dx > 0 && button.dy > 0) {
        break;
      }
      if (Date.now() > buttonDeadline) {
        throw new Error(`polyline-annotation-placement: toolbar button not laid out\n${toolbarDump}`);
      }
      await sleep(40);
    }
    const canvasRect = getClientRect(canvas);
    const center = { x: Math.floor(canvasRect.right / 2), y: Math.floor(canvasRect.bottom / 2) };
    const p1 = { x: center.x - 120, y: center.y - 90 };
    const p2 = { x: center.x + 105, y: center.y - 25 };
    const p3 = { x: center.x - 75, y: center.y + 105 };
    const p4 = { x: center.x + 60, y: center.y + 60 };
    const outside = { x: 2, y: center.y };
    const clickToolbar = async () => {
      const x = button.x + Math.floor(button.dx / 2);
      const y = button.y + Math.floor(button.dy / 2);
      // ng draws the toolbar in the frame. A posted down/up pair can be split
      // by a cursor snap, which gpui treats as a drag.
      if (USE_NG) {
        // a button laid out or re-enabled this frame can't be hit until the frame
        // is painted: click until the tool takes
        const deadline = Date.now() + TOOLBAR_CLICK_TIMEOUT_MS * SLOW_BUILD_FACTOR;
        for (;;) {
          const res = await client.request(ControlCommand.TestInput, ["click", x, y, 0, 0]);
          const raw = String(res[1] ?? "");
          if (res[0] !== 0 || !raw.startsWith("OK")) {
            throw new Error(`polyline-annotation-placement: toolbar click failed: ${raw}`);
          }
          await sleep(TOOLBAR_CLICK_SETTLE_MS);
          if ((await placementState(client)).active || Date.now() > deadline) {
            return;
          }
        }
      }
      const toolbar = findChildByClass(frame, "SUMATRA_VIRT_TOOLBAR");
      clickAt(toolbar, x, y, 0);
    };

    await clickToolbar();
    let state = await waitForPlacement(client, true);
    // a cursor query from another window (toolbar repaint, notification)
    // after the move resets the cross cursor: move again until it sticks
    const cursorDeadline = Date.now() + 2_000 * SLOW_BUILD_FACTOR;
    for (;;) {
      await moveMouse(client, canvas, center);
      state = await placementState(client);
      if (state.cursor || Date.now() > cursorDeadline) break;
      await sleep(40);
    }
    if (
      !state.notification ||
      !state.cursor ||
      state.points !== 0 ||
      state.annotations !== 0 ||
      state.command !== cmdId("CmdCreateAnnotPolyLine") ||
      state.message !== notification
    ) {
      throw new Error(`polyline-annotation-placement: toolbar did not start clean placement mode\n${state.raw}`);
    }

    await clickAt(canvas, outside.x, outside.y, 0);
    state = await waitForPlacement(client, false);
    if (state.notification || state.annotations !== 0) {
      throw new Error(`polyline-annotation-placement: outside first click did not cancel cleanly\n${state.raw}`);
    }

    await clickToolbar();
    await waitForPlacement(client, true);
    await clickAt(canvas, p1.x, p1.y, 0);
    state = await placementState(client);
    if (!state.active || state.points !== 1 || state.page !== 1 || state.annotations !== 0) {
      throw new Error(`polyline-annotation-placement: first page click did not anchor the path\n${state.raw}`);
    }
    await client.setNotificationsEnabled(false);
    const anchor = placementEnd(state.raw);
    if (USE_NG) {
      // the stroke is custom paint, which PrintWindow does not capture. The
      // dumped end is the same point the preview is drawn to.
      if (!anchor) {
        throw new Error(`polyline-annotation-placement: anchor end missing\n${state.raw}`);
      }
      await moveMouse(client, canvas, p2);
      state = await placementState(client);
      const end = placementEnd(state.raw);
      const want = { x: anchor.x + (p2.x - p1.x), y: anchor.y + (p2.y - p1.y) };
      if (!end || Math.abs(end.x - want.x) > 1 || Math.abs(end.y - want.y) > 1) {
        throw new Error(
          `polyline-annotation-placement: live preview did not follow the pointer (end=${end?.x},${end?.y} want=${want.x},${want.y})\n${state.raw}`,
        );
      }
    } else {
      const before = captureWindowPixels(canvas);
      const blueBefore = countPreviewBlue(before, p1, p2);
      await moveMouse(client, canvas, p2);
      // the preview is drawn on the next frame; a single capture can still be the
      // one from before the move
      let after = captureWindowPixels(canvas);
      let blueAfter = countPreviewBlue(after, p1, p2);
      const paintDeadline = Date.now() + 2_000;
      while (blueAfter < blueBefore + 80 && Date.now() < paintDeadline) {
        await sleep(50);
        after = captureWindowPixels(canvas);
        blueAfter = countPreviewBlue(after, p1, p2);
      }
      if (blueAfter < blueBefore + 80) {
        throw new Error(`polyline-annotation-placement: live preview did not paint (${blueBefore} -> ${blueAfter})`);
      }
    }
    await clickPoints(canvas, [p2, p3]);
    state = await placementState(client);
    if (!state.active || state.points !== 3 || state.annotations !== 0) {
      throw new Error(`polyline-annotation-placement: clicks did not accumulate vertices\n${state.raw}`);
    }
    const rightLp = packCoords(p3.x, p3.y);
    sendMessage(canvas, WM_RBUTTONDOWN, MK_RBUTTON, rightLp);
    sendMessage(canvas, WM_RBUTTONUP, 0, rightLp);
    state = await expectFinished(client, 1, "right-click");
    const bounds = /type=PolyLine page=1 rect=[^\n]+ screen=-?\d+,-?\d+,(-?\d+),(-?\d+)/.exec(state.raw);
    if (!bounds || +bounds[1]! < 180 || +bounds[2]! < 150) {
      throw new Error(
        `polyline-annotation-placement: placed vertices did not define the annotation bounds\n${state.raw}`,
      );
    }

    await client.setNotificationsEnabled(true);
    await paletteRun(client, frame, ">Create Polyline Annotation", "CmdCreateAnnotPolyLine");
    await waitForPlacement(client, true);
    await clickPoints(canvas, [p1, p2, p3]);
    const dblLp = packCoords(p3.x, p3.y);
    sendMessage(canvas, WM_LBUTTONDBLCLK, MK_LBUTTON, dblLp);
    sendMessage(canvas, WM_LBUTTONUP, 0, dblLp);
    await expectFinished(client, 2, "double-click");

    sendCommand(frame, cmdId("CmdCreateAnnotPolyLine"));
    await waitForPlacement(client, true);
    await clickAt(canvas, p1.x, p1.y, 0);
    await pressKey(frame, VK_RETURN, 0);
    state = await placementState(client);
    if (!state.active || state.points !== 1 || state.annotations !== 2) {
      throw new Error(`polyline-annotation-placement: Enter finished an invalid one-point path\n${state.raw}`);
    }
    await clickAt(canvas, p2.x, p2.y, 0);
    await pressKey(frame, VK_RETURN, 0);
    await expectFinished(client, 3, "Enter");

    sendCommand(frame, cmdId("CmdCreateAnnotPolyLine"));
    await waitForPlacement(client, true);
    await clickPoints(canvas, [p1, p2, p3]);
    await pressKey(frame, VK_SPACE, 0);
    await expectFinished(client, 4, "Space");

    sendCommand(frame, cmdId("CmdCreateAnnotPolyLine"));
    await waitForPlacement(client, true);
    await clickPoints(canvas, [p1, p2]);
    await pressKey(frame, VK_ESCAPE, 0);
    state = await waitForPlacement(client, false);
    if (state.notification || state.annotations !== 4) {
      throw new Error(`polyline-annotation-placement: Esc did not cancel without creating\n${state.raw}`);
    }

    sendCommand(frame, cmdId("CmdCreateAnnotPolyLine"));
    await waitForPlacement(client, true);
    await clickPoints(canvas, [p1, p2]);
    await clickAt(canvas, outside.x, outside.y, 0);
    state = await waitForPlacement(client, false);
    if (state.annotations !== 4) {
      throw new Error(`polyline-annotation-placement: outside later click created an annotation\n${state.raw}`);
    }

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotPolyLine"), packCoords(p1.x, p1.y));
    state = await placementState(client);
    if (state.active || state.annotations !== 5) {
      throw new Error(
        `polyline-annotation-placement: a supplied context point did not place immediately\n${state.raw}`,
      );
    }

    // Ctrl+click commits the vertex and closes the path back to the first
    // point, so the annotation repeats it (issue #6119)
    await paletteRun(client, frame, ">Create Polyline Annotation", "CmdCreateAnnotPolyLine");
    await waitForPlacement(client, true);
    // enough vertices that closing the path grows the point vec: appending an
    // element of the vec to itself used to read the freed buffer
    await clickPoints(canvas, [p1, p2, p3]);
    await clickCanvas(client, canvas, p4, MK_CONTROL);
    state = await expectFinished(client, 6, "Ctrl+click");
    // the dump lists every annotation; the one just placed is the last
    const all = [...state.raw.matchAll(/polyline vertices=(\d+) closed=(\d)/g)];
    const closed = all[all.length - 1];
    if (!closed || closed[2] !== "1") {
      throw new Error(`polyline-annotation-placement: Ctrl+click did not close the path\n${state.raw}`);
    }
    if (+closed[1]! !== 5) {
      throw new Error(
        `polyline-annotation-placement: closed path has ${closed[1]} vertices, wanted the 4 clicked plus the repeat`,
      );
    }

    // two vertices are a single segment: closing would double back on it, so
    // Ctrl+click keeps collecting instead
    await paletteRun(client, frame, ">Create Polyline Annotation", "CmdCreateAnnotPolyLine");
    await waitForPlacement(client, true);
    await clickAt(canvas, p1.x, p1.y, 0);
    await clickCanvas(client, canvas, p2, MK_CONTROL);
    state = await placementState(client);
    if (!state.active || state.points !== 2) {
      throw new Error(`polyline-annotation-placement: Ctrl+click closed a single segment\n${state.raw}`);
    }
    await pressKey(frame, VK_ESCAPE, 0);
    await waitForPlacement(client, false);

    // Shift snaps the segment from the previous vertex to 45 degrees (issue #6195).
    // SetCursorPos queues a WM_MOUSEMOVE with no Shift, which can land after the
    // synthetic one and undo the snap. Resend until the horizontal preview sticks.
    sendCommand(frame, cmdId("CmdCreateAnnotPolyLine"));
    await waitForPlacement(client, true);
    await clickAt(canvas, p1.x, p1.y, 0);
    state = await placementState(client);
    const shiftAnchor = placementEnd(state.raw);
    const nearlyFlat = { x: p1.x + 150, y: p1.y + 12 };
    await moveMouse(client, canvas, nearlyFlat, MK_SHIFT);
    let snapped = false;
    for (let i = 0; i < 4 && !snapped; i++) {
      // a posted move reads GetKeyState, which is up on a locked desktop, and
      // would clear the snap the control-pipe move just applied
      if (USE_NG) {
        await moveMouse(client, canvas, nearlyFlat, MK_SHIFT);
      } else {
        sendMessage(canvas, WM_MOUSEMOVE, MK_SHIFT, packCoords(nearlyFlat.x, nearlyFlat.y));
      }
      state = await placementState(client);
      const end = placementEnd(state.raw);
      const origin = USE_NG && shiftAnchor ? shiftAnchor : p1;
      snapped = !!end && Math.abs(end.y - origin.y) <= 1 && Math.abs(end.x - origin.x) > 40;
    }
    if (!snapped) {
      throw new Error(`polyline-annotation-placement: Shift did not snap preview to horizontal\n${state.raw}`);
    }
    await moveMouse(client, canvas, nearlyFlat);
    const want =
      USE_NG && shiftAnchor
        ? { x: shiftAnchor.x + (nearlyFlat.x - p1.x), y: shiftAnchor.y + (nearlyFlat.y - p1.y) }
        : nearlyFlat;
    // Queued cursor moves can overwrite the synthetic Shift-release move too.
    const pointerDeadline = Date.now() + 2_000 * SLOW_BUILD_FACTOR;
    let restored = false;
    for (;;) {
      if (USE_NG) {
        await moveMouse(client, canvas, nearlyFlat);
      } else {
        sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(nearlyFlat.x, nearlyFlat.y));
      }
      state = await placementState(client);
      const back = placementEnd(state.raw);
      restored = !!back && back.x === want.x && back.y === want.y;
      if (restored || Date.now() > pointerDeadline) break;
      await sleep(40);
    }
    if (!restored) {
      const back = placementEnd(state.raw);
      throw new Error(
        `polyline-annotation-placement: releasing Shift did not restore the pointer (end=${back?.x},${back?.y} want=${want.x},${want.y})\n${state.raw}`,
      );
    }
    await clickCanvas(client, canvas, nearlyFlat, MK_SHIFT);
    await pressKey(frame, VK_RETURN, 0);
    state = await expectFinished(client, 7, "Shift+click");
    const polys = [...state.raw.matchAll(/polyline vertices=\d+ closed=\d pts=([^\n]*)/g)];
    const pts = polys[polys.length - 1]?.[1]?.split(";").map((xy) => xy.split(",").map(Number));
    if (!pts || pts.length !== 2 || Math.abs(pts[0]![1]! - pts[1]![1]!) > 1.5) {
      throw new Error(`polyline-annotation-placement: Shift+click did not snap the vertex\n${state.raw}`);
    }

    // Shift-dragging a vertex of the (selected) new polyline snaps it too
    const vertex = { x: nearlyFlat.x, y: p1.y };
    const steep = { x: p1.x + 100, y: p1.y + 108 };
    await moveMouse(client, canvas, vertex);
    if (USE_NG) {
      await ngMouse(client, "down", vertex, 0, MK_SHIFT);
      await ngMouse(client, "move", steep, 1, MK_SHIFT);
      await ngMouse(client, "up", steep, 0, MK_SHIFT);
    } else {
      sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON | MK_SHIFT, packCoords(vertex.x, vertex.y));
      // SetCapture synthesizes a move without Shift; let it land first
      await sleep(50);
      sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON | MK_SHIFT, packCoords(steep.x, steep.y));
      sendMessage(canvas, WM_LBUTTONUP, MK_SHIFT, packCoords(steep.x, steep.y));
    }
    await client.waitForRenderIdle();
    state = await placementState(client);
    const dragged = [...state.raw.matchAll(/polyline vertices=\d+ closed=\d pts=([^\n]*)/g)];
    const dpts = dragged[dragged.length - 1]?.[1]?.split(";").map((xy) => xy.split(",").map(Number));
    const ddx = dpts ? Math.abs(dpts[1]![0]! - dpts[0]![0]!) : 0;
    const ddy = dpts ? Math.abs(dpts[1]![1]! - dpts[0]![1]!) : 0;
    if (!dpts || dpts.length !== 2 || ddx < 20 || Math.abs(ddx - ddy) > 1.5) {
      throw new Error(
        `polyline-annotation-placement: Shift-drag did not snap the vertex (dx=${ddx} dy=${ddy})\n${state.raw}`,
      );
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("polyline-annotation-placement: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
