# Helper for tests/issue-1998.ts. Verifies CmdExpandToCurrentPage:
# opens a PDF with a nested table of contents, goes to the last page, collapses
# the TOC tree, invokes the command, and reports how many tree rows are visible
# before vs. after plus whether an item ends up selected.
#
# Driving by injected input doesn't work on this machine, but WM_COMMAND (to
# trigger commands) and cross-process TreeView messages (to read state) do work
# (see project memory env-gui-automation).
param([string]$Exe, [string]$Pdf, [string]$AppData)
$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class W {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc cb, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint msg, IntPtr w, IntPtr l);
}
"@

function Cls([IntPtr]$h) { $sb = New-Object Text.StringBuilder 256; [W]::GetClassNameW($h, $sb, 256) | Out-Null; $sb.ToString() }

# TreeView messages
$TVM_GETNEXTITEM = 0x110A
$TVM_EXPAND      = 0x1102
$TVGN_ROOT       = 0x0
$TVGN_NEXT       = 0x1
$TVGN_CARET      = 0x9
$TVGN_NEXTVISIBLE= 0x6
$TVE_COLLAPSE    = 0x1
$WM_COMMAND      = 0x111

function TvNext([IntPtr]$tree, [int]$flag, [IntPtr]$item) {
  return [W]::SendMessageW($tree, $TVM_GETNEXTITEM, [IntPtr]$flag, $item)
}
function CountVisible([IntPtr]$tree) {
  $n = 0
  $it = TvNext $tree $TVGN_ROOT ([IntPtr]::Zero)
  while ($it -ne [IntPtr]::Zero) {
    $n++
    $it = TvNext $tree $TVGN_NEXTVISIBLE $it
    if ($n -gt 100000) { break }
  }
  return $n
}
function CollapseRoots([IntPtr]$tree) {
  $it = TvNext $tree $TVGN_ROOT ([IntPtr]::Zero)
  while ($it -ne [IntPtr]::Zero) {
    [W]::SendMessageW($tree, $TVM_EXPAND, [IntPtr]$TVE_COLLAPSE, $it) | Out-Null
    $it = TvNext $tree $TVGN_NEXT $it
  }
}

# kill any stale dev-build instances so reuse-instance can't forward our launch
# to an old window (which would leave our process window-less)
Get-Process SumatraPDF-dll -ErrorAction SilentlyContinue | ForEach-Object { try { $_.Kill() } catch {} }
Start-Sleep -Milliseconds 300

$proc = Start-Process -FilePath $Exe -ArgumentList @("-for-testing", "-appdata", $AppData, $Pdf) -PassThru
$wantPid = [uint32]$proc.Id

# note: the EnumWindows/EnumChildWindows callbacks must NOT call PowerShell
# functions (calling back into the runspace from a native callback misbehaves) --
# inline the class-name lookup instead.
function FindFrame {
  $script:frame = [IntPtr]::Zero
  $cb = [W+EnumProc] { param($h, $l)
    $p = [uint32]0; [W]::GetWindowThreadProcessId($h, [ref]$p) | Out-Null
    if ($p -eq $wantPid) {
      $sb = New-Object Text.StringBuilder 256; [W]::GetClassNameW($h, $sb, 256) | Out-Null
      if ($sb.ToString() -eq "SUMATRA_PDF_FRAME") { $script:frame = $h; return $false }
    }
    return $true }
  [W]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
  return $script:frame
}
function FindTree([IntPtr]$frame) {
  $script:tree = [IntPtr]::Zero
  $cb = [W+EnumProc] { param($h, $l)
    $sb = New-Object Text.StringBuilder 256; [W]::GetClassNameW($h, $sb, 256) | Out-Null
    if ($sb.ToString() -eq "SysTreeView32") { $script:tree = $h; return $false }
    return $true }
  [W]::EnumChildWindows($frame, $cb, [IntPtr]::Zero) | Out-Null
  return $script:tree
}

$frame = [IntPtr]::Zero
for ($i = 0; $i -lt 60; $i++) { $frame = FindFrame; if ($frame -ne [IntPtr]::Zero) { break }; Start-Sleep -Milliseconds 200 }
if ($frame -eq [IntPtr]::Zero) { Write-Output "ERROR no-frame"; $proc.Kill(); exit 1 }

# wait for the TOC tree to load with items
$tree = [IntPtr]::Zero
for ($i = 0; $i -lt 60; $i++) {
  $tree = FindTree $frame
  if ($tree -ne [IntPtr]::Zero -and (CountVisible $tree) -gt 0) { break }
  Start-Sleep -Milliseconds 250
}
if ($tree -eq [IntPtr]::Zero) { Write-Output "ERROR no-tree"; $proc.Kill(); exit 1 }

# go to the last page (deep in the document, under nested TOC entries)
[W]::SendMessageW($frame, $WM_COMMAND, [IntPtr]264, [IntPtr]::Zero) | Out-Null   # CmdGoToLastPage
Start-Sleep -Milliseconds 600

# collapse the whole tree, then measure
CollapseRoots $tree
Start-Sleep -Milliseconds 300
$collapsed = CountVisible $tree

# invoke the command under test
[W]::SendMessageW($frame, $WM_COMMAND, [IntPtr]433, [IntPtr]::Zero) | Out-Null   # CmdExpandToCurrentPage
Start-Sleep -Milliseconds 600
$after = CountVisible $tree
$caret = TvNext $tree $TVGN_CARET ([IntPtr]::Zero)
$hasSel = if ($caret -ne [IntPtr]::Zero) { 1 } else { 0 }

Write-Output ("RESULT collapsed={0} after={1} hasSelection={2}" -f $collapsed, $after, $hasSel)
try { $proc.Kill() } catch {}
