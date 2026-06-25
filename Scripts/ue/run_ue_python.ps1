<#
    run_ue_python.ps1 -- run an Unreal Python script headlessly against the
    SatisfactoryModLoader uproject (which mounts the LaserRifleMod plugin).

    The CSS engine ships PythonScriptPlugin (Experimental) + a bundled Python
    3.11 but the plugin is NOT enabled in the uproject, so we enable it on the
    command line. Game must be closed and no build running.

    Usage:
        .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\probe.py
#>
param(
    [Parameter(Mandatory = $true)][string]$Script
)
$ErrorActionPreference = "Stop"

$Engine   = "C:\Program Files\Unreal Engine - CSS"
$Cmd      = Join-Path $Engine "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Uproject = "C:\Claude\Projects\SatisfactoryModLoader\FactoryGame.uproject"
$ScriptFull = (Resolve-Path $Script).Path
# UE -ExecutePythonScript eats backslash escapes (e.g. \u in \ue\) -> pass fwd slashes.
$ScriptFwd  = $ScriptFull -replace '\\','/'
$LogFile  = Join-Path (Split-Path $ScriptFull) ("ue-python-" + [IO.Path]::GetFileNameWithoutExtension($ScriptFull) + ".log")

# Refuse if a build or the game is running (shares the editor/toolchain).
$busy = Get-Process -Name "UnrealBuildTool","AutomationTool","FactoryGame*","UnrealEditor" -ErrorAction SilentlyContinue
if ($busy) {
    Write-Host "BLOCKED: build/game/editor running:"
    $busy | Select-Object -ExpandProperty ProcessName -Unique | ForEach-Object { Write-Host "  - $_" }
    exit 1
}

if (-not (Test-Path $Cmd))      { throw "UnrealEditor-Cmd.exe not found: $Cmd" }
if (-not (Test-Path $Uproject)) { throw "uproject not found: $Uproject" }
if (-not (Test-Path $ScriptFull)) { throw "script not found: $ScriptFull" }

Write-Host "Running UE python: $ScriptFull"
Write-Host "Log: $LogFile"

$ueArgs = @(
    "`"$Uproject`"",
    "-ExecutePythonScript=`"$ScriptFwd`"",
    "-EnablePlugins=PythonScriptPlugin",
    "-unattended", "-nopause", "-nosplash", "-nullrhi",
    "-stdout", "-FullStdOutLogOutput", "-NoLogTimes", "-utf8output"
)

& $Cmd @ueArgs 2>&1 | Tee-Object -FilePath $LogFile
$code = $LASTEXITCODE
Write-Host "--- UnrealEditor-Cmd exit code: $code ---"

# Surface the probe/script's own LR_ lines for quick reading.
Write-Host "=== LR_ lines ==="
Select-String -Path $LogFile -Pattern "LR_" -SimpleMatch | ForEach-Object { $_.Line }
exit $code
