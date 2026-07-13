# bump_marker_and_build.ps1 -- DETERMINISTIC: reads the current BUILD marker tag out of
# LaserRifleMod.cpp, increments its trailing numeric suffix, writes it back, then invokes
# build.ps1 with the matching -MarkerTag. Removes the manual "hand-edit the marker string"
# step that caused a false BUILD FAILED on the first kitprobe attempt (marker mismatch,
# not a real build failure).
param(
    [string]$Prefix = "kitprobe"   # bumps e.g. 2026-07-02-kitprobe-4 -> 2026-07-02-kitprobe-5
)
$ErrorActionPreference = "Stop"
$cpp = Join-Path $PSScriptRoot "..\..\Source\LaserRifleMod\Private\LaserRifleMod.cpp"
$cpp = (Resolve-Path $cpp).Path
$content = Get-Content $cpp -Raw

if ($content -notmatch 'BUILD (\d{4}-\d{2}-\d{2})-([a-zA-Z]+)-(\d+) LOADED') {
    throw "Could not find a 'BUILD <date>-<tag>-<n> LOADED' marker in $cpp"
}
$date = $Matches[1]; $tag = $Matches[2]; $n = [int]$Matches[3]
$newN = $n + 1
$newMarker = "$date-$tag-$newN"
$newContent = $content -replace 'BUILD \d{4}-\d{2}-\d{2}-[a-zA-Z]+-\d+ LOADED', "BUILD $newMarker LOADED"
Set-Content -Path $cpp -Value $newContent -NoNewline
Write-Host "Marker bumped: $date-$tag-$n -> $newMarker"

$buildScript = Join-Path $PSScriptRoot "..\build.ps1"
& $buildScript -MarkerTag $newMarker
