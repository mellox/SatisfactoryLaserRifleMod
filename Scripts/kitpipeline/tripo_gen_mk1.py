"""
tripo_gen_mk1.py -- 4 NEW Mk1 prototype laser-rifle bodies, using the Mk1 prompt from
tripo-rifle-prompts.md + the shared style suffix, on the documented high-quality pipeline
(v3.1-20260211, geometry+texture detailed, texture+pbr on). Same submit/pump/checkpoint
structure as tripo_gen_v31.py. API key read from the scene property; NEVER logged.
Run inside the live Blender session (execute_blender_code imports + calls submit()/pump()).
"""
import bpy, json, os, urllib.request, urllib.error

OUT = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\mk1_v31"
CKPT = os.path.join(OUT, "_tasks.json")
os.makedirs(OUT, exist_ok=True)

# Mk1 = a BASEMENT FRANKENRIFLE (user 2026-07-05): cobbled together, duct-taped, wires hanging out,
# salvaged scrap -- NOT a clean sci-fi rifle. The old shared suffix's "clean PBR / industrial / chunky
# functional / hero weapon" clauses tidied it up, so they're DROPPED here and "clean/polished/manufactured"
# is pushed into the NEGATIVE so Tripo can't drift back to a factory look.
BASE_MK1 = ("A cobbled-together improvised laser rifle, jury-rigged from salvaged junk in a basement "
            "workshop, built from mismatched scrap-metal plates and random parts bolted and welded "
            "together, wrapped in strips of duct tape and hose clamps, exposed copper wiring and loose "
            "tangled cables hanging out everywhere, dented rusty weathered gunmetal, hand-cut jagged "
            "edges, protruding bolts rivets and screws, a crude stubby improvised emitter cobbled from "
            "pipe fittings, looks hand-made barely-functional and dangerous, a Frankenstein weapon. ")
SUFFIX = ("single connected object, one improvised laser rifle, horizontal, a few orange-and-black "
          "hazard-stripe accents, detailed weathered PBR game asset, neutral studio lighting, plain "
          "dark background, no hands, no scene.")
VARIANTS = {
 "a": "cobbled from car parts and a rusty muffler, an oversized salvaged car battery taped on the side",
 "b": "cobbled from plumbing pipes and a jury-rigged circuit board, wires zip-tied and taped everywhere",
 "c": "cobbled from a broken power tool and appliance scraps, cracked plastic and sheet metal welded on",
 "d": "cobbled from e-waste and old electronics, exposed capacitors, a cracked meter dial, tangled wiring",
}
NEG = ("clean, polished, sleek, new, pristine, smooth, manufactured, factory-made, machine gun, "
       "assault rifle, real firearm, wood, bullet magazine, iron sights, human, hand, arm, text, "
       "watermark, blurry, lowres, low quality, deformed, cluttered background")   # <255
ALL_NAMES = ["mk1_" + v for v in ("a", "b", "c", "d")]

def _prompt(name):
    v = name.rsplit("_", 1)[1]
    return BASE_MK1 + VARIANTS[v] + ", " + SUFFIX

def _api(path, payload=None):
    key = ""
    for sc in bpy.data.scenes:
        k = getattr(sc, "api_key", "")
        if k:
            key = k; break
    url = "https://openapi.tripo3d.ai/v3" + path
    data = json.dumps(payload).encode() if payload is not None else None
    req = urllib.request.Request(url, data=data, method="POST" if data else "GET",
        headers={"Authorization": "Bearer " + key, "Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=90) as r:
        return json.loads(r.read().decode())

def _load():
    return json.load(open(CKPT)) if os.path.exists(CKPT) else {"tasks": {}}

def _save(ck):
    with open(CKPT, "w") as f:
        json.dump(ck, f, indent=1)

def key_present():
    return any(getattr(sc, "api_key", "") for sc in bpy.data.scenes)

def submit(names=None):
    names = names or ALL_NAMES
    ck = _load(); done = []; hit_cap = False
    for name in names:
        if name in ck["tasks"] and ck["tasks"][name].get("task_id"):
            continue
        try:
            res = _api("/generation/text-to-model", {
                "type": "text_to_model", "prompt": _prompt(name), "negative_prompt": NEG,
                "model": "v3.1-20260211", "texture": True, "pbr": True,
                "texture_quality": "detailed", "geometry_quality": "detailed",
                "face_limit": 100000})
        except urllib.error.HTTPError as e:
            if e.code == 429:
                hit_cap = True; break
            raise
        tid = res.get("data", {}).get("task_id") or res.get("task_id")
        ck["tasks"][name] = {"task_id": tid, "status": "submitted"}
        _save(ck); done.append(name)
    return {"submitted": done, "total": len(ck["tasks"]), "hit_cap": hit_cap}

def pump():
    ck = _load(); tasks = ck["tasks"]
    ids = [t["task_id"] for t in tasks.values() if t.get("task_id")]
    if ids:
        res = _api("/tasks/list", {"task_ids": ids})
        details = res.get("data", {}).get("tasks", res.get("tasks", {}))
        for name, t in tasks.items():
            d = details.get(t["task_id"])
            if not d:
                continue
            t["status"] = d.get("status", t.get("status")); t["progress"] = d.get("progress")
            if t["status"] == "success" and not t.get("downloaded"):
                out = d.get("output") or {}
                url = out.get("pbr_model") or out.get("model_url") or out.get("model")
                if isinstance(url, dict):
                    url = url.get("url")
                if url:
                    dest = os.path.join(OUT, name + ".glb")
                    urllib.request.urlretrieve(url, dest)
                    t["downloaded"] = dest
    _save(ck)
    by = {}
    for t in tasks.values():
        by[t.get("status", "?")] = by.get(t.get("status", "?"), 0) + 1
    return {"statuses": by, "progress": {n: t.get("progress") for n, t in tasks.items()},
            "downloaded": sum(1 for t in tasks.values() if t.get("downloaded"))}
