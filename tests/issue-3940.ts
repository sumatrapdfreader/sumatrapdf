// #3940: a link's inline app.popUpMenu(['Symbol', 'Path=...'], [...]) shows each
// array as a submenu: its first string is the title, the rest its items.
// Picking an item that is neither a URL nor a page copies its text.
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, runStandalone, tmpPath, USE_NG } from "./util";
import { getClipboardText, setClipboardText, sleep, VK_DOWN, VK_RETURN, VK_RIGHT } from "./winapi";
import { killAndWait, launchControlled } from "./win-automation";
import { writeFileSync } from "node:fs";

// the link's script from the issue's example.pdf
const MENU_JS =
  "var citem = app.popUpMenu( [ 'Symbol', 'Path=Resistors', 'Symbol=resistor_sym_1.smb', 'ComponentType=gate', " +
  "'FunctionType=Resistor - Shunt' ], [ 'Location', 'ID=sht1.cmp3', 'X=182.500', 'Y=210.000', 'Mirror=OFF', " +
  "'Scale=1.000', 'Angle=0.000' ],  [ 'Object Properties', 'Reference Designator=R0001', 'Pack_Type=0805', " +
  "'Temperature Range=-55+155' ] ); if ( citem == 'Reference Designator=R0001' ) { app.alert( " +
  "'Reference Designator=R0001', 3 ); }";

const ITEMS = [
  "Symbol",
  "Path=Resistors",
  "FunctionType=Resistor - Shunt",
  "Location",
  "Angle=0.000",
  "Object Properties",
  "Reference Designator=R0001",
  "Temperature Range=-55+155",
];
const LEVELS = "0,1,1,1,1,0,1,1,1,1,1,1,0,1,1,1";
// Down selects the first item, so "Object Properties", the 3rd, takes 3
const OBJECT_PROPS_DOWNS = 3;
const PICKED = "Reference Designator=R0001";
const POPUP_MS = 3000;

function pdfString(s: string): string {
  return "(" + s.replace(/[\\()]/g, (c) => "\\" + c) + ")";
}

// one page whose link covers all of it
function makePdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: string[] = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    `<< /Type /Annot /Subtype /Link /Rect [0 0 612 792] /Border [0 0 0] /A << /S /JavaScript /JS ${pdfString(MENU_JS)} >> >>`,
  ];
  let out = "%PDF-1.4\n";
  const offsets: number[] = [];
  body.forEach((b, i) => {
    offsets.push(out.length);
    out += `${i + 1} 0 obj\n${b}\nendobj\n`;
  });
  const xrefPos = out.length;
  out += `xref\n0 ${body.length + 1}\n0000000000 65535 f \n`;
  for (const o of offsets) {
    out += `${String(o).padStart(10, "0")} 00000 n \n`;
  }
  out += `trailer\n<< /Size ${body.length + 1} /Root 1 0 R >>\nstartxref\n${xrefPos}\n%%EOF\n`;
  return enc(out);
}

async function uiState(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
}

// ng shows the menu in a gpui popup; orig's TrackPopupMenu can't be driven from here
async function testNgCopy(pdf: string): Promise<void> {
  setClipboardText("");
  const { proc, client } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    const m = /canvas=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(await uiState(client));
    if (!m) {
      throw new Error("issue-3940: no canvas rect");
    }
    const x = +m[1]! + Math.floor(+m[3]! / 2);
    const y = +m[2]! + Math.floor(+m[4]! / 2);
    await client.request(ControlCommand.TestInput, ["click", x, y, 0, 0]);
    const deadline = Date.now() + POPUP_MS;
    while (!/ popup=1/.test(await uiState(client))) {
      if (Date.now() > deadline) {
        throw new Error("issue-3940: clicking the link didn't open its menu");
      }
      await sleep(40);
    }
    await client.waitForRenderIdle();
    const keys = [...Array(OBJECT_PROPS_DOWNS).fill(VK_DOWN), VK_RIGHT, VK_RETURN];
    for (const key of keys) {
      await client.request(ControlCommand.TestInput, ["key", key, 0, 0, 0]);
    }
    const got = await getClipboardText(POPUP_MS);
    if (got !== PICKED) {
      throw new Error(`issue-3940: clipboard ${JSON.stringify(got)}, expected ${JSON.stringify(PICKED)}`);
    }
  } finally {
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-3940.pdf");
  writeFileSync(pdf, makePdf());

  await withControlledSumatra(EXE, async (client: ControlClient) => {
    const res = await client.request(ControlCommand.TestPageLinks, [pdf, 1]);
    const dump = String(res[1] ?? "");
    const menus = dump.split("\n").filter((l) => l.startsWith("kind=jsMenu "));
    if (res[0] !== 0 || menus.length !== 1) {
      throw new Error(`issue-3940: expected 1 jsMenu link:\n${dump}`);
    }
    const menu = menus[0]!;
    for (const item of ITEMS) {
      if (!menu.includes(item)) {
        throw new Error(`issue-3940: menu should list '${item}':\n${menu}`);
      }
    }
    if (!menu.includes(`levels=${LEVELS}`)) {
      throw new Error(`issue-3940: expected 'levels=${LEVELS}':\n${menu}`);
    }
  });

  if (USE_NG) {
    await testNgCopy(pdf);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
