// Screen readers read the document through UI Automation: the canvas exposes
// a Document element (AutomationId "Document") whose TextPattern holds the
// pages' text and the current text selection. Orig: src/uia; ng: the gpui
// accessibility tree (src/ng/gui/DocCanvasA11y.cpp).
//
// The UIA client is Windows PowerShell's System.Windows.Automation.
// Run:  bun tests/uia-document-text.ts [--no-build] [-ng]

import { writeFileSync } from "node:fs";
import { IS_WIN } from "./host.ts";
import { makeTextPdf, runPowerShell, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const kWord = "Zanzibar";
const kLine = `Hello ${kWord} world`;

type UiaDoc = { name: string; text: string; selection: string };

// Polls inside one PowerShell run: ng collects its accessibility tree on the
// frame after the first UIA request.
function readUiaDocument(frame: number, wantSelection: boolean): UiaDoc {
  const script = `
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
$A = [System.Windows.Automation.AutomationElement]
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
      $out = @{ name = $doc.Current.Name; text = $text; selection = $sel }
      if ($text.Contains('${kWord}') -and (-not ${wantSelection ? "$true" : "$false"} -or $sel.Length -gt 0)) { break }
    }
  } catch { $out = @{ error = $_.Exception.Message } }
  Start-Sleep -Milliseconds 250
}
if (-not $out) { $out = @{ error = 'no Document element' } }
$out | ConvertTo-Json -Compress
`;
  const r = runPowerShell(script);
  const line = r.out.trim().split(/\r?\n/).pop() ?? "";
  let res: UiaDoc & { error?: string };
  try {
    res = JSON.parse(line);
  } catch {
    throw new Error(`uia-document-text: bad PowerShell output:\n${r.out}`);
  }
  if (res.error) {
    throw new Error(`uia-document-text: ${res.error}`);
  }
  return { name: res.name ?? "", text: res.text ?? "", selection: res.selection ?? "" };
}

export async function testit(): Promise<void> {
  if (!IS_WIN) {
    console.log("uia-document-text: UI Automation is Windows-only, skipping");
    return;
  }
  const pdf = tmpPath("uia-document-text.pdf");
  writeFileSync(pdf, makeTextPdf(kLine));

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

    await client.seedTextSelection(1);
    const after = readUiaDocument(frame, true);
    if (!after.selection.startsWith(`Hello ${kWord}`) || !kLine.startsWith(after.selection.trimEnd())) {
      throw new Error(`uia-document-text: selection ${JSON.stringify(after.selection)}, expected a prefix of ${kLine}`);
    }
    console.log(
      `uia-document-text: ok, text ${JSON.stringify(before.text)} selection ${JSON.stringify(after.selection)}`,
    );
  } finally {
    client.close();
    try {
      await killAndWait(proc);
    } catch {
      // already exited
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
