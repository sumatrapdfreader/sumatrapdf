// #6517: Siemens Xpedition (SCH2PDF) schematic PDFs keep a string table and a
// menu builder _b._c() in the catalog's /OpenAction script; each link passes
// index arrays into the table, _b._c(symbol, prop, page, jumps), and _c shows
// them as nested app.popUpMenu submenus.
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, runStandalone, tmpPath, USE_NG } from "./util";
import { sleep, VK_DOWN, VK_RETURN, VK_RIGHT } from "./winapi";
import { killAndWait, launchControlled } from "./win-automation";

function pdfString(s: string): string {
  let out = "(";
  for (const ch of s) {
    if (ch === "\\" || ch === "(" || ch === ")") {
      out += "\\";
    }
    out += ch;
  }
  return out + ")";
}

// SCH2PDF 6.0's /OpenAction script (bug-6517.pdf), with 'Sheet 2 Ref' and
// 'Jump To: ' appended to the string table
const OPEN_JS =
  "var t = ['Symbol :','Property :','Push Schematic','Symbol: ','Globals:gnd.1','Property: '," +
  "'Global Signal Name','GND','Sheet 2 Ref','Jump To: '];  var _this = this;var _b = {_c: function (ss_, pp_, pa_, jj_) " +
  "{try {var m = [];var ja = [];var s = [];s.push(t[ss_[0]]);s.push(t[ss_[1]]);m.push(s);if (pp_.length > 0) " +
  "{var p = [];for (var inp = 0; inp < pp_.length; ++inp) {if (pp_[inp] instanceof Array) {try {var res = " +
  "t[pp_[inp][0]] + ' = ' + t[pp_[inp][1]];p.push(String(res));} catch (err) {}} else {p.push(String(t[pp_[inp]]));}}" +
  "m.push(p);}if (pa_ || pa_ == '0') {var pushMenu = [];pushMenu.push(String(t[2]));m.push(String(t[2]));}" +
  "if (jj_.length > 0) {var p1 = [];for (var index = 0; index < jj_.length; ++index) {if (jj_[index] instanceof Array) " +
  "{try {var res1 = t[jj_[index][0]];ja[String(res1)] = jj_[index][1];p1.push(String(res1));} catch (err) {}} else " +
  "{p1.push(String(t[jj_[index]]));}}m.push(p1);}var i = app.popUpMenu.apply(app, m);if (i == t[2]) " +
  "{_this.pageNum = pa_;_this.zoomType = zoomtype.fitW;} else {for (var it in ja) { if (it == i) " +
  '{var pageNum = ja[it];var dest = "[{"+pageNum+"}]";_this.gotoNamedDest(dest);}}}} catch (error) {}}};';

// the sample's link, verbatim
const SYMBOL_JS = " var symbol = [3, 4];  var prop = [5,[6,7]];  _b._c(symbol,prop,'',[]); ";
// Push Schematic goes to 0-based page 1, Sheet 2 Ref to named dest [{7}]
const JUMP_JS = " var symbol = [3, 4];  var prop = [5,[6,7]];  var jmp = [9,[8,7]];  _b._c(symbol,prop,'1',jmp); ";

const SYMBOL_RECT = "72 700 172 740";
const JUMP_RECT = "72 600 172 640";
// covers the whole page, so a click in the middle of the canvas hits it
const PAGE_RECT = "0 0 612 792";
// Down selects the first item, so "Push Schematic", the 3rd, takes 3
const PUSH_ITEM_DOWNS = 3;
const JUMP_TO_DOWNS = 4;
const POPUP_MS = 3000;

function makePdf(jumpRect: string, withSymbol: boolean): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R /Names 8 0 R /OpenAction 10 0 R >>");
  body[2] = enc("<< /Type /Pages /Kids [3 0 R 11 0 R] /Count 2 >>");
  body[6] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
  const stream = "BT /F1 12 Tf 72 710 Td (GND) Tj 0 -100 Td (JMP) Tj ET";
  body[7] = enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);

  body[4] = enc(
    `<< /Type /Annot /Subtype /Link /Rect [${SYMBOL_RECT}] /Border [0 0 1] /A << /Type /Action /S /JavaScript /JS ${pdfString(SYMBOL_JS)} >> >>`,
  );
  body[5] = enc(
    `<< /Type /Annot /Subtype /Link /Rect [${jumpRect}] /Border [0 0 1] /A << /Type /Action /S /JavaScript /JS ${pdfString(JUMP_JS)} >> >>`,
  );
  body[3] = enc(
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${withSymbol ? "4 0 R 5 0 R" : "5 0 R"}] ` +
      `/Resources << /Font << /F1 6 0 R >> >> /Contents 7 0 R >>`,
  );
  body[11] = enc(
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 6 0 R >> >> /Contents 7 0 R >>`,
  );

  body[8] = enc("<< /Dests 9 0 R >>");
  body[9] = enc("<< /Names [ ([{7}]) [11 0 R /Fit] ] >>");
  body[10] = enc(`<< /Type /Action /S /JavaScript /JS ${pdfString(OPEN_JS)} >>`);

  const maxN = 11;
  const parts: Buffer[] = [enc("%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0]!.length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n] ?? enc("null"), enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let n = 1; n <= maxN; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

async function uiState(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
}

async function waitForUi(client: ControlClient, re: RegExp, what: string): Promise<void> {
  const deadline = Date.now() + POPUP_MS;
  while (!re.test(await uiState(client))) {
    if (Date.now() > deadline) {
      throw new Error(`issue-6517: ${what}`);
    }
    await sleep(40);
  }
}

// ng shows the menu in a gpui popup; picking `keys` goes to page 2.
// orig's TrackPopupMenu can't be driven from here.
async function testNgPopup(keys: number[], what: string): Promise<void> {
  const pdf = tmpPath("issue-6517-page.pdf");
  writeFileSync(pdf, makePdf(PAGE_RECT, false));
  const { proc, client } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    const m = /canvas=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(await uiState(client));
    if (!m) {
      throw new Error("issue-6517: no canvas rect");
    }
    const x = +m[1]! + Math.floor(+m[3]! / 2);
    const y = +m[2]! + Math.floor(+m[4]! / 2);
    await client.request(ControlCommand.TestInput, ["click", x, y, 0, 0]);
    await waitForUi(client, / popup=1/, "clicking the link didn't open its menu");

    await client.waitForRenderIdle();
    for (const key of keys) {
      await client.request(ControlCommand.TestInput, ["key", key, 0, 0, 0]);
    }
    await client.request(ControlCommand.TestInput, ["key", VK_RETURN, 0, 0, 0]);
    await waitForUi(client, / page=2 /, `picking '${what}' didn't go to page 2`);
  } finally {
    await killAndWait(proc);
  }
}

function checkMenu(menu: string, items: string[], levels: string, actions: string): void {
  for (const item of items) {
    if (!menu.includes(item)) {
      throw new Error(`issue-6517: menu should list '${item}':\n${menu}`);
    }
  }
  for (const want of [`levels=${levels}`, `actions=${actions}`]) {
    if (!menu.includes(want)) {
      throw new Error(`issue-6517: expected '${want}':\n${menu}`);
    }
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6517.pdf");
  writeFileSync(pdf, makePdf(JUMP_RECT, true));

  await withControlledSumatra(EXE, async (client: ControlClient) => {
    const res = await client.request(ControlCommand.TestPageLinks, [pdf, 1]);
    const dump = String(res[1] ?? "");
    if (res[0] !== 0) {
      throw new Error(`issue-6517: TestPageLinks failed:\n${dump}`);
    }

    const menus = dump.split("\n").filter((l) => l.startsWith("kind=jsMenu "));
    if (menus.length !== 2) {
      throw new Error(`issue-6517: expected 2 jsMenu links, got ${menus.length}:\n${dump}`);
    }
    // the symbol link is the upper one (smaller src y)
    const srcY = (l: string) => +(/ src=[-\d.]+,([-\d.]+),/.exec(l)?.[1] ?? 0);
    menus.sort((a, b) => srcY(a) - srcY(b));
    const items = ["Symbol:", "Globals:gnd.1", "Property:", "Global Signal Name = GND"];
    checkMenu(menus[0]!, items, "0,1,0,1", "-;-;-;-");
    // Push Schematic: 0-based '1' is page 2; Sheet 2 Ref: [{7}] names page 2
    const jumpItems = [...items, "Push Schematic", "Jump To:", "Sheet 2 Ref"];
    checkMenu(menus[1]!, jumpItems, "0,1,0,1,0,0,1", "-;-;-;-;page:2;-;page:2");
  });

  if (USE_NG) {
    await testNgPopup(Array(PUSH_ITEM_DOWNS).fill(VK_DOWN), "Push Schematic");
    // Right opens the Jump To submenu on its first item
    await testNgPopup([...Array(JUMP_TO_DOWNS).fill(VK_DOWN), VK_RIGHT], "Jump To > Sheet 2 Ref");
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
