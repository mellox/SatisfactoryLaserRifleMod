#!/usr/bin/env python
# glb_extract.py -- pull PBR textures out of a Tripo .glb into the MkNN_tex/ layout
# import_tripo.py expects:  Base Color.png (sRGB), Normal.png, Roughness.png(R), Metallic.png(R)
# glTF packs metallic-roughness as one image: R=occlusion, G=roughness, B=metallic -> we split.
# Usage: python glb_extract.py <input.glb> <out_tex_dir>
import struct, json, sys, os
from PIL import Image
import io

def parse_glb(path):
    with open(path, "rb") as f:
        data = f.read()
    magic, ver, length = struct.unpack("<III", data[:12])
    assert magic == 0x46546C67, "not a GLB"
    off = 12; gltf = None; binc = None
    while off < length:
        clen, ctype = struct.unpack("<II", data[off:off+8]); off += 8
        chunk = data[off:off+clen]; off += clen
        if ctype == 0x4E4F534A:   # JSON
            gltf = json.loads(chunk.decode("utf-8"))
        elif ctype == 0x004E4942: # BIN
            binc = chunk
    return gltf, binc

def img_bytes(gltf, binc, img_index):
    im = gltf["images"][img_index]
    bv = gltf["bufferViews"][im["bufferView"]]
    s = bv.get("byteOffset", 0); n = bv["byteLength"]
    return binc[s:s+n]

def tex_to_image_index(gltf, tex_index):
    return gltf["textures"][tex_index]["source"]

def load_pil(gltf, binc, tex_index):
    if tex_index is None: return None
    ii = tex_to_image_index(gltf, tex_index)
    return Image.open(io.BytesIO(img_bytes(gltf, binc, ii)))

def main(glb, outdir):
    os.makedirs(outdir, exist_ok=True)
    gltf, binc = parse_glb(glb)
    mat = gltf.get("materials", [{}])[0]
    pbr = mat.get("pbrMetallicRoughness", {})
    base_t = (pbr.get("baseColorTexture") or {}).get("index")
    mr_t   = (pbr.get("metallicRoughnessTexture") or {}).get("index")
    nrm_t  = (mat.get("normalTexture") or {}).get("index")
    wrote = {}
    base = load_pil(gltf, binc, base_t)
    if base:
        base.convert("RGB").save(os.path.join(outdir, "Base Color.png")); wrote["Base Color"] = base.size
    nrm = load_pil(gltf, binc, nrm_t)
    if nrm:
        nrm.convert("RGB").save(os.path.join(outdir, "Normal.png")); wrote["Normal"] = nrm.size
    mr = load_pil(gltf, binc, mr_t)
    if mr:
        mr = mr.convert("RGB"); r, g, b = mr.split()
        g.save(os.path.join(outdir, "Roughness.png"))   # G = roughness
        b.save(os.path.join(outdir, "Metallic.png"))    # B = metallic
        wrote["Roughness(G)"] = g.size; wrote["Metallic(B)"] = b.size
    print("EXTRACTED", os.path.basename(glb), "->", outdir)
    for k, v in wrote.items(): print("   ", k, v)
    if not wrote: print("   WARNING: no textures found (material/texture indices missing)")
    return wrote

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
