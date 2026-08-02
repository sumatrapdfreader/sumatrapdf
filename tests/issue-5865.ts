// #5865: after quitting while in fullscreen with session restore on, mouse-wheel
// scrolling did nothing in the auto-restored document (keyboard scrolling still
// worked). The hidden tab bar kept regenerating WM_PAINT, which starved the
// low-priority WM_TIMER that drives smooth wheel scrolling.
//
// Exercises the real session save/restore path, so it can NOT use -for-testing
// (that skips both session restore and saving settings); it points the app at a
// throwaway settings dir with -appdata instead.
import { spawnSync } from "node:child_process";
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT, cmdId, runStandalone, tmpPath } from "./util";
import { clientToScreen, getClientRect, getScrollInfo, packCoords, postMessage, setCursorPos, sleep,
    waitForTopWindow } from "./winapi";
import { FRAME_CLASS, findCanvas, sendCommand } from "./win-automation";

const WM_MOUSEWHEEL = 0x020a;
const WHEEL_DELTA = 120;

const SETTINGS = `RestoreSession = true
RememberOpenedFiles = true
RememberStatePerDocument = true
LazyLoading = false
SmoothScroll = true
UseTabs = true
ReuseInstance = false
CheckForUpdates = false
UiLanguage = en
`;

function launch(appDataDir: string, extra: string[]): Bun.Subprocess {
    return Bun.spawn([EXE, "-appdata", appDataDir, ...extra], { stdout: "ignore", stderr: "ignore" });
}

// wheel down over the middle of the canvas; returns how the vertical scrollbar moved
async function wheelScroll(canvas: number, notches: number): Promise<{ before: number; after: number }> {
    // park the real cursor over the document: the wheel handler redirects to the
    // ToC tree when the cursor is over the sidebar
    const cr = getClientRect(canvas);
    const mid = clientToScreen(canvas, Math.floor(cr.right / 2), Math.floor(cr.bottom / 2));
    setCursorPos(mid.x, mid.y);
    await sleep(200);
    const before = getScrollInfo(canvas).pos;
    for (let i = 0; i < notches; i++) {
        postMessage(canvas, WM_MOUSEWHEEL, (-WHEEL_DELTA << 16) >>> 0, packCoords(mid.x, mid.y));
        await sleep(120);
    }
    await sleep(800); // smooth scroll animates
    return { before, after: getScrollInfo(canvas).pos };
}

// CPU seconds burned while idle: the paint storm that caused #5865 pegs a core
function cpuTime(pid: number): number {
    const args = ["-NoProfile", "-Command", `(Get-Process -Id ${pid}).CPU`];
    const out = spawnSync("powershell", args, { encoding: "utf8" });
    return parseFloat((out.stdout || "0").trim()) || 0;
}

export async function testit(): Promise<void> {
    const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
    const dir = tmpPath("issue-5865-appdata");
    rmSync(dir, { recursive: true, force: true });
    mkdirSync(dir, { recursive: true });
    writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);

    // run 1: open the doc, go fullscreen, quit (saves WindowState = 3)
    let proc = launch(dir, [pdf]);
    let frame = await waitForTopWindow(proc.pid, FRAME_CLASS);
    if (!frame) {
        throw new Error("run1: no frame window");
    }
    await sleep(3000);
    sendCommand(frame, cmdId("CmdToggleFullscreen"));
    await sleep(1500);
    let canvas = findCanvas(frame);
    if (!canvas) {
        throw new Error("run1: no canvas");
    }
    let r = await wheelScroll(canvas, 3);
    if (r.after === r.before) {
        throw new Error(`run1: wheel scroll did nothing in fullscreen (pos stuck at ${r.before})`);
    }
    sendCommand(frame, cmdId("CmdExit"));
    await proc.exited;
    await sleep(500);

    // run 2: session restore into fullscreen, no file on the cmd line
    proc = launch(dir, []);
    frame = await waitForTopWindow(proc.pid, FRAME_CLASS);
    if (!frame) {
        throw new Error("run2: no frame window");
    }
    await sleep(3500);
    canvas = findCanvas(frame);
    if (!canvas) {
        throw new Error("run2: no canvas");
    }
    const cpu0 = cpuTime(proc.pid);
    await sleep(2000);
    const cpuIdle = cpuTime(proc.pid) - cpu0;
    r = await wheelScroll(canvas, 3);
    sendCommand(frame, cmdId("CmdExit"));
    await proc.exited;

    if (r.after === r.before) {
        throw new Error(`wheel scroll did nothing in the restored fullscreen session (pos stuck at ${r.before})`);
    }
    if (cpuIdle > 0.5) {
        throw new Error(`restored fullscreen window is spinning: ${cpuIdle.toFixed(2)}s CPU over 2s idle`);
    }
}

if (import.meta.main) {
    await runStandalone(testit);
}
