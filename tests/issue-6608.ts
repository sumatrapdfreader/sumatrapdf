// Windows 10 draws a whole native title bar above ours as soon as the frame
// leaves any non-client area at its top, so the top inset must stay zero. The
// top border Windows 10 then lacks is painted by us in the first client row.
//
// Run: bun tests/issue-6608.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { USE_NG, makeOnePagePdf, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { captureWindowPixels, clientToScreen, getWindowRect } from "./winapi.ts";

type TopEdge = { dy: number; color: string };

async function topEdge(client: ControlClient, action: string): Promise<TopEdge> {
  const res = await client.request(ControlCommand.TestFrameTopEdge, [action]);
  const raw = String(res[1] ?? "");
  const m = /edge=(\d+) color=([0-9a-f]{6})/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`issue-6608: TestFrameTopEdge: ${raw}`);
  }
  return { dy: Number(m[1]), color: m[2] };
}

// rows of the frame's client area that are not entirely `color` (rrggbb)
function rowsNotColored(frame: number, dy: number, color: string): string[] {
  const img = captureWindowPixels(frame, "client");
  if (!img) {
    throw new Error("issue-6608: could not capture the frame");
  }
  const bad: string[] = [];
  for (let y = 0; y < dy; y++) {
    for (let x = 0; x < img.w; x++) {
      const i = (y * img.w + x) * 4;
      const [b, g, r] = [img.data[i], img.data[i + 1], img.data[i + 2]];
      const got = [r, g, b].map((c) => c.toString(16).padStart(2, "0")).join("");
      if (got !== color) {
        bad.push(`row ${y} is ${got} at x=${x}`);
        break;
      }
    }
  }
  return bad;
}

export async function testit(): Promise<void> {
  if (!IS_WIN || USE_NG) {
    console.log("SKIP issue-6608: the custom Win32 frame is the original app's");
    return;
  }
  const pdf = tmpPath("issue-6608.pdf");
  writeFileSync(pdf, makeOnePagePdf(), "latin1");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    const inset = clientToScreen(frame, 0, 0).y - getWindowRect(frame).top;
    if (inset !== 0) {
      throw new Error(`issue-6608: top non-client inset is ${inset}: Windows 10 draws its title bar above ours`);
    }

    const edge = await topEdge(client, "force");
    if (edge.dy < 1) {
      throw new Error("issue-6608: a restored frame paints no top edge as Windows 10");
    }
    const bad = rowsNotColored(frame, edge.dy, edge.color);
    if (bad.length > 0) {
      throw new Error(`issue-6608: the top edge is not ${edge.color}: ${bad.join(", ")}`);
    }
    console.log("issue-6608: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
