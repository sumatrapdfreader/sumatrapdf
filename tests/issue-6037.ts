// #6037: creating a stamp / square from the context menu or command palette
// must paint it immediately. SetSelectedAnnotation only ScheduleRepaint, which
// blits the cached page bitmap (no annot). A later click re-selects the same
// annot and MainWindowRerender — that is why clicking made it appear.
import { mkdirSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util";
import { captureWindowToPng, packCoords, sendMessage, sleep, WM_COMMAND } from "./winapi";
import { findCanvas, killAndWait, launchControlled } from "./win-automation";

function countRedPixels(png: string): number {
  const p = png.split("\\").join("\\\\");
  const ps = `Add-Type -AssemblyName System.Drawing; $b=[System.Drawing.Bitmap]::FromFile('${p}'); $n=0; for($y=0;$y -lt $b.Height;$y+=2){for($x=0;$x -lt $b.Width;$x+=2){$c=$b.GetPixel($x,$y); if($c.R -gt 180 -and $c.G -lt 80 -and $c.B -lt 80){$n++}}}; $b.Dispose(); Write-Output $n`;
  const r = Bun.spawnSync(["powershell", "-NoProfile", "-Command", ps]);
  return parseInt(r.stdout.toString().trim(), 10) || 0;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6037");
  mkdirSync(dir, { recursive: true });
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);

    // WM_COMMAND is what the context menu and the command palette send.
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotStamp"), packCoords(80, 80));
    await sleep(400);
    await client.waitForRenderIdle();

    const png = join(dir, "after-create.png");
    if (!captureWindowToPng(canvas, png)) {
      throw new Error("issue-6037: capture after create failed");
    }
    const red = countRedPixels(png);
    if (red < 20) {
      throw new Error(`issue-6037: stamp not painted after create (red=${red})`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("issue-6037: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
