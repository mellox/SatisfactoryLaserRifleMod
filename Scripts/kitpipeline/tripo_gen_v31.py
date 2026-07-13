"""
tripo_gen_v31.py -- PROPER high-quality generation for the Mk6-10 energy-rifle candidates.
Uses the DOCUMENTED pipeline params (component-farming-prompts.md / Tripo v3 API), NOT the
Blender addon's v2.5 standard-quality defaults that produced the melted-plastic batch:
  POST /v3/generation/text-to-model  model=v3.1-20260211, geometry_quality=detailed,
  texture_quality=detailed, texture+pbr on, face_limit 100000.

Creative direction (user 2026-07-05): energy-WEAPON concepts, NOT conventional rifles with
sci-fi paint. Rising futuristic tech Mk6->Mk10, parts literally made of energy, Mk10 barely
a physical object. The old body template deliberately described a conventional rifle -- that
is exactly the "machine gun not a laser rifle" look being rejected, so these prompts replace
it (quality pipeline kept, art direction new).

Run inside the live Blender session (execute_blender_code imports this and calls submit()/pump()).
API key from the scene property; never logged.
"""
import bpy, json, os, urllib.request

OUT = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\mk6to10_v31"
CKPT = os.path.join(OUT, "_tasks.json")
os.makedirs(OUT, exist_ok=True)

BASE = ("a futuristic directed-energy laser rifle, a held weapon with a pistol grip and a "
        "forward energy emitter, sci-fi energy-weapon concept, single isolated object on a "
        "plain dark background, dramatic clean studio lighting, ")
LEVELS = {
 "mk6": ("sleek manufactured energy carbine, matte charcoal alloy chassis, a bright glowing "
         "cyan energy core visible through cooling slots along the frame, a ringed focusing-"
         "lens emitter at the front instead of a gun barrel, smooth machined panels, clean "
         "grounded near-future sci-fi"),
 "mk7": ("advanced plasma rifle, a segmented floating barrel shroud with luminous electric-"
         "blue plasma coils wrapped around a glowing energy core, sleek angular armor with "
         "light-emitting seams, a wide emitter aperture, high-tech exotic sci-fi weapon"),
 "mk8": ("high-tech energy rifle where part of the frame is replaced by solid glowing violet "
         "energy, hovering barrel segments suspended in a containment field with visible "
         "energy arcs between them, seamless monocoque emitter housing, advanced alien-tech"),
 "mk9": ("exotic near-magic energy weapon, half gloss-black sculpted shell and half a glowing "
         "magenta energy lattice, fragmented components floating apart held together by beams "
         "of light, a radiant emitter crystal, elite otherworldly design, mostly energy"),
 "mk10": ("an impossible otherworldly energy weapon barely recognizable as a rifle, its "
          "structure mostly made of radiant white-and-gold light and flowing energy, "
          "shattered fragments hovering in place held by luminous force, a floating central "
          "emitter core, seamless organic energy-construct form defying physics, a divine "
          "far-future sci-fi artifact"),
}
VARIANTS = {
 "a": "compact form, short overall length",
 "b": "long-emitter form, extended slender front",
 "c": "broad heavy form, bulky powerful midsection",
 "d": "slim minimal elegant form, thin refined profile",
}
NEG = ("machine gun, assault rifle, real firearm, conventional gun, wood, bullet magazine, "
       "ammo clip, iron sights, human, hand, arm, text, watermark, blurry, lowres, "
       "low quality, broken mesh, deformed, cluttered background")   # <255

def _prompt(name):
    lvl, var = name.rsplit("_", 1)
    return BASE + LEVELS[lvl] + ", " + VARIANTS[var]

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
    if os.path.exists(CKPT):
        return json.load(open(CKPT))
    return {"tasks": {}}

def _save(ck):
    with open(CKPT, "w") as f:
        json.dump(ck, f, indent=1)

import urllib.error

ALL_NAMES = [f"{l}_{v}" for l in ("mk6","mk7","mk8","mk9","mk10") for v in ("a","b","c","d")]

def submit(names):
    """Submit each name via the v3 detailed pipeline; checkpoint after EACH. Stops cleanly
    on HTTP 429 (concurrency cap, error 2000 -- 10 running max per category); the caller
    retries later as slots free."""
    ck = _load()
    done = []
    hit_cap = False
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
                hit_cap = True
                break
            raise
        tid = res.get("data", {}).get("task_id") or res.get("task_id")
        ck["tasks"][name] = {"task_id": tid, "status": "submitted"}
        _save(ck)
        done.append(name)
    return {"submitted": done, "total_tasks": len(ck["tasks"]), "hit_concurrency_cap": hit_cap}

def tick():
    """One driver step: download finished models, then submit any not-yet-submitted names
    (concurrency-capped). Call repeatedly until downloaded == len(ALL_NAMES)."""
    p = pump()
    ck = _load()
    missing = [n for n in ALL_NAMES if n not in ck["tasks"] or not ck["tasks"][n].get("task_id")]
    s = submit(missing) if missing else {"submitted": []}
    p2 = pump()   # catch anything that finished during submit
    return {"downloaded": p2["downloaded"], "statuses": p2["statuses"],
            "submitted_now": s["submitted"],
            "still_unsubmitted": len([n for n in ALL_NAMES
                                      if n not in _load()["tasks"]])}

def pump():
    """Batch-query all tasks, download any newly-succeeded model."""
    ck = _load()
    tasks = ck["tasks"]
    ids = [t["task_id"] for t in tasks.values() if t.get("task_id")]
    if ids:
        res = _api("/tasks/list", {"task_ids": ids})
        details = res.get("data", {}).get("tasks", res.get("tasks", {}))
        for name, t in tasks.items():
            d = details.get(t["task_id"])
            if not d:
                continue
            t["status"] = d.get("status", t.get("status"))
            t["progress"] = d.get("progress")
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
    return {"statuses": by,
            "downloaded": sum(1 for t in tasks.values() if t.get("downloaded"))}
