# run_kit_finalize_batch.py -- runs finalize_kit_rifle_blender.py's finalize() for a list
# of body_ids and writes the FULL UE hand-off JSON per rifle (not just the raw slot map),
# so each is immediately ready for the later UE-import+build step. Purely mechanical --
# no judgment calls, safe to run unattended while the game is open.
#
# USAGE (inside Blender via execute_blender_code):
#   import importlib.util
#   spec = importlib.util.spec_from_file_location("run_batch", <this path>)
#   mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
#   mod.run_batch(["body_mk2_junk", "body_mk3_rough", ...])
import importlib.util, json, os, re

FINALIZE_SCRIPT = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\Scripts\kitpipeline\finalize_kit_rifle_blender.py"
HANDOFF_DIR = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\Scripts\kitpipeline"
TEXROOT = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\component_library\kit_composed\_matfix"
KIT_PARTS_RAW = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\component_library\kit_parts\raw"
KIT_BODIES_RAW = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\component_library\kit_bodies\raw"

def _mk_index(body_id):
    m = re.search(r"mk(\d+)", body_id)
    if not m:
        raise ValueError("could not parse mk index from body_id: " + body_id)
    return int(m.group(1))

def run_one(body_id, mod, log):
    res = mod.finalize(body_id, log=log)
    if not res:
        return {"body_id": body_id, "status": "FAILED", "reason": "finalize() returned None"}

    mk_index = _mk_index(body_id)
    part_source_glbs = {}
    for part_id in set(res["slot_to_part"].values()):
        if part_id is None:
            continue
        if part_id == body_id:
            src = os.path.join(KIT_BODIES_RAW, part_id + "_tex.glb")
        else:
            src = os.path.join(KIT_PARTS_RAW, part_id + "_tex.glb")
        if not os.path.exists(src):
            log("WARNING: source glb not found for part %s at %s" % (part_id, src))
            continue
        part_source_glbs[part_id] = src

    handoff = {
        "body_id": body_id,
        "mk_index": mk_index,
        "fbx": res["fbx"],
        "body_only_fbx": res.get("body_only_fbx"),
        "muzzle_frac": res.get("muzzle_frac"),
        "slot_to_part": res["slot_to_part"],
        "texture_root": TEXROOT,
        "part_source_glbs": part_source_glbs,
    }
    out_path = os.path.join(HANDOFF_DIR, "_handoff_%s.json" % body_id)
    with open(out_path, "w") as f:
        json.dump(handoff, f, indent=2)

    ok = os.path.exists(out_path) and os.path.exists(res["fbx"])
    return {"body_id": body_id, "status": "OK" if ok else "FAILED",
            "rotation_deg": res["rotation_deg"], "handoff": out_path, "fbx": res["fbx"]}

def run_batch(body_ids, log=print):
    spec = importlib.util.spec_from_file_location("finalize_kit_rifle", FINALIZE_SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)

    results = []
    for body_id in body_ids:
        log("=== %s ===" % body_id)
        r = run_one(body_id, mod, log)
        results.append(r)
        log("  -> %s" % r["status"])
    return results
