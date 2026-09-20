# Launch the demo and capture its window to verify 3D rendering.
# NOTE: keep this file ASCII-only -- Windows PowerShell 5.1 mis-decodes
#       BOM-less UTF-8 scripts containing non-ASCII characters.
param(
    [int]$WaitSeconds = 12,
    [string]$OutFile = ""
)

$ErrorActionPreference = 'Continue'
$run = "C:\Users\yuguilin\Desktop\gitlab\CGX-TP\CGX-TP\demo\run"
if ([string]::IsNullOrEmpty($OutFile)) { $OutFile = Join-Path $run "demo_shot.png" }

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class W32 {
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

Get-Process robot3ddemo -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500

$p = Start-Process -FilePath (Join-Path $run "robot3ddemo.exe") `
                   -WorkingDirectory $run -PassThru
Write-Output ("Started PID=" + $p.Id + ", waiting " + $WaitSeconds + "s ...")
Start-Sleep -Seconds $WaitSeconds

$p.Refresh()
if ($p.HasExited) {
    Write-Output ("RESULT: process exited, ExitCode=" + $p.ExitCode)
    exit 1
}
Write-Output ("RESULT: process alive, WorkingSet=" + [math]::Round($p.WorkingSet64/1MB,1) + " MB")

$hwnd = $p.MainWindowHandle
Write-Output ("MainWindowHandle=" + $hwnd + "  Title='" + $p.MainWindowTitle + "'")
if ($hwnd -eq [IntPtr]::Zero) {
    Write-Output "WARN: no main window handle, cannot capture"
    exit 2
}

[void][W32]::ShowWindow($hwnd, 5)
[void][W32]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds 800

$rc = New-Object W32+RECT
[void][W32]::GetWindowRect($hwnd, [ref]$rc)
$w = $rc.Right - $rc.Left
$h = $rc.Bottom - $rc.Top
Write-Output ("Window size: " + $w + "x" + $h)

Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap($w, $h)
$gfx = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $gfx.GetHdc()
$ok = [W32]::PrintWindow($hwnd, $hdc, 2)
$gfx.ReleaseHdc($hdc)
$gfx.Dispose()
Write-Output ("PrintWindow=" + $ok)
$bmp.Save($OutFile, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output ("Saved screenshot: " + $OutFile)
Write-Output ("PID=" + $p.Id + " still running")
