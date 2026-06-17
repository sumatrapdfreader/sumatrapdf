# Helper for tests/cmd-start-autoscroll.ts. Verifies CmdStartAutoScroll triggers
# middle-click-style auto-scroll without a middle button: it anchors at the
# current cursor position, so we place the real cursor over the canvas, invoke
# the command via WM_COMMAND, then move the cursor offset to set a scroll speed
# and check the document scrolls.
#
# SetCursorPos / PostMessage / cross-process GetScrollInfo all work here; injected
# button input does not (see project memory env-gui-automation).
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
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int ht, bool repaint);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int x, y; }
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int left, top, right, bottom; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct SI { public uint cbSize; public uint fMask; public int nMin; public int nMax; public uint nPage; public int nPos; public int nTrackPos; }
  [DllImport("user32.dll")] public static extern bool GetScrollInfo(IntPtr h, int bar, ref W.SI si);
}
"@

$WM_MOUSEMOVE = 0x0200
$WM_COMMAND   = 0x0111
$CmdStartAutoScroll = 434

function VPos([IntPtr]$canvas) {
  $si = New-Object W+SI
  $si.cbSize = [System.Runtime.InteropServices.Marshal]::SizeOf([type]([W+SI]))
  $si.fMask = 0x17
  [W]::GetScrollInfo($canvas, 1, [ref]$si) | Out-Null
  return $si.nPos
}
function PackLp([int]$x, [int]$y) { return [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF)) }

Get-Process SumatraPDF-dll -ErrorAction SilentlyContinue | ForEach-Object { try { $_.Kill() } catch {} }
Start-Sleep -Milliseconds 300
$proc = Start-Process -FilePath $Exe -ArgumentList @("-for-testing", "-appdata", $AppData, $Pdf) -PassThru
$wantPid = [uint32]$proc.Id

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
function FindCanvas([IntPtr]$frame) {
  $script:canvas = [IntPtr]::Zero
  $cb = [W+EnumProc] { param($h, $l)
    $sb = New-Object Text.StringBuilder 256; [W]::GetClassNameW($h, $sb, 256) | Out-Null
    if ($sb.ToString() -eq "SUMATRA_PDF_CANVAS") { $script:canvas = $h; return $false }
    return $true }
  [W]::EnumChildWindows($frame, $cb, [IntPtr]::Zero) | Out-Null
  return $script:canvas
}

$frame = [IntPtr]::Zero
for ($i = 0; $i -lt 60; $i++) { $frame = FindFrame; if ($frame -ne [IntPtr]::Zero) { break }; Start-Sleep -Milliseconds 200 }
if ($frame -eq [IntPtr]::Zero) { Write-Output "ERROR no-frame"; $proc.Kill(); exit 1 }

[W]::ShowWindow($frame, 9) | Out-Null
[W]::MoveWindow($frame, 0, 0, 900, 750, $true) | Out-Null
Start-Sleep -Milliseconds 1200

$canvas = [IntPtr]::Zero
for ($i = 0; $i -lt 30; $i++) { $canvas = FindCanvas $frame; if ($canvas -ne [IntPtr]::Zero) { break }; Start-Sleep -Milliseconds 200 }
if ($canvas -eq [IntPtr]::Zero) { Write-Output "ERROR no-canvas"; $proc.Kill(); exit 1 }

$rc = New-Object W+RECT
[W]::GetClientRect($canvas, [ref]$rc) | Out-Null
$cx = [int](($rc.right - $rc.left) / 2)
$cy = [int](($rc.bottom - $rc.top) / 2)

# put the real cursor over the canvas center -- the command anchors there
$pt = New-Object W+POINT; $pt.x = $cx; $pt.y = $cy
[W]::ClientToScreen($canvas, [ref]$pt) | Out-Null
[W]::SetCursorPos($pt.x, $pt.y) | Out-Null
Start-Sleep -Milliseconds 150

# invoke CmdStartAutoScroll (no middle button involved)
[W]::PostMessageW($frame, $WM_COMMAND, [IntPtr]$CmdStartAutoScroll, [IntPtr]0) | Out-Null
Start-Sleep -Milliseconds 100

# move the cursor offset down from the anchor to set a scroll speed
[W]::PostMessageW($canvas, $WM_MOUSEMOVE, [IntPtr]0, (PackLp $cx ($cy + 80))) | Out-Null
Start-Sleep -Milliseconds 150
$p0 = VPos $canvas
Start-Sleep -Milliseconds 800
$p1 = VPos $canvas

# invoke again to stop (toggles off, like a second middle-click)
[W]::PostMessageW($frame, $WM_COMMAND, [IntPtr]$CmdStartAutoScroll, [IntPtr]0) | Out-Null
Start-Sleep -Milliseconds 300
$p2 = VPos $canvas

Write-Output ("RESULT start={0} mid={1} afterStop={2}" -f $p0, $p1, $p2)
try { $proc.Kill() } catch {}
