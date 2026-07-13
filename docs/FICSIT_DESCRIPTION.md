# Laser Rifle Mod — FICSIT Description

## Overview

The Laser Rifle mod adds a powerful craftable hitscan weapon to Satisfactory with a **10-tier research progression** (Mk1 through Mk10). Each tier scales damage, capacity, and performance while dramatically transforming the rifle's appearance and firing characteristics. Progress from a cobbled-together scrap-metal "frankenrifle" at Mk1 to a sleek energy weapon at Mk10, complete with per-tier unique beam colors, dynamic visual effects, and configurable gameplay mechanics.

---

## The 10 Laser Rifles: Mk1 to Mk10

### Mk1: The Frankenrifle
The journey begins with the **Mk1 Laser Rifle**, a ramshackle assembly of salvaged electronics and wires held together by hope. Crafted from raw materials at your Equipment Workshop—5 Reinforced Iron Plates, 40 Wire, 10 Copper Wire, and 10 Quartz—it delivers modest hitscan damage but serves as your entry point into laser weaponry. Mk1 research also unlocks the **Energy Cell recipe** (the custom fuel cell), allowing you to power your rifle.

**Research cost:** Raw parts + parts only (no prior rifle needed).

### Mk2 to Mk5: Steel & Refinement
Each step from Mk2 through Mk5 evolves the rifle's construction. Craft each by consuming the previous Mk rifle plus new tier-appropriate parts (copper, steel, rotors, cooling systems, and more). The bodies transition from the frankenrifle's jury-rigged look toward more purposeful designs. Damage, energy capacity, and heat tolerance scale smoothly across these tiers.

**Research cost per tier:** Consumes the prior Mk rifle + tier-specific parts.

### Mk6 to Mk10: Energy Weapons
Mk6 onward transforms the laser into sleek, glowing energy weapons. Mk6 emits a **cyan beam**, Mk7 a **blue beam**, Mk8 a **violet beam**, Mk9 a **magenta beam**, and Mk10 a brilliant **white-gold energy burst**. These rifles sport luminous cores that self-illuminate based on your equipped tier; crafting them requires advanced parts (motors, cooling systems, crystal oscillators, and quantum components) plus the prior Mk rifle.

**Research cost per tier:** Consumes the prior Mk rifle + advanced tier-specific parts.

---

## Research Progression & MAM Tree

All 10 rifles live in a single **"Laser Rifle"** research chain in the MAM. Progress sequentially—researching Mk2 requires completing Mk1, researching Mk3 requires completing Mk2, and so on. Each research costs the ingredients shown in the MAM description (which match your Equipment Workshop recipe exactly). To upgrade from Mk1 to Mk2 rifle, you must research the Mk2 schematic (consuming a Mk1 rifle as part of research cost) AND craft a new Mk2 (consuming another Mk1 rifle as part of the crafting cost).

Early research tiers unlock quickly with basic resources; later tiers demand advanced components and considerable investment, forming a natural gate on raw firepower.

---

## Energy Cell Fuel System

The laser fires and reloads by consuming fuel: either the **Energy Cell** (a custom ammunition type unlocked with Mk1 research) or the base-game **Battery** (if you carry one). When you run dry and reload:

1. The weapon checks your inventory for available fuel
2. It consumes the fuel with the **highest capacity** you currently carry—prioritizing Batteries (8 portions) over Energy Cells (3 portions)
3. One fuel refill recharges the rifle's internal pack to a new energy level
4. Each fire consumes 1 charge from the current pack; a full pack holds 5 shots (Mk1) up to 30 shots (Mk10)

**Energy Cell recipe:** Craft at the Equipment Workshop (unlocked alongside Mk1 rifle). Costs 5 Wire + 2 Quartz Crystals, takes 2 seconds, yields 1 Energy Cell. Stack them in your inventory to keep your rifle firing.

**Reload mechanics:**
- Fire until the pack is empty
- Press R to reload → the rifle consumes one fuel item and refills
- Out of fuel? Reload fails with a helpful message; craft more Energy Cells at your workbench
- The rifle never jams permanently—just craft more ammo and resume

---

## Heat & Overheat Mechanic

The laser rifle **heats up as you fire** and must cool between bursts or risk overheat.

**Heat buildup:**
- Each shot generates heat
- Sustained rapid fire accumulates heat toward the overheat threshold (~half a fuel cell's worth of shots at Mk1, more at higher Mk)
- Heat is **cell-relative**: the threshold scales with your current fuel cell's shot capacity, so every Mk overheats after dumping roughly half a cell of ammo at full auto

**Cooling:**
- Heat cools **only during idle time**—not while swapping cells or during continuous fire
- Higher Mk rifles cool faster (Mk1 cools slowest; Mk10 cools roughly 1.5x faster)
- Reload during overheat to swap fuel cells while the rifle cools (the long reload gives the heat time to drop)

**Overheat state:**
- When overheat threshold is reached, firing locks out; the rifle cannot shoot until heat drops below the threshold
- The overheat glow intensifies on the rifle, and heat-related visual FX (smoke, arcs) ramp up
- No ammo is wasted; the weapon simply stops firing until cool

**Strategy:** Pace your shots to stay under the threshold, or dump a cell and reload to force a cool-down window. Low-Mk rifles encourage tactical bursts; high-Mk rifles reward sustained fire but still heat up if you hold the trigger too long.

---

## Unique Bodies & Visual Effects

Each Mk rifle sports a distinct 3D model and per-tier visual identity:

- **Mk1–Mk5:** Static steel-based bodies with industrial construction.
- **Mk6–Mk10:** Glowing energy rifles with luminous emissive cores in their color (Mk6 cyan, Mk7 blue, Mk8 violet, Mk9 magenta, Mk10 white-gold).

**Per-Mk visual effects while firing or overheating:**
- **Electric Sparks (Mk1–Mk10):** Tier-colored crackles burst from the muzzle—warm amber at Mk1, transitioning through the spectrum to violet at Mk10. The spark intensity scales with heat and the slider setting.
- **Arcing Bolts (Mk4+):** Animated jagged branching arcs (resembling tesla coils) crackle off the body and jump to nearby terrain when heat builds.
- **Ground Jump (Mk4+):** When overheat is severe (~85%), a long arc strikes from the rifle to the ground or nearest obstacle.
- **Impact Splash:** On hit, 3 short arcs burst from the impact point.
- **Heat Smoke (all Mk):** As heat builds, colored smoke trails intensify, scaling with the heat slider setting.
- **Plasma Orb (Mk6+, optional):** At high research tiers, a subtle emissive glow pulses near the muzzle.

**Muzzle bloom** briefly illuminates the barrel on each shot, and the **custom crosshair** displays your current rifle's status.

---

## First-Person Held Viewmodel

When equipped, the laser rifle renders as a first-person viewmodel anchored to your camera, exactly like Satisfactory's other handheld weapons. Each Mk is visually distinct:

- **Hold orientation** is tuned per-Mk so the barrel lines up with your crosshair (no sight misalignment)
- **Hold scale** adjusts per-tier to balance on-screen presence (low-Mk rifles stay compact; high-Mk energy weapons are larger and more imposing)
- **Reload animation** scales with Mk: Mk1 takes ~5.8 seconds to swap fuel; Mk10 completes in ~3 seconds (faster reload at higher tiers)
- **Dangling components** on Mk1 (a rigged mesh variant) include reactive wires and a swinging battery, adding mechanical character

---

## Custom Crosshair & Energy HUD

Your custom crosshair displays all critical rifle info in real-time:

- **Current energy:** A small meter showing your current fuel pack's shots remaining (e.g., "12 / 30" = 12 shots left in a 30-shot pack)
- **Spare fuel count:** Shows how many spare Energy Cells or Batteries you carry (e.g., "x3 spare")
- **Heat level:** A heat indicator bar that rises as you fire and falls as you idle
- **Overheat warning:** The indicator turns red when approaching the overheat threshold; turns fully red and firing locks out on overheat

This centralized display ensures you always know your ammo, cooldown status, and overheat risk at a glance.

---

## Configurable In-Game Settings

Access the mod settings via **Mod Settings → Laser Rifle** in your savegame menu to tune gameplay and visuals:

### Main Settings
- **Freeze Appearance:** Lock the rifle's body and beam color to a specific Mk (0 = follow research, 1–10 = fixed Mk) for cosmetic override
- **Glow Intensity:** Beam brightness multiplier
- **Glow Tightness:** Beam width (thinner or thicker)
- **Beam Min & Intensity:** Minimum beam visibility and peak brightness
- **Heat: Overheat Shots (Mk1 base):** Shots before overheat at Mk1 (higher Mk scale automatically)
- **Heat: Cooldown Speed:** How fast heat drops while idle (higher = faster recovery)
- **Heat: Glow Boost:** Extra beam brightness when overheating
- **Heat: Fire Slowdown Max:** Movement penalty when severely overheated
- **Overheat Penalty:** Additional recoil/cooldown penalty when hitting overheat
- **Smoke: Amount, Start Heat, Opacity:** Fine-tune heat smoke visibility and intensity
- **FX: Electric Sparks:** Slider (0–3) controlling electric spark density and crackle while firing

### Advanced Settings (for balance tuning)
- **Damage: Mk Bump:** Small per-Mk damage multiplier
- **Other research/tier tuning options** (parked/inactive; available for future expansion)
- **Random Components (WIP):** Feature flag for randomized kit loadouts (default off)

All settings adjust live—no reload required.

---

## Crafting & Economics

### Energy Cell
- **Recipe:** 5 Wire + 2 Quartz Crystals → 1 Energy Cell (2s at Equipment Workshop)
- **Fuel value:** Mk-dependent; Mk1 Energy Cell = 3 fuel portions = 3 refills × 5 shots (15 total shots)
- **Strategy:** Craft in bulk; a small buffer keeps you firing without frequent workbench trips

### Laser Rifle Recipes (Equipment Workshop)
- **Mk1:** 5 Reinforced Iron Plates + 40 Wire + 10 Copper Wire + 10 Quartz Crystals
- **Mk2–Mk10:** Each consumes the prior Mk rifle + tier-specific parts (motors, steel, rotors, cooling systems, quantum components, etc.)

Early tiers are cheap and quick; later tiers demand rarer resources and considerable crafting time, balancing progression against your production capacity.

---

## Damage Scaling

Damage scales per Mk using a tunable curve:
- **Base formula:** `(1 + MkBump)^Mk`, where MkBump is a configurable small bonus per level
- **Mk1 damage:** Starting point (modest, balanced for early-game)
- **Mk10 damage:** ~5–10× Mk1 (exact multiplier depends on your MkBump setting)

With default settings, higher Mk rifles deal exponentially more damage. Adjust **Damage: Mk Bump** in Advanced settings to make progression more linear or steeper. Carefully calibrated defaults ensure each tier feels like a meaningful upgrade without trivializing late-game threats.

---

## Multiplayer & Dedicated Servers

The laser rifle works in singleplayer, co-op listen servers, and (with limitations) on dedicated servers:

- **Singleplayer & Listen Servers:** Full functionality; all mechanics work as intended
- **Dedicated Server (known issue):** Reload consumption is gated by authority checks and currently only runs fully on the local controller, which can allow free reloads in rare MP scenarios. Use in co-op listen mode for reliable shared behavior. (This limitation is flagged for future fix.)

---

## Performance & Balance Notes

- **Hitscan weapon:** Traces are instant; no projectile travel time
- **Aim-assist on hit:** A small sphere-sweep around the beam endpoint helps land shots on creatures (friendly-fire style)
- **Visual effects gating:** Electric sparks, arcs, and smoke scale with slider settings and heat to avoid performance impact at high settings

---

## Removal from Saves

To safely remove the mod from an existing savegame:

1. **Unequip the rifle** if you're carrying one
2. **Disable the mod** in the mods menu (or delete the mod from your mods folder)
3. **Load your save** — the game will suppress the rifle class and forget any equipped instances

Your save remains intact. The Energy Cell items will persist in storage/inventory but become inert (unequippable, uncraftable) once the mod is gone. Re-enable the mod to restore full functionality.

---

## Known Limitations & Future Expansion

- **Random kit components (WIP):** The kit randomization system is behind a feature flag and not yet fully integrated
- **Research tiers (parked):** Damage/Heat/Cooling research tiers were simplified to Mk-only progression; research-based scaling may return in future updates
- **Multiplayer reload bug:** See Dedicated Servers section above

---

## Configuration Tips & Troubleshooting

**"My rifle overheats too quickly"**
- Adjust **Heat: Overheat Shots** slider higher to increase the shots-before-overheat threshold
- Pace your fire instead of holding the trigger; reloads give cooling time

**"The beam looks too thin/bright"**
- Use **Beam Min**, **Beam Intensity**, and **Glow Tightness** sliders to adjust

**"I want the Mk5 look but Mk10 damage"**
- Set **Freeze Appearance** to 5, then research up to Mk10 for the power without the visual change

**"Sparks are too bright/too dim"**
- Adjust **FX: Electric Sparks** slider (0 = none, 1–3 = increasing density)
- Lower **Heat: Glow Boost** if overheat effects are overwhelming

---

## Credits & Support

**Created by:** mello

The Laser Rifle mod is a complete weapon system built atop Satisfactory's modding framework (SML3) and powered by Wwise audio. Balancing, art, and mechanics have been refined through extensive playtesting and community feedback.

For issues, feature requests, or compatibility questions, please report via the mod's support channel (if available on ficsit.app or your mod host).

---

## Description Change History

### 2026-07-13 — v1.0.0 initial description
- Initial comprehensive FICSIT description covering all 10 Mk laser rifle progression, MAM research tree, Energy Cell fuel system, heat/overheat mechanics, per-Mk bodies and visual effects, first-person viewmodel, custom crosshair HUD, configurable in-game settings (15+ sliders in Main and Advanced categories), crafting recipes, damage scaling, multiplayer support, and known limitations. Document serves as authoritative source for all mod capabilities, end-to-end player experience, and configuration options.
