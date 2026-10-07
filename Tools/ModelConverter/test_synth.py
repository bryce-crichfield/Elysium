import json, struct, numpy as np, tempfile
from pathlib import Path
import bake_core as bc

def glb(gltf, binblob):
    j = json.dumps(gltf).encode(); j += b" " * ((4 - len(j) % 4) % 4)
    binblob += b"\0" * ((4 - len(binblob) % 4) % 4)
    total = 12 + 8 + len(j) + 8 + len(binblob)
    return (struct.pack("<III", 0x46546C67, 2, total) + struct.pack("<II", len(j), 0x4E4F534A) + j
            + struct.pack("<II", len(binblob), 0x004E4942) + binblob)

class B:
    def __init__(s): s.data = b""; s.views = []; s.acc = []
    def add(s, arr, ctype, typ, normalized=False):
        arr = np.ascontiguousarray(arr)
        off = len(s.data); s.data += arr.tobytes(); s.data += b"\0" * ((4 - len(s.data) % 4) % 4)
        s.views.append({"buffer": 0, "byteOffset": off, "byteLength": arr.nbytes})
        a = {"bufferView": len(s.views) - 1, "componentType": ctype, "count": len(arr), "type": typ}
        if normalized: a["normalized"] = True
        s.acc.append(a); return len(s.acc) - 1

qx = -np.sqrt(0.5)
nodes_base = [
    {"name": "Armature", "scale": [0.01]*3, "rotation": [qx, 0, 0, np.sqrt(0.5)], "children": [1, 3]},
    {"name": "mixamorig:Hips", "translation": [0, 100, 0], "children": [2]},
    {"name": "mixamorig:Spine", "translation": [0, 10, 0]},
]
def world(nodes, i, par):
    m = np.eye(4)
    chain = []
    while i is not None: chain.append(i); i = par.get(i)
    for k in reversed(chain):
        n = nodes[k]; m = m @ bc.trs_to_mat(n.get("translation",[0,0,0]), n.get("rotation",[0,0,0,1]), n.get("scale",[1,1,1]))
    return m
par = {1:0, 3:0, 2:1}

def build_anim(b, nodes):
    t = np.array([0, 1.0], np.float32)
    tin = b.add(t, 5126, "SCALAR")
    hips_t = b.add(np.array([[0,100,0],[0,100,50]], np.float32), 5126, "VEC3")
    # spine rotation: identity -> 90deg about Z
    c = np.sqrt(0.5)
    spine_r = b.add(np.array([[0,0,0,1],[0,0,c,c]], np.float32), 5126, "VEC4")
    return {"animations":[{"samplers":[{"input":tin,"output":hips_t,"interpolation":"LINEAR"},{"input":tin,"output":spine_r,"interpolation":"LINEAR"}],
        "channels":[{"sampler":0,"target":{"node":1,"path":"translation"}},{"sampler":1,"target":{"node":2,"path":"rotation"}}]}]}

tmp = Path(tempfile.mkdtemp())
# ---- model with skin
b = B()
pos = np.array([[0,1,0],[0.2,1.1,0],[-0.2,1.1,0]], np.float32)
pa = b.add(pos, 5126, "VEC3")
ja = b.add(np.array([[0,1,0,0]]*3, np.uint8), 5121, "VEC4")      # skin joint idx 0 (spine), 1 (hips)
wa = b.add(np.array([[0.5,0.5,0,0]]*3, np.float32), 5126, "VEC4")
ia = b.add(np.array([0,1,2], np.uint16), 5123, "SCALAR")
joints = [2, 1]
ibm = np.array([np.linalg.inv(world(nodes_base, n, par)).T for n in joints], np.float32).reshape(2,16)
iba = b.add(ibm, 5126, "MAT4") if False else None
# MAT4 accessor: count=2, type MAT4
arr = np.ascontiguousarray(ibm); off = len(b.data); b.data += arr.tobytes()
b.views.append({"buffer":0,"byteOffset":off,"byteLength":arr.nbytes}); b.acc.append({"bufferView":len(b.views)-1,"componentType":5126,"count":2,"type":"MAT4"}); iba = len(b.acc)-1
nodes = [dict(n) for n in nodes_base] + [{"name":"Body","mesh":0,"skin":0}]
nodes[3]["scale"] = [100,100,100]          # junk mesh-node transform (should be ignored)
g = {"asset":{"version":"2.0"},"nodes":nodes,"scenes":[{"nodes":[0]}],
     "meshes":[{"primitives":[{"attributes":{"POSITION":pa,"JOINTS_0":ja,"WEIGHTS_0":wa},"indices":ia}]}],
     "skins":[{"joints":joints,"inverseBindMatrices":iba}]}
g.update(build_anim(b, nodes)); g["accessors"]=b.acc; g["bufferViews"]=b.views; g["buffers"]=[{"byteLength":len(b.data)}]
(tmp/"Model.glb").write_bytes(glb(g, b.data))

# ---- animation only
b2 = B()
g2 = {"asset":{"version":"2.0"},"nodes":[dict(n) for n in nodes_base],"scenes":[{"nodes":[0]}]}
g2.update(build_anim(b2, nodes_base)); g2["accessors"]=b2.acc; g2["bufferViews"]=b2.views; g2["buffers"]=[{"byteLength":len(b2.data)}]
(tmp/"Wave.glb").write_bytes(glb(g2, b2.data))

out = tmp/"out"
logs = []
ok, bad = bc.bake_batch([tmp/"Model.glb", tmp/"Wave.glb"], out, None, bc.Options(fps=10), log=lambda m: (logs.append(m), print(m)))
assert ok == 2 and bad == 0, (ok, bad)

# ---- read back .skel
def rstr(d, o): n = struct.unpack_from("<H", d, o)[0]; return d[o+2:o+2+n].decode(), o+2+n
d = (out/"Model.skel").read_bytes(); assert d[:4] == b"SKEL"
cnt = struct.unpack_from("<II", d, 4)[1]; o = 12; bones = []
for _ in range(cnt):
    name, o = rstr(d, o); p = struct.unpack_from("<i", d, o)[0]; o += 4
    f = np.frombuffer(d, "<f4", 10 + 16, o); o += 26*4
    bones.append((name, p, f[:3].astype(float), f[3:7].astype(float), f[7:10].astype(float), f[10:].reshape(4,4).T.astype(float)))
print([(b_[0], b_[1]) for b_ in bones])
assert [b_[0] for b_ in bones] == ["Hips", "Spine"] and bones[1][1] == 0
W = []
for name, p, t, q, s, ibm_ in bones:
    m = bc.trs_to_mat(t, q, s); W.append(m if p < 0 else W[p] @ m)
for k, (name, p, t, q, s, ibm_) in enumerate(bones):
    assert np.allclose(W[k] @ ibm_, np.eye(4), atol=1e-5), (name, W[k] @ ibm_)
print("skin matrices at bind == identity: OK;  root scale:", bones[0][4], " spine T:", bones[1][2])

# ---- .mesh
d = (out/"Model.mesh").read_bytes(); assert d[:4] == b"MESH"
ver, nv, ni, ns, nb = struct.unpack_from("<IIIII", d, 4); o = 24
name, o = rstr(d, o); mat, o = rstr(d, o); o += 16; tex, o = rstr(d, o); first, cntI = struct.unpack_from("<II", d, o); o += 8
verts = np.frombuffer(d, bc.VERTEX_DTYPE, nv, o); o += nv*56; idx = np.frombuffer(d, "<u4", ni, o)
print("mesh:", nv, ni, ns, nb, verts["j"][0], verts["w"][0], idx)
assert list(verts["j"][0]) == [1, 0, 1, 1]   # skin joints [spine,hips] -> bones [1,0]; padding idx 0 -> spine -> 1
assert np.allclose(verts["w"][0], [0.5, 0.5, 0, 0])

# ---- .anim: rebuild world matrices per frame and compare with original (unfolded) math
d = (out/"Wave.anim").read_bytes(); assert d[:4] == b"ANIM"
ver, fps, dur, F, NT = struct.unpack_from("<IffII", d, 4); o = 24
tracks = {}
for _ in range(NT):
    name, o = rstr(d, o); fl = d[o]; o += 1; ch = []
    for bit, w in enumerate((3, 4, 3)):
        n = 1 if fl & (1 << bit) else F
        a = np.frombuffer(d, "<f4", n*w, o).reshape(n, w).astype(float); o += n*w*4
        ch.append(np.tile(a, (F, 1)) if n == 1 else a)
    tracks[name] = ch
print("anim:", fps, dur, F, NT, {k: [x.shape for x in v] for k, v in tracks.items()})
c = np.sqrt(0.5)
for i in range(F):
    a = i / (F - 1)
    # original: hips translation lerp, spine rotation slerp(identity -> 90deg Z)
    ang = a * np.pi / 2
    nodes_t = [dict(n) for n in nodes_base]
    nodes_t[1]["translation"] = [0, 100, 50 * a]
    nodes_t[2]["rotation"] = [0, 0, np.sin(ang/2), np.cos(ang/2)]
    Wo_h, Wo_s = world(nodes_t, 1, par), world(nodes_t, 2, par)
    h = bc.trs_to_mat(tracks["Hips"][0][i], tracks["Hips"][1][i], tracks["Hips"][2][i])
    sp = bc.trs_to_mat(tracks["Spine"][0][i], tracks["Spine"][1][i], tracks["Spine"][2][i])
    Wn_h, Wn_s = h, h @ sp
    D = np.diag([0.01, 0.01, 0.01, 1])
    assert np.allclose(Wn_h @ D, Wo_h, atol=1e-5), i
    assert np.allclose(Wn_s @ D, Wo_s, atol=1e-5), i
print("animation world matrices match original for all", F, "frames: OK")
# the model-embedded clip should be identical to the anim-only clip
a1 = (out/"Model.anim").read_bytes(); a2 = (out/"Wave.anim").read_bytes()
assert a1 == a2, "skinned-file anim differs from anim-only file"
print("Model.anim == Wave.anim: OK")
