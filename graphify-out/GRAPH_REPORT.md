# Graph Report - SatisfactoryLaserRifleMod  (2026-06-25)

## Corpus Check
- 43 files · ~7,037,457 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 301 nodes · 452 edges · 41 communities (30 shown, 11 thin omitted)
- Extraction: 98% EXTRACTED · 2% INFERRED · 0% AMBIGUOUS · INFERRED: 8 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `705b815c`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- [[_COMMUNITY_Community 0|Community 0]]
- [[_COMMUNITY_Community 1|Community 1]]
- [[_COMMUNITY_Community 2|Community 2]]
- [[_COMMUNITY_Community 3|Community 3]]
- [[_COMMUNITY_Community 4|Community 4]]
- [[_COMMUNITY_Community 5|Community 5]]
- [[_COMMUNITY_Community 6|Community 6]]
- [[_COMMUNITY_Community 7|Community 7]]
- [[_COMMUNITY_Community 8|Community 8]]
- [[_COMMUNITY_Community 9|Community 9]]
- [[_COMMUNITY_Community 10|Community 10]]
- [[_COMMUNITY_Community 11|Community 11]]
- [[_COMMUNITY_Community 12|Community 12]]
- [[_COMMUNITY_Community 13|Community 13]]
- [[_COMMUNITY_Community 15|Community 15]]
- [[_COMMUNITY_Community 16|Community 16]]
- [[_COMMUNITY_Community 17|Community 17]]
- [[_COMMUNITY_Community 18|Community 18]]
- [[_COMMUNITY_Community 19|Community 19]]
- [[_COMMUNITY_Community 20|Community 20]]
- [[_COMMUNITY_Community 21|Community 21]]
- [[_COMMUNITY_Community 22|Community 22]]
- [[_COMMUNITY_Community 24|Community 24]]
- [[_COMMUNITY_Community 38|Community 38]]
- [[_COMMUNITY_Community 39|Community 39]]
- [[_COMMUNITY_Community 40|Community 40]]

## God Nodes (most connected - your core abstractions)
1. `Tick()` - 20 edges
2. `FireLaser()` - 12 edges
3. `Building a Laser Rifle Mod for Satisfactory — Full Pipeline` - 12 edges
4. `FLinearColor` - 11 edges
5. `Part 6 — In-editor asset build (exact steps & values) — [asset], your click-work` - 11 edges
6. `EffectiveMkLevel()` - 10 edges
7. `CurrentBeamColor()` - 10 edges
8. `FVector` - 9 edges
9. `GetSub()` - 9 edges
10. `MakeSlider()` - 9 edges

## Surprising Connections (you probably didn't know these)
- `Rifle Icon Hero` --references--> `Satisfactory Laser Rifle Guide Part 0`  [INFERRED]
  Content/Equipment/LaserRifle/Icons/src/rifle_icon_hero.png → satisfactory-laser-rifle-guide.md
- `Mk01 Color` --references--> `Tripo Rifles README`  [INFERRED]
  tripo_rifles/Mk01.fbm/Color.jpg → tripo_rifles/README.txt
- `Mk01 Metallic` --references--> `Tripo Rifles README`  [INFERRED]
  tripo_rifles/Mk01.fbm/Metallic.jpg → tripo_rifles/README.txt
- `Mk01 Normal` --references--> `Tripo Rifles README`  [INFERRED]
  tripo_rifles/Mk01.fbm/Normal.jpg → tripo_rifles/README.txt
- `Mk01 Roughness` --references--> `Tripo Rifles README`  [INFERRED]
  tripo_rifles/Mk01.fbm/Roughness.jpg → tripo_rifles/README.txt

## Import Cycles
- None detected.

## Communities (41 total, 11 thin omitted)

### Community 0 - "Community 0"
Cohesion: 0.14
Nodes (38): AFGCharacterPlayer, ALaserRifleSubsystem, APlayerController, FLinearColor, FVector, int32, ALaserRifleWeapon(), ApplyGripFromConfig() (+30 more)

### Community 1 - "Community 1"
Cohesion: 0.19
Nodes (18): ApplyDamageScaling(), CountPurchased(), GetConfigBool(), GetConfigFloat(), GetCoolTierCount(), GetDamageMultiplier(), GetDamageTierCount(), GetHeatTierCount() (+10 more)

### Community 2 - "Community 2"
Cohesion: 0.25
Nodes (17): FString, GetAllSchematics(), GetCoolTiers(), GetDamageTiers(), GetHeatTiers(), GetLevels(), LR_AddInfoUnlock(), LR_ApplyDefaultCost() (+9 more)

### Community 3 - "Community 3"
Cohesion: 0.23
Nodes (14): FName, FSettingsWidgetLocationDescriptor, FText, BuildSessionSettings(), FinishSetting(), MakeCheckBox(), MakeLocation(), MakeSlider() (+6 more)

### Community 4 - "Community 4"
Cohesion: 0.10
Nodes (20): Mk01 Color, Mk01 Metallic, Mk01 Normal, Mk01 Roughness, Rifle Icon Hero, Satisfactory Laser Rifle Guide Part 0, Smoke Puff, 1. Tripo Pro + API key (+12 more)

### Community 5 - "Community 5"
Cohesion: 0.36
Nodes (11): build_tree(), _carr(), _children(), cls(), _coord(), err(), log(), main() (+3 more)

### Community 6 - "Community 6"
Cohesion: 0.28
Nodes (8): LaserRifleSchematics(), LR_AddUnlock(), UFGSchematicCategory(), ULaserRifleSchematic_Base(), ULaserRifleUnlockRecipe(), LASERRIFLEMOD_API, namespace, UFGUnlock

### Community 7 - "Community 7"
Cohesion: 0.31
Nodes (6): assign(), fbx_opts(), get(), import_mesh(), import_tex(), log()

### Community 8 - "Community 8"
Cohesion: 0.25
Nodes (7): FGeometry, FPaintArgs, FSlateRect, FSlateWindowElementList, FWidgetStyle, NativePaint(), int32

### Community 9 - "Community 9"
Cohesion: 0.33
Nodes (5): env_attack_decay(), Stdlib-only laser-zap synthesizer (no numpy). Produces mono 48kHz 16-bit PCM WAV, Fast linear attack, exponential decay to ~0 by end of dur., f_start->f_end pitch sweep (exponential-ish via sweep_pow), plus a 2nd/3rd     h, synth()

### Community 12 - "Community 12"
Cohesion: 0.48
Nodes (4): log(), make_bp(), tset(), warn()

### Community 13 - "Community 13"
Cohesion: 0.40
Nodes (4): LaserRifleSettings(), UFGUserSettingCategory(), LASERRIFLEMOD_API, namespace

### Community 38 - "Community 38"
Cohesion: 0.06
Nodes (32): 4a. Item / Equipment Descriptor — [asset], 4b. Equipment Blueprint (the thing that exists in your hands) — [either], 4c. Recipe (how the player crafts it) — [asset], 4d. Schematic (how the player unlocks the recipe) — [asset], 4e. Wire registration into a Game World Module — [either], 6.1 Folders (under `LaserRifleMod Content`), 6.2 Import the 10 body meshes, 6.3 Icons (optional but nice) (+24 more)

### Community 39 - "Community 39"
Cohesion: 0.60
Nodes (5): img_bytes(), load_pil(), main(), parse_glb(), tex_to_image_index()

### Community 40 - "Community 40"
Cohesion: 0.49
Nodes (10): err(), gen_class(), item_amount(), load_class(), log(), main(), make_desc(), make_equip_bp() (+2 more)

## Knowledge Gaps
- **66 isolated node(s):** `ALaserRifleSubsystem`, `LASERRIFLEMOD_API`, `UTexture2D`, `int32`, `FPaintArgs` (+61 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **11 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `FireLaser()` connect `Community 0` to `Community 3`?**
  _High betweenness centrality (0.020) - this node is a cross-community bridge._
- **Why does `FName` connect `Community 3` to `Community 0`?**
  _High betweenness centrality (0.018) - this node is a cross-community bridge._
- **Why does `FLinearColor` connect `Community 0` to `Community 8`?**
  _High betweenness centrality (0.011) - this node is a cross-community bridge._
- **What connects `ALaserRifleSubsystem`, `LASERRIFLEMOD_API`, `UTexture2D` to the rest of the system?**
  _71 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Community 0` be split into smaller, more focused modules?**
  _Cohesion score 0.14169570267131243 - nodes in this community are weakly interconnected._
- **Should `Community 4` be split into smaller, more focused modules?**
  _Cohesion score 0.09956709956709957 - nodes in this community are weakly interconnected._
- **Should `Community 38` be split into smaller, more focused modules?**
  _Cohesion score 0.06060606060606061 - nodes in this community are weakly interconnected._