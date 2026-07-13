r"""
finalize_kit_all_ue.py -- run finalize_kit_rifle_ue.py for ALL TEN Mk handoffs in one
editor session (10 separate headless launches would cost ~2min + ~28GB spin-up each).
Copies each _handoff_<body>.json to the fixed _handoff.json path the single-rifle script
reads, then exec()s it with fresh globals.

Run headless (game/build closed):
  .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\kitpipeline\finalize_kit_all_ue.py
"""
import unreal, os, shutil

KP = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\Scripts\kitpipeline"
SINGLE = os.path.join(KP, "finalize_kit_rifle_ue.py")
HANDOFF = os.path.join(KP, "_handoff.json")

BODIES = ["body_mk1_junk", "body_mk2_junk", "body_mk3_rough", "body_mk4_rough",
          "body_mk5_mid", "body_mk6_mid", "body_mk7_mid", "body_mk8_high",
          "body_mk9_high", "body_mk10_sleek"]

src = open(SINGLE).read()
done, failed = [], []
for body in BODIES:
    per = os.path.join(KP, "_handoff_%s.json" % body)
    if not os.path.exists(per):
        unreal.log_warning("LR_KITALL | MISSING handoff for %s, skipped" % body)
        failed.append(body)
        continue
    shutil.copyfile(per, HANDOFF)
    unreal.log_warning("LR_KITALL | ===== %s =====" % body)
    try:
        exec(compile(src, SINGLE, "exec"), {"__name__": "__main__"})
        done.append(body)
    except Exception as e:
        unreal.log_warning("LR_KITALL | FAIL %s: %s" % (body, e))
        failed.append(body)

unreal.log_warning("LR_KITALL | SUMMARY: %d/%d ok%s" % (
    len(done), len(BODIES), (" FAILED: %s" % failed) if failed else ""))
