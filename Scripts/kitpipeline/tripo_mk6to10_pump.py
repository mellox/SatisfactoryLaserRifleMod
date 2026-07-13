"""
tripo_mk6to10_pump.py -- one PUMP STEP for the Mk6-10 candidate generation batch.
Run repeatedly inside the live Blender session (execute_blender_code exec's this file).
Each call:
  1. Batch-queries all known task ids (POST /v3/tasks/list).
  2. Downloads the model for any newly-succeeded task -> <name>.glb in OUT.
  3. Submits pending prompts while in-flight count < MAX_INFLIGHT (rate-limit safe,
     one at a time, stops on the first rate-limit error).
  4. Rewrites the checkpoint and returns a summary dict.
API key comes from bpy.context.scene.api_key (never logged).
"""
import bpy, json, os, urllib.request

OUT = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\mk6to10_candidates"
CKPT = os.path.join(OUT, "_tasks.json")
MAX_INFLIGHT = 8   # observed cap ~10; stay under it

MOUNT = ("six distinct flat or recessed mounting zones at: left foregrip, top of receiver, "
         "forward barrel top, barrel side, top of stock, side of grip -- each a plain "
         "unornamented mounting surface, ready to accept an attached accessory")
NEG = ("human hand, human, text, watermark, bullet magazine, ammo clip, conventional firearm "
       "receiver, blank untextured patch, attached accessories, wires, batteries, antennas, gauges")
BASE = "single energy rifle, isolated object, plain background, "
LEVELS = {
 "mk6": "sleek manufactured energy rifle body, brushed gunmetal alloy panels with precision seams, glowing cyan energy conduit inset along the barrel, machined receiver, clean industrial sci-fi",
 "mk7": "advanced composite energy rifle body, layered angular armor panels, integrated luminous blue energy coils around the barrel shroud, low-profile rail fin on top, high-tech military sci-fi",
 "mk8": "high-tech energy rifle body, seamless monocoque alloy housing, segmented floating barrel shroud, violet plasma channel glowing through the frame, minimal panel lines, futuristic precision weapon",
 "mk9": "near-future masterwork energy rifle body, gloss obsidian housing with luminous magenta seam lines, suspended barrel segments, sculpted aerodynamic form, elite futuristic weapon",
 "mk10": "otherworldly pinnacle energy rifle body, radiant white and gold sculpted housing, hovering split-frame segments, seamless flowing organic-tech form, faint energy aura along the frame, ultimate sci-fi artifact",
}
VARIANTS = {
 "a": "classic full-length rifle silhouette with shoulder stock",
 "b": "bullpup silhouette with compact rear-set receiver",
 "c": "long-barrel marksman silhouette with extended slender barrel",
 "d": "compact carbine silhouette with short barrel and sturdy stock",
}

def _prompt_for(name):
    lvl, var = name.rsplit("_", 1)
    return BASE + LEVELS[lvl] + ", " + VARIANTS[var] + ", " + MOUNT

def _api(path, payload=None):
    url = "https://openapi.tripo3d.ai/v3" + path
    data = json.dumps(payload).encode() if payload is not None else None
    req = urllib.request.Request(url, data=data, method="POST" if data else "GET",
        headers={"Authorization": "Bearer " + bpy.context.scene.api_key,
                 "Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.loads(r.read().decode())

def pump():
    with open(CKPT) as f:
        ck = json.load(f)
    tasks = ck["tasks"]
    pending = ck.get("pending_submission", [])

    # 1. batch status query
    ids = [t["task_id"] for t in tasks.values()]
    if ids:
        res = _api("/tasks/list", {"task_ids": ids})
        details = res.get("data", {}).get("tasks", res.get("tasks", {}))
        for name, t in tasks.items():
            d = details.get(t["task_id"])
            if not d:
                continue
            t["status"] = d.get("status", t.get("status"))
            t["progress"] = d.get("progress")
            out = d.get("output") or {}
            if t["status"] == "success" and not t.get("downloaded"):
                url = out.get("pbr_model") or out.get("model_url") or out.get("model")
                if isinstance(url, dict):
                    url = url.get("url")
                if url:
                    dest = os.path.join(OUT, name + ".glb")
                    urllib.request.urlretrieve(url, dest)
                    t["downloaded"] = dest

    # 2. submit pending while capacity allows -- via the addon's TripoClient (the proven
    # submission path; the raw v3 POST /task guess 404'd). One at a time, checkpoint after
    # each, stop on the first error (rate limit).
    inflight = sum(1 for t in tasks.values()
                   if t.get("status") in ("recovered", "submitted", "queued", "running"))
    submitted_now, rate_limited = [], False
    if pending and inflight < MAX_INFLIGHT:
        import sys, asyncio
        TC = sys.modules["tripo-3d-for-blender.tripo3d.client"].TripoClient
        async def submit_batch():
            nonlocal inflight, rate_limited
            client = TC(api_key=bpy.context.scene.api_key)
            try:
                while pending and inflight < MAX_INFLIGHT and not rate_limited:
                    name = pending[0]
                    try:
                        tid = await client.text_to_model(prompt=_prompt_for(name),
                            negative_prompt=NEG, texture=True, pbr=True)
                        tasks[name] = {"task_id": tid, "status": "submitted"}
                        pending.pop(0)
                        submitted_now.append(name)
                        inflight += 1
                        with open(CKPT, "w") as f:
                            json.dump({"tasks": tasks, "pending_submission": pending}, f, indent=1)
                    except Exception as e:
                        rate_limited = True
                        ck["last_submit_error"] = str(e)[:200]
            finally:
                await client.close()
        asyncio.run(submit_batch())

    ck["tasks"] = tasks
    ck["pending_submission"] = pending
    with open(CKPT, "w") as f:
        json.dump(ck, f, indent=1)

    by_status = {}
    for t in tasks.values():
        by_status[t.get("status", "?")] = by_status.get(t.get("status", "?"), 0) + 1
    return {"statuses": by_status,
            "downloaded": sum(1 for t in tasks.values() if t.get("downloaded")),
            "pending_submission": len(pending), "submitted_now": submitted_now,
            "rate_limited": rate_limited}
