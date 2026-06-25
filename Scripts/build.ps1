<#
    LaserRifleMod — build + deploy (run on Windows, game CLOSED).

    Implements the cross-session build protocol from _team/BUILD-COORDINATION.md:
      1. refuse to build if another build/the game is running, or the lock is held
      2. acquire the lock
      3. sync repo -> SatisfactoryModLoader\Mods (the tree UBT actually compiles)
      4. build the FactoryEditor (Development) module  <-- needed so the cooker's
         editor can LOAD the plugin; a new C++ module has no editor binary yet
      5. RunUAT PackagePlugin (Shipping, Win64) and copy into the game
      6. verify the build marker is embedded in the deployed DLL
      7. release the lock (always)

    All output is mirrored to .\last-build.log (gitignored) so it can be reviewed
    after the run.

    Usage (PowerShell):
        cd C:\Claude\Projects\SatisfactoryLaserRifleMod
        Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force   # once per shell
        .\Scripts\build.ps1
        .\Scripts\build.ps1 -MarkerTag 2026-06-21-scaffold-1
#>

param(
    [string]$MarkerTag = "2026-06-21-core-25"
)

$ErrorActionPreference = "Stop"

# --- Paths (this machine; see _team/playbooks/satisfactory-mod.md) -------------
$ModName   = "LaserRifleMod"
$Repo      = "C:\Claude\Projects\SatisfactoryLaserRifleMod"
$SmlProj   = "C:\Claude\Projects\SatisfactoryModLoader"
$Uproject  = Join-Path $SmlProj "FactoryGame.uproject"
$ModsDir   = Join-Path $SmlProj ("Mods\" + $ModName)
$EngineDir = "C:\Program Files\Unreal Engine - CSS"
$BuildBat  = Join-Path $EngineDir "Engine\Build\BatchFiles\Build.bat"
$RunUAT    = Join-Path $EngineDir "Engine\Build\BatchFiles\RunUAT.bat"
$GameDir   = "C:\Program Files (x86)\Steam\steamapps\common\Satisfactory"
$Lock      = "C:\Claude\Projects\_team\BUILD-LOCK.txt"
$DeployDll = Join-Path $GameDir ("FactoryGame\Mods\{0}\Binaries\Win64\FactoryGameSteam-{0}-Win64-Shipping.dll" -f $ModName)
$EditorDll = Join-Path $ModsDir ("Binaries\Win64\UnrealEditor-{0}.dll" -f $ModName)
$Marker    = "$ModName BUILD $MarkerTag LOADED"
$LogFile   = Join-Path $Repo "last-build.log"

# --- Logging helper: write to console AND .\last-build.log -------------------
"=== LaserRifleMod build $(Get-Date -Format o) | marker tag: $MarkerTag ===" | Set-Content $LogFile
function Log($msg) { Write-Host $msg; Add-Content -Path $LogFile -Value $msg }

# --- 1. Refuse to build if busy ----------------------------------------------
$busy = Get-Process -Name "UnrealBuildTool","AutomationTool","cl","link","MSBuild","UnrealEditor*","FactoryGame*" -ErrorAction SilentlyContinue
if ($busy) {
    Log "BLOCKED: a build or the game is running (close Satisfactory / wait for the other build):"
    $busy | Select-Object -ExpandProperty ProcessName -Unique | ForEach-Object { Log "  - $_" }
    exit 1
}
$held = (Test-Path $Lock) -and (((Get-Date) - (Get-Item $Lock).LastWriteTime).TotalMinutes -lt 30)
if ($held) {
    Log "BLOCKED: build lock held by another session (<30 min old): $Lock"
    Get-Content $Lock | ForEach-Object { Log "  $_" }
    exit 1
}

# --- 2. Acquire lock ----------------------------------------------------------
"$ModName | pid=$PID | $(Get-Date -Format o)" | Set-Content $Lock
Log "Lock acquired: $Lock"

try {
    # --- 3. Sync repo -> Mods (UBT compiles the COPY, not the repo) -----------
    Log "Syncing Source -> Mods ..."
    robocopy "$Repo\Source" "$ModsDir\Source" /MIR /NFL /NDL /NJH /NJS /NP 2>&1 | Tee-Object -FilePath $LogFile -Append | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy Source failed (exit $LASTEXITCODE)" }

    Log "Syncing Config -> Mods ..."
    robocopy "$Repo\Config" "$ModsDir\Config" /MIR /NFL /NDL /NJH /NJS /NP 2>&1 | Tee-Object -FilePath $LogFile -Append | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy Config failed (exit $LASTEXITCODE)" }

    Copy-Item (Join-Path $Repo "$ModName.uplugin") (Join-Path $ModsDir "$ModName.uplugin") -Force

    # --- 4. Build the editor module (Development) -----------------------------
    # The cook step launches the editor, which must LOAD our plugin. A brand-new
    # C++ module has no UnrealEditor-<Mod>.dll yet, so cook fails with
    # "module could not be found". This builds it (incremental after the first run).
    Log "Building FactoryEditor (Development) so the cooker can load the module ..."
    & $BuildBat FactoryEditor Win64 Development -Project="$Uproject" -WaitMutex 2>&1 | Tee-Object -FilePath $LogFile -Append
    if ($LASTEXITCODE -ne 0) { throw "FactoryEditor module build failed (exit $LASTEXITCODE)" }
    if (-not (Test-Path $EditorDll)) { throw "Editor module DLL not produced: $EditorDll" }
    Log "Editor module built: $EditorDll"

    # --- 5. Build + package (PackagePlugin) ----------------------------------
    Log "Packaging $ModName (RunUAT PackagePlugin) ..."
    $uatArgs = @(
        "-ScriptsForProject=$Uproject", "PackagePlugin",
        "-project=$Uproject",
        "-clientconfig=Shipping", "-serverconfig=Shipping",
        "-utf8output", "-DLCName=$ModName", "-build", "-platform=Win64",
        "-nocompileeditor", "-installed",
        "-CopyToGameDirectory_Windows=$GameDir"
    )
    & $RunUAT @uatArgs 2>&1 | Tee-Object -FilePath $LogFile -Append
    if ($LASTEXITCODE -ne 0) { throw "RunUAT failed (exit $LASTEXITCODE)" }

    # --- 6. Verify the build marker in the deployed DLL (UTF-16) --------------
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
        Log "BUILD OK - marker verified in DLL: '$Marker'"
        Log "Deployed: $DeployDll"
    } else {
        throw "Marker NOT found in DLL - deployed binary is stale or not ours. Expected: '$Marker'"
    }
}
catch {
    Log "BUILD FAILED: $($_.Exception.Message)"
    throw
}
finally {
    if (Test-Path $Lock) { [System.IO.File]::Delete($Lock) }
    Log "Lock released."
    Log "Full log: $LogFile"
}
