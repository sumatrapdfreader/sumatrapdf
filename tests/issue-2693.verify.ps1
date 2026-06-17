# Helper for tests/issue-2693.ts. Verifies that middle-click auto-scroll moves
# the document with a *fractional* speed (sub-pixel accumulation), which is the
# core of the smoothness fix.
#
# Trick: middle-click auto-scroll speed is (cursorOffset / 10) pixels per 20ms.
# With a cursor offset of 9, the OLD integer code computes 9/10 = 0 -> the
# document doesn't move at all; the NEW float code computes 0.9 and accumulates
# it, so the document keeps scrolling. We first do a large move to get past the
# drag threshold and into scrolling mode, then settle at offset 9 and measure
# how far the document scrolls during a fixed window.
#
# Driving by injected input doesn't work on this machine, but PostMessage and
# cross-process GetScrollInfo do (see project memory env-gui-automation).
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
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int left, top, right, bottom; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct SI { public uint cbSize; public uint fMask; public int nMin; public int nMax; public uint nPage; public int nPos; public int nTrackPos; }
  [DllImport("user32.dll")] public static extern bool GetScrollInfo(IntPtr h, int bar, ref W.SI si);
}
"@

$WM_MOUSEMOVE   = 0x0200
$WM_MBUTTONDOWN = 0x0207
$MK_MBUTTON     = 0x0010

function VPos([IntPtr]$canvas) {
  $si = New-Object W+SI
  $si.cbSize = [System.Runtime.InteropServices.Marshal]::SizeOf([type]([W+SI]))
  $si.fMask = 0x17  # SIF_ALL
  [W]::GetScrollInfo($canvas, 1, [ref]$si) | Out-Null   # SB_VERT
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

# enter auto-scroll mode, move well past the drag threshold (offset 50 -> speed 5)
[W]::PostMessageW($canvas, $WM_MBUTTONDOWN, [IntPtr]$MK_MBUTTON, (PackLp $cx $cy)) | Out-Null
Start-Sleep -Milliseconds 50
[W]::PostMessageW($canvas, $WM_MOUSEMOVE, [IntPtr]0, (PackLp $cx ($cy + 50))) | Out-Null
Start-Sleep -Milliseconds 400

# settle at offset 9: old code -> speed 0 (no move), new code -> speed 0.9
[W]::PostMessageW($canvas, $WM_MOUSEMOVE, [IntPtr]0, (PackLp $cx ($cy + 9))) | Out-Null
Start-Sleep -Milliseconds 200
$p0 = VPos $canvas
Start-Sleep -Milliseconds 1500
$p1 = VPos $canvas

# stop auto-scroll (second middle-click toggles it off)
[W]::PostMessageW($canvas, $WM_MBUTTONDOWN, [IntPtr]$MK_MBUTTON, (PackLp $cx ($cy + 9))) | Out-Null

Write-Output ("RESULT fracStart={0} fracEnd={1}" -f $p0, $p1)
try { $proc.Kill() } catch {}
