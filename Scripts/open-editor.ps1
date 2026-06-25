<#  Opens the Satisfactory modding project in the CSS Unreal editor.
    First launch can take 10-30 min (shader compile) - this is normal.  #>
$ErrorActionPreference = "Stop"
$Editor  = "C:\Program Files\Unreal Engine - CSS\Engine\Binaries\Win64\UnrealEditor.exe"
$Project = "C:\Claude\Projects\SatisfactoryModLoader\FactoryGame.uproject"

if (-not (Test-Path $Editor))  { Write-Host "CSS editor not found at: $Editor" -ForegroundColor Red; exit 1 }
if (-not (Test-Path $Project)) { Write-Host "Project not found at: $Project" -ForegroundColor Red; exit 1 }

# Don't open the editor while a build is running (they share the project).
$busy = Get-Process -Name "UnrealBuildTool","AutomationTool","cl","link","MSBuild" -ErrorAction SilentlyContinue
if ($busy) { Write-Host "A build is running - wait for it to finish, then re-run this." -ForegroundColor Red; exit 1 }

Write-Host "Launching the CSS editor... first load compiles shaders and can take 10-30 min." -ForegroundColor Cyan
Write-Host "Watch the bottom-right 'Compiling Shaders' counter; the project is ready when it hits 0." -ForegroundColor Cyan
& $Editor $Project
