<#
    LaserRifleMod — build + deploy (run on Windows, game CLOSED).

    Implements the cross-session build protocol from _team/BUILD-COORDINATION.md:
      1. refuse to build if another build/the game is running, or the lock is held
      2. acquire the lock
      3. sync repo -> SatisfactoryModLoader\Mods (the tree UBT actually compiles)
      4. RunUAT PackagePlugin (Shipping, Win64) and copy into the game
      5. verify the build marker is embedded in the deployed DLL
      6. release the lock (always)

    Usage (PowerShell):
        cd C:\Claude\Projects\SatisfactoryLaserRifleMod
        .\Scripts\build.ps1                       # uses the default marker tag
        .\Scripts\build.ps1 -MarkerTag 2026-06-21-scaffold-1
#>

param(
    [string]$MarkerTag = "2026-06-21-scaffold-1"
)

$ErrorActionPreference = "Stop"

# --- Paths (this machine; see _team/playbooks/satisfactory-mod.md) -------------
$ModName   = "LaserRifleMod"
$Repo      = "C:\Claude\Projects\SatisfactoryLaserRifleMod"
$SmlProj   = "C:\Claude\Projects\SatisfactoryModLoader"
$Uproject  = Join-Path $SmlProj "FactoryGame.uproject"
$ModsDir   = Join-Path $SmlProj ("Mods\" + $ModName)
$RunUAT    = "C:\Program Files\Unreal Engine - CSS\Engine\Build\BatchFiles\RunUAT.bat"
$GameDir   = "C:\Program Files (x86)\Steam\steamapps\common\Satisfactory"
$Lock      = "C:\Claude\Projects\_team\BUILD-LOCK.txt"
$DeployDll = Join-Path $GameDir ("FactoryGame\Mods\{0}\Binaries\Win64\FactoryGameSteam-{0}-Win64-Shipping.dll" -f $ModName)
$Marker    = "$ModName BUILD $MarkerTag LOADED"

# --- 1. Refuse to build if busy ----------------------------------------------
$busy = Get-Process -Name "UnrealBuildTool","AutomationTool","cl","link","MSBuild","FactoryGame*" -ErrorAction SilentlyContinue
if ($busy) {
    Write-Host "BLOCKED: a build or the game is running (close Satisfactory / wait for the other build):" -ForegroundColor Red
    $busy | Select-Object -ExpandProperty ProcessName -Unique | ForEach-Object { Write-Host "  - $_" }
    exit 1
}
$held = (Test-Path $Lock) -and (((Get-Date) - (Get-Item $Lock).LastWriteTime).TotalMinutes -lt 30)
if ($held) {
    Write-Host "BLOCKED: build lock held by another session (<30 min old): $Lock" -ForegroundColor Red
    Get-Content $Lock | ForEach-Object { Write-Host "  $_" }
    exit 1
}

# --- 2. Acquire lock ----------------------------------------------------------
"$ModName | pid=$PID | $(Get-Date -Format o)" | Set-Content $Lock
Write-Host "Lock acquired: $Lock" -ForegroundColor Green

try {
    # --- 3. Sync repo -> Mods (UBT compiles the COPY, not the repo) -----------
    Write-Host "Syncing Source -> Mods ..." -ForegroundColor Cyan
    robocopy "$Repo\Source" "$ModsDir\Source" /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy Source failed (exit $LASTEXITCODE)" }

    Write-Host "Syncing Config -> Mods ..." -ForegroundColor Cyan
    robocopy "$Repo\Config" "$ModsDir\Config" /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy Config failed (exit $LASTEXITCODE)" }

    # Ensure the .uplugin is present in the Mods copy.
    Copy-Item (Join-Path $Repo "$ModName.uplugin") (Join-Path $ModsDir "$ModName.uplugin") -Force

    # --- 4. Build (PackagePlugin) --------------------------------------------
    Write-Host "Building $ModName (RunUAT PackagePlugin) ..." -ForegroundColor Cyan
    $args = @(
        "-ScriptsForProject=$Uproject", "PackagePlugin",
        "-project=$Uproject",
        "-clientconfig=Shipping", "-serverconfig=Shipping",
        "-utf8output", "-DLCName=$ModName", "-build", "-platform=Win64",
        "-nocompileeditor", "-installed",
        "-CopyToGameDirectory_Windows=$GameDir"
    )
    & $RunUAT @args
    if ($LASTEXITCODE -ne 0) { throw "RunUAT failed (exit $LASTEXITCODE)" }

    # --- 5. Verify the build marker in the deployed DLL (UTF-16) --------------
    if (-not (Test-Path $DeployDll)) { throw "Deployed DLL not found: $DeployDll" }
    $bytes  = [System.IO.File]::ReadAllBytes($DeployDll)
    $needle = [System.Text.Encoding]::Unicode.GetBytes($Marker)
    $found  = $false
    $limit  = $bytes.Length - $needle.Length
    for ($i = 0; $i -le $limit; $i++) {
        if ($bytes[$i] -eq $needle[0]) {
            $match = $true
            for ($j = 1; $j -lt $needle.Length; $j++) {
                if ($bytes[$i + $j] -ne $needle[$j]) { $match = $false; break }
            }
            if ($match) { $found = $true; break }
        }
    }
    if ($found) {
        Write-Host "BUILD OK — marker verified in DLL: '$Marker'" -ForegroundColor Green
        Write-Host "Deployed: $DeployDll" -ForegroundColor Green
    } else {
        throw "Marker NOT found in DLL — deployed binary is stale or not ours. Expected: '$Marker'"
    }
}
finally {
    # --- 6. Release lock (NEVER Remove-Item next to a robocopy /MIR) ----------
    if (Test-Path $Lock) { [System.IO.File]::Delete($Lock) }
    Write-Host "Lock released." -ForegroundColor DarkGray
}
