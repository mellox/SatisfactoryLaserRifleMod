# Laser Rifle

A ten-tier laser rifle mod for Satisfactory with unique designs, progressive research, and customizable gameplay.

## Features

- **10 Tiers (Mk1–Mk10)**: Craftable rifle variants with exclusive body models, each unlocked through MAM research. Damage scaling follows a tunable curve (default: 1+MkBump)^Mk.

- **Per-Mk Body Designs**: Each tier features a procedurally-generated unique rifle body with distinct silhouettes and detail levels (Mk1: cobbled/salvage aesthetic; Mk2–5: industrial variants; Mk6–10: energy/futuristic designs). 3D models generated via Tripo API.

- **Beam Appearance**: Hitscan laser beam with per-tier color progression—amber/warm at Mk1, transitioning through the spectrum to violet and white at higher tiers. Beam intensity increases with tier and heat.

- **Visual Effects (Per-Mk)**:
  - Electrical sparks: Tier-colored radial burst tendrils emanating from the muzzle (amber → violet) during fire and heat buildup
  - Electric arcs: Branching bolts (crackle) that intensify with heat
  - Ground jump: Lightning bolt to nearby terrain when overheating
  - Impact splash: Arc tendrils at beam hit location (Mk4+)
  - Plasma glow and heat-driven smoke

- **First-Person Viewmodel**: Held orientation optimized per-tier for visual alignment with the crosshair. Muzzle position tuned so the beam emerges correctly relative to the barrel.

- **Heat & Overheat Mechanic**: 
  - Soft overheat threshold ~0.5 cell (configurable)
  - Hard overheat at 1.0 cell (blocks firing; reload/idle cools)
  - Heat builds during sustained fire, cools during reload and idle
  - Low tiers cool faster than high tiers
  - Overheat severity tunable in session settings

- **Energy Cell Fuel System**:
  - Rifle reloads by consuming the player's highest-portion fuel (extensible table: Energy Cell ~3 portions, Battery ~8 portions)
  - Energy Cell: Mod-owned craftable item, unlocked with Mk1 rifle research
  - Recipe: 5 Wire + 2 Quartz → 1 Energy Cell (2s in Craft Bench)
  - Magazine size: 5–30 shots (Mk1–Mk10)
  - HUD displays current shots, spare fuel count, and pack capacity

- **Custom Crosshair HUD**: First-person reticle showing ammunition status, heat level, and spare fuel. Scales and updates in real time.

- **In-Game Settings** (Session Configuration):
  - Heat generation and cooldown speed (per-tier scaling)
  - Damage multiplier and per-tier bump
  - Visual effect intensities (spark/arc sliders)
  - Fire slowdown and overheat penalties
  - Random loadout component (WIP/dev option)

## Installation

### Via Satisfactory Mod Manager (SMM) or ficsit.app
1. Open **Satisfactory Mod Manager** or visit **ficsit.app**
2. Search for **Laser Rifle**
3. Click **Install** and let SMM handle deployment

The mod will be automatically placed in your Satisfactory `Mods` folder.

### Manual Installation
1. Download the latest release `.zip`
2. Extract to `C:\Program Files (x86)\Steam\steamapps\common\Satisfactory\FactoryGame\Mods\LaserRifleMod`
3. Launch Satisfactory; the mod will load on startup (check the log for `===== LaserRifleMod BUILD ... LOADED =====`)

**Requirements**: Satisfactory ≥ Update 8 (game version ≥ 491125), SML 3.12+

## Building from Source

This mod is built using the **Satisfactory Modding Loader (SML)** and Unreal Engine 5.4 (CSS fork).

### Prerequisites
- **Engine**: CSS Unreal Engine 5.4 fork (`C:\Program Files\Unreal Engine - CSS`)
- **SML**: Project at `C:\Claude\Projects\SatisfactoryModLoader` (FactoryGame.uproject, SML 3.x)
- **Source**: This repository (`C:\Claude\Projects\SatisfactoryLaserRifleMod`)

### Build Steps

1. **Sync the source tree** to the SML Mods directory:
   ```powershell
   robocopy "C:\Claude\Projects\SatisfactoryLaserRifleMod\Source" `
     "C:\Claude\Projects\SatisfactoryModLoader\Mods\LaserRifleMod\Source" /MIR /NFL /NDL /NJH /NJS /NP
   robocopy "C:\Claude\Projects\SatisfactoryLaserRifleMod\Config" `
     "C:\Claude\Projects\SatisfactoryModLoader\Mods\LaserRifleMod\Config" /MIR /NFL /NDL /NJH /NJS /NP
   ```

2. **Hold the build lock** (check `C:\Claude\Projects\_team\BUILD-COORDINATION.md` for cross-session serialization)

3. **Close Satisfactory** (the game locks the deployed DLL)

4. **Run the packager** (UAT via the CSS engine):
   ```powershell
   & "C:\Program Files\Unreal Engine - CSS\Engine\Build\BatchFiles\RunUAT.bat" `
     -ScriptsForProject="C:\Claude\Projects\SatisfactoryModLoader\FactoryGame.uproject" PackagePlugin `
     -project="C:\Claude\Projects\SatisfactoryModLoader\FactoryGame.uproject" `
     -clientconfig=Shipping -serverconfig=Shipping -utf8output -DLCName=LaserRifleMod -build -platform=Win64 `
     -nocompileeditor -installed `
     -CopyToGameDirectory_Windows="C:\Program Files (x86)\Steam\steamapps\common\Satisfactory"
   ```

5. **Verify the build marker** embedded in the deployed DLL:
   ```powershell
   # The DLL should contain a timestamped marker from LaserRifleSubsystem::StartupModule
   ```

6. **Release the build lock** and test in-game

### Build Gotchas

- **Two Source Trees**: Always sync `Source/` and `Config/` from the repo to `SatisfactoryModLoader\Mods\LaserRifleMod` before building (UAT compiles the Mods copy, not the repo).
- **WeaponUpgrades Conflict** (if present): Temporarily rename `Mods\WeaponUpgrades\WeaponUpgrades.uplugin` → `.disabled_for_build` during the build, then restore. This works around a UHT false-positive "unused friend" warning that blocks the shared FactoryEditor target.

## How It Works

### Research & Progression
- **MAM Tree**: Mk1 rifle (5 Reinforced Iron Plates + 40 Wire + 10 Cable + 10 Quartz) unlocks the Mk1 rifle and Energy Cell recipe simultaneously
- **Tier Unlocks**: Each Mk2–10 rifle is researched separately and crafted via Equipment Workshop (cost: 1× previous tier + scaled materials)
- **No Base Rifle**: The ten tiers form a linear progression; there is no untiered "Laser Rifle" item

### Damage Model
- Damage scales per tier: **Base × (1 + MkBump)^Mk**, where MkBump is player-configurable (default ~0.25)
- No ammunition depletion—damage per shot is consistent until the battery/cell empties
- Headshots and creature type modifiers follow Satisfactory's standard damage pipeline

### Firing & Reload
1. **Equip** a rifle tier (e.g., Mk5)
2. **Fire** (left-click): Hitscan beam traces to crosshair; consumes 1 shot from the current magazine
3. **Magazine**: Starts at Mk-scaled shots (5→30); depletes with sustained fire
4. **Heat**: Builds during rapid fire; cool via reload or idle
5. **Reload** (R-key): When magazine empties or manually triggered, rifle consumes 1 Energy Cell from inventory (or Battery if no cells), refilling the magazine
6. **Overheat**: If heat exceeds threshold before reload completes, firing is blocked until cooled (reload/idle)

### Settings & Console Variables

**Session Settings** (in-game mod menu):
- Heat: Shots to Overheat (Mk1 base, scales to Mk10)
- Heat: Cooldown Speed (multiplier)
- Damage: Mk Bump (scaling exponent)
- FX: Spark/Arc Intensity sliders
- Heat: Fire Slowdown penalty at 2× overheat (WIP)

**Console Variables** (live tuning, not persisted):
- `lr.HoldPitch / lr.HoldYaw / lr.HoldRoll`: Adjust visual aim offset
- `lr.MuzzleDX / lr.MuzzleDY / lr.MuzzleDZ`: Nudge beam origin (cm)
- `lr.BatteryEnable`: Toggle dangling battery cosmetic (default 0 = hidden)
- `lr.SparkAmount`: Override spark/arc intensity (0–3)
- `BEAMDIAG`: Log beam tracing for debugging aim issues

## Credits

- **3D Models**: Generated via Tripo AI (v3.1, detailed PBR textures)
- **UE5 Implementation**: C++ weapon, effects, and crosshair HUD
- **Audio**: Wwise integration (laser fire, overheat warning)
- **Playtesting & Direction**: In-game tuning feedback and hold-orientation refinement

## License

This mod is provided as-is for the Satisfactory community. See the included LICENSE file for terms.

## Support

For issues, feedback, or feature requests, please check the mod's page on **ficsit.app** or open an issue on GitHub.

---

**Mod Version**: 0.1.0 | **Game**: Satisfactory Update 8+ | **SML**: 3.12+
