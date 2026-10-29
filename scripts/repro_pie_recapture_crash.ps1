# Repro: PIE Shift+F1 release -> click viewport (minCraft feedback).
# Drives MinCraftEditor with SendInput; optionally attaches gdb for a live backtrace.
param(
    [string]$Editor = "D:\Dev\GitRepo\minCraft-render\bin\MinCraftEditor.exe",
    [string]$Project = "D:\Dev\GitRepo\minCraft-render\minCraft\minCraft.meproject",
    [switch]$WithGdb,
    [int]$BootWaitSec = 25,
    [int]$PlayWaitSec = 8,
    [int]$AfterReleaseWaitMs = 800
)

$ErrorActionPreference = "Stop"
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class NativeInput {
    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT {
        public uint type;
        public INPUTUNION U;
    }
    [StructLayout(LayoutKind.Explicit)]
    public struct INPUTUNION {
        [FieldOffset(0)] public MOUSEINPUT mi;
        [FieldOffset(0)] public KEYBDINPUT ki;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct MOUSEINPUT {
        public int dx, dy;
        public uint mouseData, dwFlags, time;
        public IntPtr dwExtraInfo;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct KEYBDINPUT {
        public ushort wVk, wScan;
        public uint dwFlags, time;
        public IntPtr dwExtraInfo;
    }
    [DllImport("user32.dll", SetLastError=true)]
    public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    public const uint INPUT_MOUSE = 0, INPUT_KEYBOARD = 1;
    public const uint KEYEVENTF_KEYUP = 0x0002;
    public const uint MOUSEEVENTF_LEFTDOWN = 0x0002, MOUSEEVENTF_LEFTUP = 0x0004;
    public static void Key(ushort vk, bool up) {
        INPUT[] i = new INPUT[1];
        i[0].type = INPUT_KEYBOARD;
        i[0].U.ki.wVk = vk;
        i[0].U.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0u;
        SendInput(1, i, Marshal.SizeOf<INPUT>());
    }
    public static void Click() {
        INPUT[] i = new INPUT[2];
        i[0].type = INPUT_MOUSE; i[0].U.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        i[1].type = INPUT_MOUSE; i[1].U.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(2, i, Marshal.SizeOf<INPUT>());
    }
}
"@

function Find-EditorHwnd([int]$ProcessId) {
    Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes -ErrorAction SilentlyContinue
    $procs = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    if (-not $procs) { return [IntPtr]::Zero }
    return $procs.MainWindowHandle
}

Write-Host "Launching $Editor --project $Project"
$workDir = Split-Path $Editor -Parent
$psi = @{
    FilePath = $Editor
    ArgumentList = @("--project", $Project)
    WorkingDirectory = $workDir
    PassThru = $true
}
$proc = Start-Process @psi
Write-Host "PID=$($proc.Id); waiting ${BootWaitSec}s for window..."

$hwnd = [IntPtr]::Zero
$deadline = [datetime]::UtcNow.AddSeconds($BootWaitSec)
while ([datetime]::UtcNow -lt $deadline) {
    Start-Sleep -Milliseconds 500
    if ($proc.HasExited) {
        Write-Host "Editor exited early code=$($proc.ExitCode)"
        exit 3
    }
    $hwnd = Find-EditorHwnd $proc.Id
    if ($hwnd -ne [IntPtr]::Zero) { break }
}
if ($hwnd -eq [IntPtr]::Zero) {
    Write-Host "No MainWindowHandle yet; continuing with process anyway"
    $hwnd = (Get-Process -Id $proc.Id).MainWindowHandle
}

[NativeInput]::ShowWindow($hwnd, 5) | Out-Null
[NativeInput]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 500

Write-Host "F5 EnterPlay"
[NativeInput]::Key(0x74, $false) # VK_F5
Start-Sleep -Milliseconds 50
[NativeInput]::Key(0x74, $true)
Start-Sleep -Seconds $PlayWaitSec

if ($proc.HasExited) {
    Write-Host "Crashed during/after Play exit=$($proc.ExitCode)"
    exit 1
}

Write-Host "Shift+F1 release capture"
[NativeInput]::Key(0x10, $false) # VK_SHIFT
Start-Sleep -Milliseconds 30
[NativeInput]::Key(0x70, $false) # VK_F1
Start-Sleep -Milliseconds 50
[NativeInput]::Key(0x70, $true)
[NativeInput]::Key(0x10, $true)
Start-Sleep -Milliseconds $AfterReleaseWaitMs

$rect = New-Object NativeInput+RECT
if ([NativeInput]::GetWindowRect($hwnd, [ref]$rect)) {
    $cx = [int](($rect.Left + $rect.Right) / 2)
    $cy = [int](($rect.Top + $rect.Bottom) / 2)
    # Bias toward typical Scene viewport (left-center), not dead window center chrome.
    $cx = [int]($rect.Left + ($rect.Right - $rect.Left) * 0.45)
    $cy = [int]($rect.Top + ($rect.Bottom - $rect.Top) * 0.55)
    Write-Host "Click viewport-ish at $cx,$cy"
    [NativeInput]::SetCursorPos($cx, $cy) | Out-Null
    Start-Sleep -Milliseconds 100
    [NativeInput]::Click()
    Start-Sleep -Milliseconds 100
    [NativeInput]::Click()
}

Start-Sleep -Seconds 3
if ($proc.HasExited) {
    Write-Host "CRASH after recapture click exit=$($proc.ExitCode)"
    Get-Content (Join-Path $workDir "ed_crash.log") -Tail 40 -ErrorAction SilentlyContinue
    exit 1
}

Write-Host "Still alive PID=$($proc.Id). Leaving process running for manual inspect (kill yourself)."
Write-Host "AliveAfterRecapture=1"
exit 0
