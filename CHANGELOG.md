# Changelog

All notable changes to the Satisfactory Laser Rifle Mod are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-07-13

### Added

#### Weapon Progression
- **10 craftable Mk laser rifles** (Mk1 through Mk10) with distinct visual appearances and escalating performance
- **MAM research progression**: Mk1 unlocks the rifle and Energy Cell recipes; subsequent Mk researches unlock upgrades
- **Tier-scaled progression**: Each Mk increases magazine size (5 → 30 shots), battery capacity (4 → 8 portions), overheat tolerance, and cooldown speed via a smooth interpolation curve

#### Weapon Mechanics & Firing
- **Hitscan laser firing** with full first-person viewmodel and held weapon animations
- **Magazine and reload system**: Reload animations scale with Mk (5.8s on Mk1 down to 3.0s on Mk10); fire-to-empty consumes magazine cells
- **Heat and overheat mechanics**: Firing generates heat relative to magazine capacity (soft overheat at ~50% magazine, hard limit at 100%); overheat blocks further firing but allows reload to cool during idle periods
- **Reload scaling**: Swap phase animations (SwapDown / Hold / SwapUp) scale together per-Mk for realistic pacing across tiers
- **Hold-orientation per-Mk**: Each rifle's first-person grip position is tuned for ergonomic aim alignment and muzzle alignment

#### Fuel System
- **Energy Cell** (`Desc_LR_EnergyCell`): A mod-owned craftable battery designed for early-game progression (unlocks with Mk1 research)
- **Multi-fuel support**: Rifle consumes the highest-portion fuel available from an extensible fuel table; currently includes base-game Battery (8 portions) and Energy Cell (3 portions)
- **Fail-safe fuel resolution**: If no fuel classes are found, the system gracefully skips; if no fuel is carried, reload fails with an actionable warning
- **Energy Cell recipe**: 5 Wire + 2 Quartz → 1 Energy Cell at Craft Bench (2s crafting); craftable at Assembler and Equipment Workshop after Mk1 research
- **Extensible fuel table**: Add new fuel items by extending `LR_FUEL_TABLE` in LaserRifleWeapon.cpp; no logic changes required

#### Visual Effects & Presentation

##### Per-Mk Bodies & Beams
- **Distinct rifle bodies**: 10 unique first-person meshes for Mk1–Mk10, each with distinct silhouettes and materials
- **Mk1 custom build**: Themed as a "frankenrifle" (e-waste/cobbled electronics aesthetic) with visible wires and construction details
- **Per-tier beam colors**: Beam color shifts from warm tones at low Mk to cool/violet tones at high Mk, visually representing power progression
- **Beam intensity scaling**: Higher Mk produces brighter, more intense beams

##### Visual Effects & Particles
- **Electrical crackle arcs**: Animated procedural lightning arcs spawn near the muzzle during firing, scaling with heat and fire intensity
- **Floating sparks**: Small jagged electric sparks drift outward from the muzzle and fade; presence and density controlled by session slider
- **Overheat ground jump**: When heat reaches ≥85%, a dramatic electrical arc jumps from the rifle to the ground (line-traced to the world)
- **Impact splash**: Short arcs and sparks erupt at the beam's impact point
- **Per-Mk spark coloration**: Sparks shift from warm amber (Mk1) through mid-spectrum colors to violet (Mk10), tier-matched to beam color
- **Configurable spark slider**: "FX: Electric Sparks" session setting scales all electrical effects (0 = none, 3 = full density)

##### Custom Crosshair HUD
- **First-person crosshair**: Custom 2D crosshair replacing the default weapon reticle, designed for accurate aiming
- **Heat indicator**: Crosshair visually represents current heat level; transitions to red/glowing at overheat
- **Spare ammunition counter**: Displays count of available fuel cells/batteries in player inventory (e.g., "× 5 spare")
- **Ammo display**: Shows current magazine fill level (shots remaining / total shots per magazine)

##### Mk1 Swinging Battery
- **Dangling battery component** (Mk1 only): A visual battery cell hangs from a spring-damper pendulum mount on the rifle body
- **Dynamic motion**: Battery swings when moving and turning; slides out during reload swap and back in when reloaded
- **Spring-damper physics**: Behaves realistically with configurable swing amplitude and damping
- **Configurable placement**: Console CVars allow in-game tuning of battery position (X/Y/Z offsets) and scale

#### Audio
- **Laser firing sound**: Distinct firing audio per-tier (tone/pitch shifts with Mk level)
- **Reload sound**: Audible feedback for swap and reload animations
- **Overheat warning sound**: Audio cue when heat reaches critical levels

#### Session Configuration
- **Player-facing settings** (12 main options, 3 advanced/WIP):
  - **FX: Electric Sparks** (slider, 0–3): Controls spark density and prominence
  - **Heat: Overheat Tolerance** (dial, default 6): Adjusts shots-to-overheat as a multiplier; Mk1 base = 6 shots (tunable)
  - **Heat: Cooldown Speed** (slider): Controls heat dissipation rate when idle (scaled ~1.0× at Mk1 to ~1.5× at Mk10)
  - **Damage: Mk Bump** (slider): Multiplier curve damage scaling across Mk levels (default 1.0 = linear; higher values emphasize late-game tiers)
  - **Freeze** (toggle): Optional gameplay effect
  - **Heat: Fire Slowdown at 2x** (slider, Advanced): Rate penalty during high heat
  - **Heat: Overheat Cooldown Penalty** (slider, Advanced): Recovery penalty factor post-overheat
  - **Random Components** (toggle, Advanced/WIP): Controlled via `lr.RandomComponents` console CVar (default off)

#### Console CVars (Developer Tuning)
- **`lr.HoldPitch`**: First-person grip pitch angle per-Mk
- **`lr.MuzzleDX/DY/DZ`**: Muzzle offset corrections for beam start alignment
- **`lr.BatteryX/Y/Z`**: Mk1 battery mount position offsets (body-local cm)
- **`lr.BatteryScale`**: Mk1 battery visual size multiplier
- **`lr.BatterySwing`**: Amplitude of the dangling battery pendulum motion
- **`lr.BatteryEnable`**: Toggle dangling battery visual (1=on, 0=off; default off, parked for future)
- **`lr.RandomComponents`**: Enable/disable random rifle component variations (default off, WIP feature)
- **`lr.RigEnable`**: Switch Mk1 between rigged-mesh and static-mesh body render (default 0 = static)

#### Safety & Stability
- **Fail-safe fuel class resolution**: If Energy Cell or Battery descriptors fail to load, the rifle falls back to free reload (logged, doesn't brick gameplay)
- **Authority-gated inventory operations**: Reload fuel consumption is server-authoritative to prevent duplication exploits in multiplayer
- **Spore flower damage immunity**: Laser damage properly routes through UFGDamageType to avoid crash on contact with destructible flora
- **Graceful degradation**: Missing meshes, sounds, or optional visual components are skipped cleanly with diagnostic logging

---

## Notes for Players

- **Progression**: Research Mk1 in the MAM to unlock the rifle and Energy Cell recipe. Research subsequent Mk tiers to unlock crafting and equipping higher tiers.
- **Fuel management**: Keep Energy Cells or base-game Batteries in your inventory while carrying the rifle. The highest-portion fuel is consumed on reload.
- **Heat management**: Pace your fire to avoid overheat; continuous rapid fire will trigger overheat in ~1 magazine. Reload and idle to cool down.
- **Customization**: Adjust FX intensity, overheat tolerance, and damage scaling in session settings to tune difficulty and visual preference.
- **Console access**: Press backtick (`) to open the console and experiment with `lr.*` CVars for real-time tuning during gameplay.

## Known Limitations

- **Multiplayer**: Energy Cell consumption is server-authoritative; client-side prediction is limited.
- **Mk1 body**: The frankenrifle aesthetic is work-in-progress; hold-pitch and beam muzzle alignment are tunable via console CVars.
- **Parked features**: Dangling battery visual is currently disabled by default (`lr.BatteryEnable = 0`); re-enable to toggle it back on.
