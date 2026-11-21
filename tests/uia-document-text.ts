// Screen readers read the document through UI Automation: the canvas exposes
// a Document element (AutomationId "Document") whose TextPattern holds the
// pages' text and the current text selection, with a child per page ("Page N",
// ValuePattern = the page's text). With no document the canvas shows a
// "Start Page" element. Orig: src/uia; ng: the gpui accessibility tree
// (src/ng/gui/DocCanvasA11y.cpp).
//
// The UIA client is Windows PowerShell's System.Windows.Automation.
// Run:  bun tests/uia-document-text.ts [--no-build] [-ng]

import { writeFileSync } from "node:fs";
import { IS_WIN } from "./host.ts";
import { makeLinesPdf, runPowerShell, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const kWord = "Zanzibar";
const kLine = `Hello ${kWord} world`;
const kPageWord = "Kilimanjaro";
const kLines = [kLine, `Second ${kPageWord} page`, "Third Timbuktu page"];

type UiaPage = { name: string; value: string; w: number; h: number };
type UiaDoc = { name: string; text: string; selection: string; pages: UiaPage[] };

function runUia(body: string): any {
  const script = `
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
$A = [System.Windows.Automation.AutomationElement]
${body}
$out | ConvertTo-Json -Compress -Depth 4
`;
  const r = runPowerShell(script);
  const line = r.out.trim().split(/\r?\n/).pop() ?? "";
  let res: any;
  try {
    res = JSON.parse(line);
  } catch {
    throw new Error(`uia-document-text: bad PowerShell output:\n${r.out}`);
  }
  if (res.error) {
    throw new Error(`uia-document-text: ${res.error}`);
  }
  return res;
}

// Polls inside one PowerShell run: ng collects its accessibility tree on the
// frame after the first UIA request.
function readUiaDocument(frame: number, wantSelection: boolean): UiaDoc {
  const res = runUia(`
$root = $A::FromHandle([IntPtr]${frame})
$cond = New-Object System.Windows.Automation.PropertyCondition($A::AutomationIdProperty, 'Document')
$out = $null
for ($i = 0; $i -lt 40; $i++) {
  try {
    $doc = $root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $cond)
    if ($doc) {
      $tp = $doc.GetCurrentPattern([System.Windows.Automation.TextPattern]::Pattern)
      $text = $tp.DocumentRange.GetText(-1)
      $sel = ($tp.GetSelection() | ForEach-Object { $_.GetText(-1) }) -join ''
      $pages = @()
      $kids = $doc.FindAll([System.Windows.Automation.TreeScope]::Children, [System.Windows.Automation.Condition]::TrueCondition)
      foreach ($k in $kids) {
        $v = ''
        $vp = $null
        if ($k.TryGetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern, [ref]$vp)) { $v = $vp.Current.Value }
        $r = $k.Current.BoundingRectangle
        $pages += @{ name = $k.Current.Name; value = "$v"; w = $r.Width; h = $r.Height }
      }
      $out = @{ name = $doc.Current.Name; text = $text; selection = $sel; pages = $pages }
      if ($text.Contains('${kWord}') -and $pages.Count -eq ${kLines.length} -and (-not ${wantSelection ? "$true" : "$false"} -or $sel.Length -gt 0)) { break }
    }
  } catch { $out = @{ error = $_.Exception.Message } }
  Start-Sleep -Milliseconds 250
}
if (-not $out) { $out = @{ error = 'no Document element' } }
`);
  const pages: UiaPage[] = (res.pages ?? []).map((p: any) => ({
    name: p.name ?? "",
    value: p.value ?? "",
    w: Number.isFinite(p.w) ? p.w : 0,
    h: Number.isFinite(p.h) ? p.h : 0,
  }));
  return { name: res.name ?? "", text: res.text ?? "", selection: res.selection ?? "", pages };
}

function checkPages(pages: UiaPage[]): void {
  const names = pages.map((p) => p.name);
  const want = kLines.map((_, i) => `Page ${i + 1}`);
  if (names.join("|") !== want.join("|")) {
    throw new Error(`uia-document-text: page children ${JSON.stringify(names)}, expected ${JSON.stringify(want)}`);
  }
  if (!pages[1].value.includes(kPageWord) || pages[0].value.includes(kPageWord)) {
    throw new Error(`uia-document-text: page values ${JSON.stringify(pages.map((p) => p.value))}`);
  }
  if (!(pages[0].w > 0 && pages[0].h > 0)) {
    throw new Error(`uia-document-text: page 1 bounds ${pages[0].w}x${pages[0].h}`);
  }
}

// orig's StartPageProvider: the canvas's one child when no document is open
function readStartPage(frame: number): string {
  const res = runUia(`
$root = $A::FromHandle([IntPtr]${frame})
$cond = New-Object System.Windows.Automation.PropertyCondition($A::NameProperty, 'Start Page')
$out = @{ error = 'no Start Page element' }
for ($i = 0; $i -lt 40; $i++) {
  $el = $root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $cond)
  if ($el) { $out = @{ name = $el.Current.Name }; break }
  Start-Sleep -Milliseconds 250
}
`);
  return res.name ?? "";
}

async function testStartPage(): Promise<void> {
  const { proc, client, frame } = await launchControlled([]);
  try {
    readStartPage(frame);
  } finally {
    client.close();
    try {
      await killAndWait(proc);
    } catch {
      // already exited
    }
  }
}

export async function testit(): Promise<void> {
  if (!IS_WIN) {
    console.log("uia-document-text: UI Automation is Windows-only, skipping");
    return;
  }
  const pdf = tmpPath("uia-document-text.pdf");
  writeFileSync(pdf, makeLinesPdf(kLines));

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();

    const before = readUiaDocument(frame, false);
    if (!before.text.includes(kLine)) {
      throw new Error(`uia-document-text: document text ${JSON.stringify(before.text)} lacks ${JSON.stringify(kLine)}`);
    }
    if (before.name !== "uia-document-text.pdf") {
      throw new Error(`uia-document-text: document name ${JSON.stringify(before.name)}`);
    }
    if (before.selection !== "") {
      throw new Error(`uia-document-text: selection before selecting: ${JSON.stringify(before.selection)}`);
    }
    checkPages(before.pages);

    await client.seedTextSelection(1);
    const after = readUiaDocument(frame, true);
    if (!after.selection.startsWith(`Hello ${kWord}`) || !kLine.startsWith(after.selection.trimEnd())) {
      throw new Error(`uia-document-text: selection ${JSON.stringify(after.selection)}, expected a prefix of ${kLine}`);
    }
    console.log(`uia-document-text: ok, ${before.pages.length} pages, selection ${JSON.stringify(after.selection)}`);
  } finally {
    client.close();
    try {
      await killAndWait(proc);
    } catch {
      // already exited
    }
  }

  await testStartPage();
  console.log("uia-document-text: ok, Start Page");
}

if (import.meta.main) {
  await runStandalone(testit);
}
