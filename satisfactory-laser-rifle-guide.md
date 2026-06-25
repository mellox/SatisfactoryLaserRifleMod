# Building a Laser Rifle Mod for Satisfactory — Full Pipeline

A practical, end-to-end guide. The whole thing happens in **Coffee Stain's custom UE 5.6.1 editor** (the CSS build), packaged with **SML + Alpakit**. Stock Unreal 5.8 and its MCP plugin are **not** part of this — they target a different engine and can't see Satisfactory's classes.

The pipeline in one line:

> Source/clean mesh → import FBX into CSS 5.6.1 → build the item/equipment/recipe/schematic → package with Alpakit → test in game.

---

## Part 0 — Prerequisites (one-time setup)

You need the Satisfactory modding environment fully working *before* any of this. If you haven't done it yet, follow the official setup in order:

1. **Custom CSS Unreal Engine** — download `UnrealEngine-CSS-Editor-Win64.exe` from the [satisfactorymodding/UnrealEngine](https://github.com/satisfactorymodding/UnrealEngine/releases/latest) GitHub releases. You must link your GitHub account to Epic to access it. A stock Epic engine install will **not** work.
2. **Visual Studio 2022** with the C++ workload (only needed if you write any C++; Blueprint-only mods can skip most of this, but VS is still used to build the editor).
3. **Wwise** — required even if you never touch sound. Use the exact version the docs specify.
4. **Starter Project** — `SML-Shipping-Dev-Win64.zip`, unzipped and renamed to your mod's working folder. This contains `FactoryGame.uproject`, SML, Alpakit, and ExampleMod.

Official setup docs: <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/dependencies.html>

> **Sanity check the version.** Satisfactory currently runs on **UE 5.6.1 with CSS custom changes**. Always grab the engine + starter project versions that match the game version you're modding, or you'll redo a lot of setup.

---

## Part 1 — Get the rifle mesh

Two viable routes. Both converge at "clean FBX ready to import."

### Route A — Pre-built CC0 kit (faster, lower risk, recommended to start)

Best free, ship-safe sources:

- **[Kenney](https://kenney.nl/)** — CC0 (public domain). Sci-Fi, Weapon, and Space kits. Clean, grid-aligned, game-ready. No attribution needed, safe to ship on SMR.
- **[Quaternius](https://quaternius.com/)** — CC0. Game-ready MegaKits in Blender/FBX/OBJ. Great for weapon parts and sci-fi props.
- **[Poly Pizza](https://poly.pizza/)** — large low-poly repo with polycount previews. **Check the per-model license** — it aggregates mixed licenses.

**Licensing caution** (this matters if you ever publish):
- **CC0** = do anything, no attribution. Kenney and Quaternius. Safe.
- **CC-BY** = free but *requires attribution*. Common on Sketchfab.
- **Non-commercial / No-derivatives** = often unusable for a published mod.
- **Fab / Sketchfab / Itch.io** = mixed. Verify each asset individually; "free to download" ≠ "free to ship."

### Route B — AI-generated (Tripo 3.1, Meshy, etc.) for a unique design

Use when you want a specific rifle that doesn't exist anywhere. Generates a novel mesh + PBR textures as FBX/glTF. **Trade-off:** AI meshes have messy topology and need real cleanup (see Part 2). Don't expect "generate and drop in."

> **Decision rule:** Want it *fast and license-safe* → Route A. Want a *specific custom design* → Route B and budget cleanup time.

### ✅ Asset acquired (Route A) — Kenney Blaster Kit v2.1

- **Kit:** Kenney Blaster Kit, version 2.1.
- **License:** **CC0 (Creative Commons Zero)** — free to download, **commercial-safe, no attribution required**. Crediting Kenney is appreciated but optional. The *only* restriction is **don't use Kenney's logo**. Ship-safe on SMR.
- **Location:** `C:\Claude\Projects\SatisfactoryLaserRifleMod\kenney_blaster-kit_2.1`
- **Formats included:** FBX, GLB (glTF), OBJ — **we use the FBX** (`Models\FBX format\`). Models share a single texture atlas (`Models\FBX format\Textures\variation-a.png`).
- **Contents (40 objects):** 18 blaster bodies (`blaster-a` … `blaster-r`), magazines/clips (`clip-small`, `clip-large`), foam bullets (`bullet-foam`, `-thick`, `-tip`, `-tip-thick`), grenades, scopes, silencers, crates, and targets.
- **Remaining sub-step:** pick **one blaster body** as the rifle base, and **optionally** a magazine (`clip-*`) or bullet prop to represent the battery/fuel item's icon/world mesh. (No separate ammo *item* is planned — fuel is built-in battery/charge — but a clip/bullet prop can still skin the charge UI/icon if wanted.)

---

## Part 2 — Clean it up in Blender

Even Route A models sometimes need tweaks; Route B models always do. Target a **low-poly, game-ready** result so you don't tank performance.

**Cleanup checklist:**

- [ ] **Polycount** — aim low. If a model is 150k polys, it'll hurt performance. Decimate / retopologize down to a sane budget for a handheld prop (a few thousand tris is plenty for a rifle).
- [ ] **Topology** — for AI meshes, retopo to clean quads/tris. Remove n-gons, interior geometry, floating bits.
- [ ] **Scale & orientation** — set real-world scale; rifle should be sized like a held weapon. Apply transforms (Ctrl+A → All Transforms). Unreal uses cm; check your unit scale on export.
- [ ] **Origin / pivot** — place the origin sensibly (e.g., the grip) so it attaches and rotates correctly when equipped.
- [ ] **UVs** — ensure a clean, non-overlapping UV unwrap so textures and materials map correctly.
- [ ] **Normals** — recalculate outside (Shift+N). Fix any inverted faces.
- [ ] **Materials/slots** — assign material slots logically (body, emissive barrel, etc.) so you can drive them in-engine.
- [ ] **LODs** (optional but nice) — make a lower-poly LOD1 for distance, or let Unreal auto-generate.
- [ ] **Collision** — you can author a simple collision shape, or generate one in Unreal on import.

**Export settings (FBX):**
- Format: **FBX** (most reliable into Unreal). glTF also works.
- Include: Mesh + (optional) materials. Apply modifiers.
- Forward/Up axes: match Unreal's convention (-Y forward, Z up is a common working combo; verify orientation after import and adjust).
- Smoothing: Faces or Edge, not Off.

---

## Part 3 — Import into the CSS 5.6.1 editor

1. Open your modding `FactoryGame.uproject` in the **CSS custom editor**.
2. If you haven't created your mod plugin yet: open the **Alpakit Dev** panel → **Create Mod** → pick the **Blueprint Only** template → give it a **Mod Reference** (e.g., `LaserRifleMod`). Since SML 3.0, mods are Unreal plugins; this scaffolds the `.uplugin` and folders for you.
3. In the Content Browser, enable **View Options → Show Plugin Content** so you can see your mod folder (`LaserRifleMod Content`).
4. Set up your **folder structure** (next section), then drag your FBX into the appropriate folder (or use the **Import** button). On import:
   - Import as **Static Mesh** (a rifle prop with no skeleton). Only use Skeletal Mesh if you've rigged animated parts.
   - Generate collision (Auto Convex or a simple primitive) if you didn't author one.
   - Import textures, or import them separately and build a Material.

### Recommended folder structure

Mirror Coffee Stain's organization inside your mod's content root (`<project>/Mods/LaserRifleMod/Content/`):

```
LaserRifleMod/
├── Equipment/
│   └── LaserRifle/
│       ├── SM_LaserRifle          (static mesh)
│       ├── T_LaserRifle_Icon      (inventory icon, 64px; 256px for build-gun style)
│       ├── M_LaserRifle           (material)
│       ├── Desc_LaserRifle        (FGEquipmentDescriptor)
│       └── BP_Equip_LaserRifle    (equipment Blueprint)
├── Recipes/
│   └── Recipe_LaserRifle          (FGRecipe)
└── Schematics/
    └── Schematic_LaserRifle       (FGSchematic)
```

CSS naming conventions (worth following so it reads like base-game content): `SM_` static mesh, `T_` texture, `M_` material, `Desc_` descriptor, `BP_` blueprint, `Recipe_`, `Schematic_`.

---

## Part 4 — Build the Satisfactory layer (the actual modding)

This is the part no external tool can do for you — it must be authored here, against Coffee Stain's classes.

> **C++ vs in-editor-asset seam (read this first).** This is a C++ workspace (see `_team/playbooks/satisfactory-mod.md`), so the rifle splits across two build paths that get packaged together:
>
> - **C++ / DLL-built** (firing logic, custom ammo behavior, any subsystem reaching engine internals via AccessTransformers): authored in `Source/<ModName>/{Public,Private}`, compiled by UBT, verified via the embedded build marker — exactly the playbook flow. **Agent-friendly.**
> - **In-editor `.uasset`** (the descriptor, mesh/icon/material assignment, the equipment Blueprint's class-default fields, recipe values, schematic values): authored by hand in the CSS editor GUI, cooked into the pak alongside the DLL. **No code pipeline produces these — they're click-work in the editor.**
>
> A weapon is genuinely a hybrid. Below, each piece is tagged **[C++]**, **[asset]**, or **[either]** so you know which path it's on.

### Design decisions (LOCKED) & level progression

Confirmed for this mod (changeable later):

- **Firing:** **hitscan**, provided by the engine's native **`UFGAmmoTypeLaser`** (→ `UFGAmmoTypeInstantHit` → `UFGAmmoType`) — a trace/instant-hit laser bolt with damage, effective range, beam FX, and a built-in charge-bonus-damage field. **No custom firing C++ needed.** **[asset / engine]**
- **Ammo / battery:** **built-in charge** = the engine's item-state ammo count (`FFGWeaponItemState.CurrentAmmoCount`, SaveGame on `AFGWeapon`). No separate craftable ammo item; regen behavior tuned via config. **[asset / engine + C++ config]**
- **Upgrades:** **single "Rifle" research line, Mk1–Mk10** (one MAM/HUB schematic chain; level = highest purchased), mirroring WeaponUpgrades' pure-C++ schematic pattern (each level its own `UClass` so `IsSchematicPurchased` can count it). **[C++]**
- **Config:** SML session-setting **sliders / toggles** (reusing WeaponUpgrades' config infra). **All future options live here** — the subsystem is the single config surface, so new toggles don't require rearchitecting. **[C++]**
- **Scaling:** damage = `base × (1 + Base)^level`, default `Base ≈ 0.16` → **×4.41 at Mk10** (tunable via slider), applied by the subsystem over our laser ammo type (WeaponUpgrades pattern). **[C++]**
- **Meshes:** all from the **Kenney Blaster Kit** (no Tripo needed). 10 of the 18 bodies, ordered below. **[asset]**

**Architecture (decision A — C++ subsystem + Blueprint weapon):**

- **`[C++]` subsystem** owns: the Mk level (from purchased schematics), the per-level **damage multiplier**, the per-level **visual level**, all **config** settings, and `BlueprintCallable` getters. Damage and visual level are exposed as **independent outputs**.
- **`[asset/BP]` weapon** (a Blueprint child of `AFGWeapon`) reads those getters to swap its **body mesh** and **beam color/intensity** per level. Per-level visual values live in the BP/data, so the look is retunable **without a rebuild**.
- **Why this split:** decoupling damage from visuals future-proofs config changes. Example: a "Freeze appearance at Mk N" toggle just makes the subsystem return a frozen `VisualLevel` while `DamageMultiplier` keeps using the real research level — one new setting, no structural change. Firing (the hard part) is engine-provided, so our C++ stays focused on progression + scaling + config.

**Mk1 → Mk10 mapping (proposed default):**

| Level | Body mesh (`Models/FBX format/`) | Beam color | Hex | Visual read |
|---|---|---|---|---|
| Mk1  | `blaster-a.fbx` | emerald green | `#2ECC71` | starter sidearm |
| Mk2  | `blaster-i.fbx` | cyan          | `#19E6D6` | |
| Mk3  | `blaster-b.fbx` | sky blue      | `#36B6FF` | |
| Mk4  | `blaster-h.fbx` | deep blue     | `#3A6BFF` | mid rifle |
| Mk5  | `blaster-e.fbx` | violet        | `#8A5CFF` | |
| Mk6  | `blaster-c.fbx` | magenta       | `#E23CE0` | |
| Mk7  | `blaster-d.fbx` | red           | `#FF3B3B` | heavy |
| Mk8  | `blaster-k.fbx` | orange        | `#FF7A1A` | |
| Mk9  | `blaster-l.fbx` | amber / gold  | `#FFC21A` | |
| Mk10 | `blaster-r.fbx` | white-hot     | `#F5FBFF` | hero weapon |

> Body picks/order and the beam palette are a starting point — easy to reorder later since the level→{mesh, beam} map is just a 10-entry table the rifle reads. The single texture atlas (`Textures/variation-a.png`) means body color is driven by material-instance tint/emissive, so bodies stay a coherent family while the beam carries the per-level palette.

Here's the chain and what each piece is:

### 4a. Item / Equipment Descriptor — [asset]

A plain item uses `FGItemDescriptor`. **A weapon is not a plain item** — equipment uses a different parent class. For an equippable rifle you want the **equipment descriptor** branch:

- Create a Blueprint Class with parent **`FGEquipmentDescriptor`** (the equipment cousin of `FGItemDescriptor`). This is what tells the game the item is equipment, not just inventory clutter. (The community confirms the base-game Xeno-Zapper's native parent is an equipment descriptor, not plain `FGItemDescriptor`.)
- Set: display name, description, **icon** (`T_LaserRifle_Icon`), **mesh** for the world/conveyor appearance (`SM_LaserRifle`), stack size, and the **equipment class** it spawns when equipped (your `BP_Equip_LaserRifle`).

### 4b. Equipment Blueprint (the thing that exists in your hands) — [either]

The held-weapon **class defaults** (mesh, attach socket, tuned values) are **[asset]** work in the editor. The **firing/ammo behavior** is where you'll likely drop to **[C++]** — a custom equipment/weapon subclass in `Source/`, which the Blueprint then derives from. Decide the split up front: BP-only subclass of the base-game weapon if the behavior is simple, or a C++ base class if you need custom ammo, charge mechanics, or engine-internal access via AccessTransformers.

- Create a Blueprint whose parent is the appropriate **equipment class**. The cleanest path is to **look at the base-game weapon** (e.g., the Rifle / Xeno-Zapper) in the project files and derive from / mirror its parent class. Satisfactory's weapons descend from the equipment hierarchy (`FGEquipment` → weapon/ranged subclasses).
- This is where firing logic, the held mesh, attach socket, ammo handling, and effects live. Item *state* (ammo/fuel) is stored on the equipment item — Satisfactory uses item state specifically for equipment like this.
- For a "laser" specifically: decide whether it's hitscan or projectile, whether it consumes a custom ammo item descriptor or a battery/charge, and wire the muzzle effect/sound.

> **Tip:** The fastest way to get a working weapon is to study the existing base-game rifle Blueprint and copy its structure, swapping in your mesh, icon, and tuned values. Don't build the equipment class from a blank parent if you can inherit the working one.

### 4c. Recipe (how the player crafts it) — [asset]

- Create a Blueprint Class with parent **`FGRecipe`**.
- Define **ingredients** (item costs), **product** (your `Desc_LaserRifle`, qty 1), and **ProducedIn** (which machine/bench can craft it — e.g., the Equipment Workshop / craft bench).
- Watch the rules: don't set an ingredient/product quantity above half the item's stack size or the machine never hits 100% uptime. Fluids are counted in liters internally (×1000).
- The recipe also **registers** the item — items aren't available in-game unless something (usually a recipe) references them.

### 4d. Schematic (how the player unlocks the recipe) — [asset]

- Create a Blueprint Class with parent **`FGSchematic`**.
- A schematic is the HUB milestone / MAM research "package." It grants the recipe via an **unlock** (use the BP unlock wrappers, e.g., unlock-recipes, and point it at `Recipe_LaserRifle`).
- Set type (e.g., **Hub Upgrade / Milestone**, not a tutorial phase), tier, unlock cost, and dependencies if you want it gated behind other schematics.
- **Register the schematic** at runtime via the schematic manager (`Add Available Schematic`) inside your mod's **Game World Module** `PostInit` — **and guard against double-registration** by checking `GetAvailableSchematics` / `GetPurchasedSchematics` first, or it'll appear multiple times in the HUB every load.

### 4e. Wire registration into a Game World Module — [either]

- The schematic registration (`Add Available Schematic` with the dedup guard) can live in a **[asset]** GameWorldModule Blueprint `PostInit`, or in your **[C++]** module startup if you prefer to keep registration in code alongside the rest of your subsystem logic. In a C++ workspace, code-side registration is often cleaner and easier to review.

---

## Part 5 — Package and test with Alpakit

1. Open the **Alpakit** panel (the alpaca-in-a-box toolbar button).
2. Set your **Satisfactory Game Path** (e.g., `C:\Program Files\Epic Games\SatisfactoryEarlyAccess\`).
3. Check **Copy Mods to Game** (so you don't move files manually) and optionally **Start Game**.
4. Set `SML.cfg` → `developmentMode = true` in your game's config folder so SML loads your in-dev mod.
5. Hit **Alpakit!** next to your mod. It compiles, cooks, packages, and (with the options above) installs and launches.
6. In game: unlock your schematic in the HUB → craft the rifle via your recipe → equip and fire.

**Iterate:** after Blueprint changes, re-Alpakit. After C++ or Editor-affecting changes, close the editor and rebuild Development Editor from Visual Studio first.

---

## Where Claude Code actually helps (and where it doesn't)

**Helps a lot — run Claude Code against your mod project folder:**
- Scaffolding the mod's folder structure and `.uplugin`.
- Writing/structuring the **Game World Module** registration logic (including the anti-double-registration guard).
- Generating C++ for custom weapon behavior, custom ammo descriptors, or firing logic if you go the native route.
- Boilerplate for recipes/schematics if you script content generation.
- Debugging build errors, reading logs, fixing SML version mismatches.

**Does NOT help:**
- The **UE 5.8 MCP plugin** — wrong engine, can't open your project, can't see `FGEquipment`/`FGRecipe`/`FGSchematic`. Skip it entirely for Satisfactory.
- Mesh *generation* — that's Tripo/Kenney/Blender, not any MCP toolset.

---

## Common pitfalls

- **Wrong engine version** — if the game loads fine without your mod but crashes with it, you likely built against the wrong engine/SML version. Match the docs exactly.
- **Plain `FGItemDescriptor` for a weapon** — you'll get an inventory item you can't equip. Use the **equipment descriptor** branch and an equipment Blueprint.
- **Item never appears** — it isn't registered; make sure a recipe references it.
- **Schematic shows multiple times** — missing the double-registration guard in `PostInit`.
- **Huge polycount mesh** — performance hit; keep it low-poly/game-ready.
- **Unverified license** — don't ship a Sketchfab/Fab asset without checking its license; only CC0 is automatically safe.
- **150k-poly "amazing" model** — looks great in preview, ruins frame rate in a factory full of machines. Optimize first.

---

## Quick reference — the class chain

| Piece | Parent class | Purpose |
|---|---|---|
| Item descriptor | `FGEquipmentDescriptor` | Defines the rifle as equipment (icon, mesh, name, equip class) |
| Equipment | base-game weapon/`FGEquipment` subclass | The held weapon: firing logic, ammo, effects |
| Recipe | `FGRecipe` | Crafting cost + product + machine; registers the item |
| Schematic | `FGSchematic` | HUB/MAM unlock that grants the recipe |
| Registration | `GameWorldModule` | Registers the schematic at world load (with dedup guard) |

---

## Key official docs

- Setup / dependencies: <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/dependencies.html>
- Create an Item: <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/SimpleMod/item.html>
- Recipe vs Schematic: <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/SimpleMod/recipe.html>
- Schematics reference: <https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/Schematic.html>
- Modeling guide (mesh import specifics): linked from the modding docs index <https://docs.ficsit.app/>
- Satisfactory Modding Discord — the fastest place for weapon-specific questions.

---

## Part 6 — In-editor asset build (exact steps & values) — [asset], your click-work

The C++ core is built and deployed (subsystem + 10 schematics + config). These are
the in-editor assets that reference it. Do them in the CSS 5.6.1 editor.

### ⚠️ Contract values that MUST match the C++ (exact strings)

| Thing | Exact value (do not change) | Why |
|---|---|---|
| Laser ammo asset path | `/LaserRifleMod/Equipment/LaserRifle/Ammo_LaserRifle` | the subsystem resolves `Ammo_LaserRifle_C` here to scale its damage |
| Research tree asset path | `/LaserRifleMod/Research/MAM_LaserRifle` | RootWorld auto-registers `MAM_LaserRifle_C` here |
| Schematic classes (tree nodes) | `USchematic_LaserRifle_01` … `_10` | the 10 Mk levels the subsystem counts |
| Subsystem getters (call from weapon BP) | `GetVisualLevel`, `GetDamageMultiplier`, `GetRifleLevel`, `GetMaxLevel` | on `ALaserRifleSubsystem` (BlueprintPure) |

> Tip throughout: where a base-game equivalent exists (Rifle weapon BP, Rifle
> recipe, an ammo type), **duplicate it and reskin** rather than building from a
> blank parent — far fewer hidden fields to get wrong.

### 6.1 Folders (under `LaserRifleMod Content`)
Create: `Equipment/LaserRifle/`, `Equipment/LaserRifle/Meshes/`, `Equipment/LaserRifle/Icons/`, `Recipes/`, `Research/`.
(Enable Content Browser → View Options → Show Plugin Content if the mod folder is hidden.)

### 6.2 Import the 10 body meshes
From `kenney_blaster-kit_2.1/Models/FBX format/` import these into `Equipment/LaserRifle/Meshes/`, in Mk order:
`blaster-a, blaster-i, blaster-b, blaster-h, blaster-e, blaster-c, blaster-d, blaster-k, blaster-l, blaster-r`.
- Import as **Static Mesh**, Generate Collision = on, Combine Meshes = on.
- Kenney meshes are small (~metres in cm) — after import, check size against a held weapon; if tiny, set Import Uniform Scale (try 100) and re-import, or scale in the weapon BP.
- Import `Models/FBX format/Textures/variation-a.png` once. Create one material `M_LaserRifle_Body` from it with a **Tint (Vector param)** and **Emissive (Vector param + scalar Intensity)** so the weapon BP can drive per-level color via a Dynamic Material Instance.

### 6.3 Icons (optional but nice)
Quickest: import `kenney_blaster-kit_2.1/Previews/blaster-*.png` (already 64px) into `Icons/` as `T_LaserRifle_Icon_Mk1..10`. Or use one shared icon to start.

### 6.4 Laser ammo — `Ammo_LaserRifle`  (parent `UFGAmmoTypeLaser`)
Create Blueprint Class → parent **UFGAmmoTypeLaser**, save EXACTLY at `Equipment/LaserRifle/Ammo_LaserRifle`.
- `mDamageTypesOnImpact`: add one element, an instanced `UFGDamageType`, `mDamageAmount` = your Mk1 base (e.g. **10**). (The subsystem multiplies THIS per level.)
- `mMaxAmmoEffectiveRange` ≈ **10000**; `mMagazineSize` = battery capacity (e.g. **25**); `mFireRate` to taste.
- Beam FX fields (tracer/impact) for the laser look; the per-level beam COLOR is driven from the weapon BP (6.5).
- `mBonusChargeDamagePercentage`: optional extra damage at full charge.

### 6.5 Weapon — `BP_Equip_LaserRifle`  (parent `AFGWeapon`)
Easiest: **duplicate the base-game Rifle equipment BP** and repoint it; otherwise create a Blueprint Class with parent **AFGWeapon**.
- Set its allowed/desired ammo to **Ammo_LaserRifle** (so it fires our bolt).
- Add a **Static Mesh Component "BodyMesh"** for the visible body (the AFGWeapon skeletal `mWeaponMesh` is for rigged guns; a static body is simpler).
- On **Equipped** and whenever level changes, call the subsystem and apply visuals:
  - get the `ALaserRifleSubsystem` (Get All Actors Of Class / mod-subsystem accessor),
  - `V = GetVisualLevel()` (1–10; treat 0 as 1 for the look),
  - set `BodyMesh` static mesh to `Meshes[V]` (a 10-entry array you fill: SM for blaster-a…r in Mk order),
  - set the body MID **Tint/Emissive** + beam color to the Mk `V` color (Mk1 emerald `#2ECC71` → Mk10 white-hot `#F5FBFF`, per the table in Part 4).
- Damage scaling itself needs **no BP work** — the subsystem already multiplies the ammo damage by `GetDamageMultiplier()`.

### 6.6 Equipment descriptor — `Desc_LaserRifle`  (parent `FGEquipmentDescriptor`)
- `mDisplayName` = "Laser Rifle"; `mDescription` = short blurb.
- Icon = `T_LaserRifle_Icon_Mk1` (or shared); world/inventory mesh = a body SM (e.g. blaster-a).
- Stack size = 1; **equipment class = `BP_Equip_LaserRifle`**.

### 6.7 Recipe — `Recipe_LaserRifle`  (parent `FGRecipe`)
Easiest: duplicate the base Rifle recipe. Otherwise:
- `mProduct` = `Desc_LaserRifle` × 1.
- `mIngredients` = your craft cost (e.g. 20 Reinforced Iron Plate, 20 Wire, 10 Rotor) — keep each qty ≤ half its stack size.
- `mProducedIn` = the **Equipment Workshop / Craft Bench** (the building the base Rifle is crafted in). The recipe also registers the item.

### 6.8 Research tree — `MAM_LaserRifle`  (parent `UFGResearchTree`)
Save EXACTLY at `Research/MAM_LaserRifle` (RootWorld auto-registers `MAM_LaserRifle_C`).
- Add **10 nodes**, linear chain Mk1 → Mk2 → … → Mk10.
- Node N → schematic class **`USchematic_LaserRifle_0N`** (`_10` for node 10). These C++ classes appear in the schematic class picker.
- Set the tree's MAM category/unlock so it shows up (e.g. gated behind a base MAM node you already have, or available from start).
- This is what makes the Mk1–10 actually purchasable; without it the schematics exist but can't be researched.

### 6.9 Build + test
Run `Scripts/build.ps1` (game closed) — it cooks these assets into the pak and recompiles the C++ (now `core-2`, which auto-registers the tree). In game:
1. Research Mk1 in the MAM → craft the rifle (recipe) → equip + fire.
2. Research up the line → confirm damage rises and the body/beam change per Mk.
3. Test the **Freeze Appearance at Mk** config (Mod Savegame Settings) → look locks, damage keeps climbing.

> Differential note (from the C++ review): the subsystem writes damage onto the
> ammo's shared `UFGDamageType` — verify in-game that researching changes felt
> damage and that reloading/save-reload keeps the right value.
