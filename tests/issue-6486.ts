// Test for issue #6486: PDF layers (optional content groups).
//
// Generates a PDF with four layers, each filling a square: Red (on) and Blue
// (off) in a "Shapes" group, and a radio group of Day (on, green) and Night
// (off, black). Checks the Layers view's rows, that toggling a row changes
// what renders (through the cached display list, like the window) and the
// state saved for the document, and that turning Night on turns Day off.

import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

function buildLayersPdf(): Uint8Array {
  const content = [
    "/OC /R BDC 1 0 0 rg 10 10 80 80 re f EMC",
    "/OC /B BDC 0 0 1 rg 110 10 80 80 re f EMC",
    "/OC /D BDC 0 1 0 rg 10 110 80 80 re f EMC",
    "/OC /N BDC 0 0 0 rg 110 110 80 80 re f EMC",
  ].join("\n");
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R /OCProperties << /OCGs [5 0 R 6 0 R 7 0 R 8 0 R] " +
      "/D << /Order [[(Shapes) 5 0 R 6 0 R] 7 0 R 8 0 R] /OFF [6 0 R 8 0 R] /RBGroups [[7 0 R 8 0 R]] >> >> >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] /Contents 4 0 R " +
      "/Resources << /Properties << /R 5 0 R /B 6 0 R /D 7 0 R /N 8 0 R >> >> >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
    "<< /Type /OCG /Name (Red) >>",
    "<< /Type /OCG /Name (Blue) >>",
    "<< /Type /OCG /Name (Day) >>",
    "<< /Type /OCG /Name (Night) >>",
  ];
  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  objs.forEach((o, i) => {
    offsets.push(pdf.length);
    pdf += `${i + 1} 0 obj\n${o}\nendobj\n`;
  });
  const xref = pdf.length;
  pdf += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += `${String(off).padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return new TextEncoder().encode(pdf);
}

type LayersState = { panel: number; states: string; red: number; blue: number; rows: string[] };

async function layers(client: ControlClient, action: string): Promise<LayersState> {
  const [, raw] = await client.request(ControlCommand.TestLayers, [action]);
  const st: LayersState = { panel: 0, states: "", red: -1, blue: -1, rows: [] };
  for (const line of String(raw ?? "").split("\n")) {
    if (line === "") {
      continue;
    }
    const color = line.match(/^red=(\d+) blue=(\d+)$/);
    if (color) {
      st.red = Number(color[1]);
      st.blue = Number(color[2]);
    } else if (line.startsWith("panel=")) {
      st.panel = Number(line.slice(6));
    } else if (line.startsWith("states=")) {
      st.states = line.slice(7);
    } else {
      st.rows.push(line);
    }
  }
  return st;
}

function expectEq(what: string, got: unknown, want: unknown): void {
  const g = JSON.stringify(got);
  const w = JSON.stringify(want);
  if (g !== w) {
    throw new Error(`issue-6486: ${what}: got ${g}, want ${w}`);
  }
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-6486.pdf");
  writeFileSync(pdfPath, buildLayersPdf());

  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();

      let st = await layers(client, "colors");
      console.log(`issue-6486: initial ${JSON.stringify(st)}`);
      const initialRows = [
        "0 label 0 Shapes",
        "1 checkbox 1 Red",
        "1 checkbox 0 Blue",
        "0 radio 1 Day",
        "0 radio 0 Night",
      ];
      expectEq("rows", st.rows, initialRows);
      expectEq("states before any change", st.states, "");
      if (st.red <= 0 || st.blue !== 0) {
        throw new Error(`issue-6486: want red shown and blue hidden, got red=${st.red} blue=${st.blue}`);
      }

      // Red off
      st = await layers(client, "toggle 1");
      expectEq("Red row after Red off", st.rows[1], "1 checkbox 0 Red");
      if (st.states.length !== 4) {
        throw new Error(`issue-6486: want a state saved for the 4 layers, got "${st.states}"`);
      }
      st = await layers(client, "colors");
      expectEq("red after Red off", st.red, 0);

      // Blue on
      await layers(client, "toggle 2");
      st = await layers(client, "colors");
      if (st.blue <= 0) {
        throw new Error(`issue-6486: Blue turned on but nothing blue rendered`);
      }

      // Night on: a radio button turns Day off
      st = await layers(client, "toggle 4");
      expectEq("rows after Night on", st.rows.slice(3), ["0 radio 0 Day", "0 radio 1 Night"]);

      // a label isn't a layer
      const before = st.states;
      st = await layers(client, "toggle 0");
      expectEq("states after clicking the label", st.states, before);

      // back as the document has it: nothing to save
      await layers(client, "toggle 1");
      await layers(client, "toggle 2");
      st = await layers(client, "toggle 3");
      expectEq("rows back as loaded", st.rows, initialRows);
      expectEq("states back as loaded", st.states, "");

      st = await layers(client, "show");
      expectEq("Layers view shown", st.panel, 1);
    },
    [pdfPath],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
