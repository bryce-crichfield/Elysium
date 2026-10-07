"""
bake_core.py - Mixamo FBX/GLB  ->  .skel / .mesh / .anim

Pipeline:  FBX --(Blender, headless)--> GLB --(this file)--> .skel + .mesh + .anim
GLB inputs skip the Blender step.

Requires: Python 3.9+, numpy, Blender (for FBX input only).

CLI:  python bake_core.py "Walking.fbx" "Model.fbx" -o out_dir [--blender PATH] [--fps 30]

==============================  OUTPUT FORMATS  ==============================
All little-endian. Right-handed, Y-up, meters (glTF conventions). Quaternions are
(x, y, z, w). Matrices are 16 floats, COLUMN-major. Strings are u16 length + UTF-8
bytes (no terminator). Bone names have the "mixamorig:" prefix stripped.
Bones are stored parent-before-child, so world matrices can be built in one pass.

.skel   "SKEL" u32 version=1, u32 boneCount
        per bone:  string name | i32 parent (-1 = root) |
                   f32[3] bindT | f32[4] bindQ | f32[3] bindS |   (local, relative to parent)
                   f32[16] inverseBind
        skinMatrix = boneWorld * inverseBind

.mesh   "MESH" u32 version=1, u32 vertexCount, u32 indexCount, u32 submeshCount, u32 boneCount
        per submesh: string name | string material | f32[4] baseColor |
                     string baseColorTexture (file name next to the .mesh, may be "") |
                     u32 firstIndex | u32 indexCount
        vertices (vertexCount * 56 bytes, packed):
                     f32[3] pos | f32[3] normal | f32[2] uv | u16[4] joints | f32[4] weights
        indices: u32[indexCount]   (triangle list, CCW front faces; UV origin top-left)
        joints index into the .skel bone array. Weights are normalized (sum to 1).

.anim   "ANIM" u32 version=1, f32 fps, f32 duration, u32 frameCount, u32 trackCount
        per track:   string boneName | u8 flags | T | R | S
                     flags bit0/1/2 = T/R/S is constant -> that channel stores ONE value
                     otherwise it stores frameCount values (T: f32[3], R: f32[4], S: f32[3])
        Frame i is at time i / fps. Tracks are keyed by bone NAME (resolve to indices
        once when you bind a clip to a skeleton). Bones with no track stay at bind pose.
===============================================================================
"""
from __future__ import annotations

import argparse
import glob
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import traceback
from dataclasses import dataclass
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
BLENDER_SCRIPT = HERE / "blender_fbx_to_glb.py"

_PREFIX = re.compile(r"^mixamorig\d*[:_]", re.I)


def clean_name(name: str) -> str:
    return _PREFIX.sub("", name)


@dataclass
class Options:
    write_skel: bool = True
    write_mesh: bool = True
    write_anim: bool = True
    fps: float = 30.0
    keep_glb: bool = False


# --------------------------------------------------------------------------- math
def quat_to_mat3(q):
    x, y, z, w = q
    return np.array([
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
    ])


def mat3_to_quat(m):
    tr = m[0, 0] + m[1, 1] + m[2, 2]
    if tr > 0:
        s = math.sqrt(tr + 1.0) * 2
        w, x, y, z = 0.25 * s, (m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s
    elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
        s = math.sqrt(1.0 + m[0, 0] - m[1, 1] - m[2, 2]) * 2
        w, x, y, z = (m[2, 1] - m[1, 2]) / s, 0.25 * s, (m[0, 1] + m[1, 0]) / s, (m[0, 2] + m[2, 0]) / s
    elif m[1, 1] > m[2, 2]:
        s = math.sqrt(1.0 + m[1, 1] - m[0, 0] - m[2, 2]) * 2
        w, x, y, z = (m[0, 2] - m[2, 0]) / s, (m[0, 1] + m[1, 0]) / s, 0.25 * s, (m[1, 2] + m[2, 1]) / s
    else:
        s = math.sqrt(1.0 + m[2, 2] - m[0, 0] - m[1, 1]) * 2
        w, x, y, z = (m[1, 0] - m[0, 1]) / s, (m[0, 2] + m[2, 0]) / s, (m[1, 2] + m[2, 1]) / s, 0.25 * s
    q = np.array([x, y, z, w])
    return q / np.linalg.norm(q)


def trs_to_mat(t, q, s):
    q = np.asarray(q, float)
    q = q / np.linalg.norm(q)
    m = np.eye(4)
    m[:3, :3] = quat_to_mat3(q) * np.asarray(s, float)[None, :]
    m[:3, 3] = t
    return m


def decompose(m):
    t = m[:3, 3].copy()
    c = m[:3, :3]
    s = np.linalg.norm(c, axis=0)
    if np.linalg.det(c) < 0:
        s[0] = -s[0]
    safe = np.where(np.abs(s) < 1e-12, 1.0, s)
    return t, mat3_to_quat(c / safe[None, :]), s


def slerp_arrays(q0, q1, a):
    d = np.sum(q0 * q1, axis=1)
    q1 = np.where((d < 0)[:, None], -q1, q1)
    d = np.abs(d)
    theta = np.arccos(np.clip(d, -1.0, 1.0))
    sin_t = np.sin(theta)
    sin_safe = np.where(sin_t < 1e-8, 1.0, sin_t)
    w0 = np.sin((1 - a) * theta) / sin_safe
    w1 = np.sin(a * theta) / sin_safe
    spherical = q0 * w0[:, None] + q1 * w1[:, None]
    linear = q0 + (q1 - q0) * a[:, None]
    out = np.where((d > 0.9995)[:, None], linear, spherical)
    return out / np.linalg.norm(out, axis=1, keepdims=True)


# --------------------------------------------------------------------------- GLB reader
_COMP = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
_NCOMP = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT2": 4, "MAT3": 9, "MAT4": 16}


class Gltf:
    def __init__(self, path):
        data = Path(path).read_bytes()
        magic, _version, length = struct.unpack_from("<III", data, 0)
        if magic != 0x46546C67:
            raise ValueError(f"{path} is not a binary glTF (.glb)")
        off, self.j, self.bin = 12, None, b""
        while off < min(length, len(data)):
            clen, ctype = struct.unpack_from("<II", data, off)
            off += 8
            chunk = data[off:off + clen]
            off += clen
            if ctype == 0x4E4F534A:
                self.j = json.loads(chunk.decode("utf-8"))
            elif ctype == 0x004E4942 and not self.bin:
                self.bin = chunk
        if self.j is None:
            raise ValueError("GLB has no JSON chunk")
        self.nodes = self.j.get("nodes", [])
        self.parents = {}
        for i, n in enumerate(self.nodes):
            for c in n.get("children", []):
                self.parents[c] = i
        self._world = {}

    def accessor(self, idx):
        a = self.j["accessors"][idx]
        if "sparse" in a:
            raise NotImplementedError("sparse accessors are not supported")
        dtype = np.dtype(_COMP[a["componentType"]]).newbyteorder("<")
        n, count = _NCOMP[a["type"]], a["count"]
        if "bufferView" not in a:
            return np.zeros((count, n), dtype)
        bv = self.j["bufferViews"][a["bufferView"]]
        off = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
        stride = bv.get("byteStride") or n * dtype.itemsize
        arr = np.ndarray((count, n), dtype, buffer=self.bin, offset=off, strides=(stride, dtype.itemsize))
        arr = np.array(arr)
        if a.get("normalized"):
            kind = dtype.kind, dtype.itemsize
            denom = {("u", 1): 255.0, ("u", 2): 65535.0, ("i", 1): 127.0, ("i", 2): 32767.0}[kind]
            arr = np.maximum(arr.astype(np.float32) / denom, -1.0)
        return arr

    def local_matrix(self, i):
        n = self.nodes[i]
        if "matrix" in n:
            return np.array(n["matrix"], float).reshape(4, 4).T
        return trs_to_mat(n.get("translation", [0, 0, 0]), n.get("rotation", [0, 0, 0, 1]), n.get("scale", [1, 1, 1]))

    def node_trs(self, i):
        n = self.nodes[i]
        if "matrix" in n:
            return decompose(self.local_matrix(i))
        q = np.array(n.get("rotation", [0, 0, 0, 1]), float)
        return np.array(n.get("translation", [0, 0, 0]), float), q / np.linalg.norm(q), np.array(n.get("scale", [1, 1, 1]), float)

    def world(self, i):
        if i is None:
            return np.eye(4)
        if i not in self._world:
            p = self.parents.get(i)
            self._world[i] = (self.world(p) if p is not None else np.eye(4)) @ self.local_matrix(i)
        return self._world[i]


def uniform_scale_of(m):
    s = abs(np.linalg.det(m[:3, :3])) ** (1.0 / 3.0)
    return 1.0 if abs(s - 1.0) < 1e-4 else float(s)


# --------------------------------------------------------------------------- skeleton
@dataclass
class Skeleton:
    names: list
    parent: list
    t: np.ndarray
    q: np.ndarray
    s: np.ndarray
    ibm: np.ndarray            # (N,4,4) true matrices
    node_to_bone: dict
    scale: float               # armature scale that was folded out (e.g. 0.01 from cm FBX)


def extract_skeleton(g: Gltf, log):
    skins = g.j.get("skins")
    if not skins:
        return None
    joints = skins[0]["joints"]
    n = len(joints)
    jset = set(joints)
    skin_index = {node: k for k, node in enumerate(joints)}

    if "inverseBindMatrices" in skins[0]:
        ibm0 = g.accessor(skins[0]["inverseBindMatrices"]).astype(np.float64).reshape(n, 4, 4).transpose(0, 2, 1)
    else:
        ibm0 = np.tile(np.eye(4), (n, 1, 1))

    def joint_parent(node):
        p = g.parents.get(node)
        while p is not None and p not in jset:
            p = g.parents.get(p)
        return p

    jp = {node: joint_parent(node) for node in joints}
    children, roots = {node: [] for node in joints}, []
    for node in joints:
        (roots if jp[node] is None else children[jp[node]]).append(node)

    order, stack = [], list(reversed(roots))          # parent-before-child DFS
    while stack:
        nd = stack.pop()
        order.append(nd)
        stack.extend(reversed(children[nd]))
    node_to_bone = {nd: i for i, nd in enumerate(order)}

    # The armature node (and anything above it) usually carries the cm->m scale and the
    # Z-up->Y-up rotation. Fold all of that into the root bone and push the uniform
    # scale into bone translations, so the runtime skeleton is plain unit-scale TRS.
    s = uniform_scale_of(g.world(g.parents.get(roots[0])))
    D = np.diag([s, s, s, 1.0])
    if abs(s - 1.0) > 1e-6:
        log(f"  armature scale {s:g} folded into skeleton")

    rest_dev = max(float(np.abs(g.world(nd) @ ibm0[skin_index[nd]] - np.eye(4)).max()) for nd in joints)
    if rest_dev > 1e-3:
        log(f"  WARNING: bind pose and inverse-bind differ by {rest_dev:.3g} (mesh node has a transform?)")

    names, parent = [], []
    T, Q, S, IBM = [], [], [], []
    for nd in order:
        p = jp[nd]
        if p is None:
            t, q, sc = decompose(g.world(nd))
            sc = sc / s
        else:
            t, q, sc = decompose(np.linalg.inv(g.world(p)) @ g.world(nd))
            t = t * s
        names.append(clean_name(g.nodes[nd].get("name", f"bone{nd}")))
        parent.append(-1 if p is None else node_to_bone[p])
        T.append(t), Q.append(q), S.append(sc)
        IBM.append(D @ ibm0[skin_index[nd]])
    return Skeleton(names, parent, np.array(T), np.array(Q), np.array(S), np.array(IBM), node_to_bone, s)


# --------------------------------------------------------------------------- mesh
def _write_image(g, img_idx, out_dir, stem, log):
    img = g.j["images"][img_idx]
    if "bufferView" not in img:
        log(f"  texture {img_idx} is an external URI; not copied")
        return Path(img.get("uri", "")).name
    bv = g.j["bufferViews"][img["bufferView"]]
    off = bv.get("byteOffset", 0)
    ext = {"image/png": ".png", "image/jpeg": ".jpg"}.get(img.get("mimeType"), ".bin")
    name = f"{stem}_tex{img_idx}{ext}"
    (out_dir / name).write_bytes(g.bin[off:off + bv["byteLength"]])
    return name


def extract_mesh(g: Gltf, skel, out_dir: Path, stem: str, log):
    verts, inds, subs, tex_cache = [], [], [], {}
    base = first = 0
    for ni, node in enumerate(g.nodes):
        if "mesh" not in node:
            continue
        mesh = g.j["meshes"][node["mesh"]]
        jmap = None
        if skel is not None and "skin" in node:
            jl = g.j["skins"][node["skin"]]["joints"]
            if any(x not in skel.node_to_bone for x in jl):
                log(f"  WARNING: mesh '{node.get('name')}' uses joints missing from skeleton (mapped to bone 0)")
            jmap = np.array([skel.node_to_bone.get(x, 0) for x in jl], np.int64)
        for pi, prim in enumerate(mesh.get("primitives", [])):
            if prim.get("mode", 4) != 4:
                log("  skipping non-triangle primitive")
                continue
            at = prim["attributes"]
            pos = g.accessor(at["POSITION"]).astype(np.float32)
            n = len(pos)
            nrm = g.accessor(at["NORMAL"]).astype(np.float32) if "NORMAL" in at else np.tile(np.float32([0, 1, 0]), (n, 1))
            uv = g.accessor(at["TEXCOORD_0"]).astype(np.float32) if "TEXCOORD_0" in at else np.zeros((n, 2), np.float32)
            if jmap is not None and "JOINTS_0" in at and "WEIGHTS_0" in at:
                jj = jmap[g.accessor(at["JOINTS_0"]).astype(np.int64)]
                ww = g.accessor(at["WEIGHTS_0"]).astype(np.float32)
                tot = ww.sum(axis=1, keepdims=True)
                ww = np.where(tot > 1e-8, ww / np.where(tot > 1e-8, tot, 1), np.float32([1, 0, 0, 0]))
            else:
                jj = np.zeros((n, 4), np.int64)
                ww = np.tile(np.float32([1, 0, 0, 0]), (n, 1))
            idx = g.accessor(prim["indices"]).reshape(-1).astype(np.uint32) if "indices" in prim else np.arange(n, dtype=np.uint32)

            mat_name, color, tex = "", [1, 1, 1, 1], ""
            if prim.get("material") is not None:
                m = g.j["materials"][prim["material"]]
                pbr = m.get("pbrMetallicRoughness", {})
                mat_name, color = m.get("name", f"material{prim['material']}"), pbr.get("baseColorFactor", [1, 1, 1, 1])
                ti = pbr.get("baseColorTexture", {}).get("index")
                src = g.j["textures"][ti].get("source") if ti is not None else None
                if src is not None:
                    if src not in tex_cache:
                        tex_cache[src] = _write_image(g, src, out_dir, stem, log)
                    tex = tex_cache[src]

            v = np.empty(n, VERTEX_DTYPE)
            v["pos"], v["nrm"], v["uv"], v["j"], v["w"] = pos, nrm, uv, jj, ww
            verts.append(v)
            inds.append(idx + np.uint32(base))
            subs.append((f"{node.get('name', mesh.get('name', 'mesh'))}_{pi}", mat_name, color, tex, first, len(idx)))
            base += n
            first += len(idx)
    if not verts:
        return None
    return np.concatenate(verts), np.concatenate(inds), subs, (len(skel.names) if skel else 0)


VERTEX_DTYPE = np.dtype([("pos", "<f4", 3), ("nrm", "<f4", 3), ("uv", "<f4", 2), ("j", "<u2", 4), ("w", "<f4", 4)])
assert VERTEX_DTYPE.itemsize == 56


# --------------------------------------------------------------------------- animation
def _sample_vec(times, vals, interp, ts):
    if len(times) == 1:
        return np.tile(vals[0], (len(ts), 1))
    if interp == "STEP":
        return vals[np.clip(np.searchsorted(times, ts, side="right") - 1, 0, len(times) - 1)]
    return np.stack([np.interp(ts, times, vals[:, c]) for c in range(vals.shape[1])], axis=1)


def _sample_quat(times, vals, interp, ts):
    vals = vals / np.linalg.norm(vals, axis=1, keepdims=True)
    if len(times) == 1:
        return np.tile(vals[0], (len(ts), 1))
    i0 = np.clip(np.searchsorted(times, ts, side="right") - 1, 0, len(times) - 2)
    if interp == "STEP":
        return vals[i0]
    a = np.clip((ts - times[i0]) / np.maximum(times[i0 + 1] - times[i0], 1e-12), 0.0, 1.0)
    return slerp_arrays(vals[i0], vals[i0 + 1], a)


@dataclass
class Clip:
    name: str
    fps: float
    duration: float
    frames: int
    tracks: list   # (boneName, T(F,3), R(F,4), S(F,3))


def extract_animations(g: Gltf, skel, stem: str, fps: float, log):
    anims = g.j.get("animations", [])
    if not anims:
        return []
    if skel is not None:
        bone_nodes = set(skel.node_to_bone)
    else:  # animation-only file (Mixamo "Without Skin"): identify bones by name
        bone_nodes = {i for i, n in enumerate(g.nodes) if _PREFIX.match(n.get("name", ""))}
        if not bone_nodes:
            bone_nodes = {c["target"]["node"] for a in anims for c in a["channels"] if "node" in c["target"]}
    roots = sorted(n for n in bone_nodes if g.parents.get(n) not in bone_nodes)
    if not roots:
        return []
    s = skel.scale if skel is not None else uniform_scale_of(g.world(g.parents.get(roots[0])))
    root_set = set(roots)

    clips = []
    for ai, anim in enumerate(anims):
        per_node, t_min, t_max = {}, math.inf, -math.inf
        for ch in anim["channels"]:
            tgt = ch["target"]
            node, path = tgt.get("node"), tgt.get("path")
            if node not in bone_nodes or path not in ("translation", "rotation", "scale"):
                continue
            smp = anim["samplers"][ch["sampler"]]
            times = g.accessor(smp["input"]).reshape(-1).astype(np.float64)
            vals = g.accessor(smp["output"]).astype(np.float64)
            interp = smp.get("interpolation", "LINEAR")
            if interp == "CUBICSPLINE":          # [in-tangent, value, out-tangent] per key; keep values
                vals, interp = vals[1::3], "LINEAR"
            per_node.setdefault(node, {})[path] = (times, vals[:len(times)], interp)
            t_min, t_max = min(t_min, times[0]), max(t_max, times[-1])
        name = stem if len(anims) == 1 else f"{stem}_{anim.get('name') or ai}"
        duration = (t_max - t_min) if per_node else 0.0
        if duration <= 1e-9:
            log(f"  animation '{name}' is a single pose / empty; skipped")
            continue
        frames = int(round(duration * fps)) + 1
        ts = t_min + np.arange(frames) / fps

        tracks = []
        for node in sorted(per_node):
            ch = per_node[node]
            t0, q0, s0 = g.node_trs(node)
            T = _sample_vec(*ch["translation"], ts) if "translation" in ch else np.tile(t0, (frames, 1))
            R = _sample_quat(*ch["rotation"], ts) if "rotation" in ch else np.tile(q0, (frames, 1))
            S = _sample_vec(*ch["scale"], ts) if "scale" in ch else np.tile(s0, (frames, 1))
            if node in root_set:
                A = g.world(g.parents.get(node))
                if not np.allclose(A, np.eye(4), atol=1e-9):
                    for i in range(frames):
                        T[i], R[i], S[i] = decompose(A @ trs_to_mat(T[i], R[i], S[i]))
                    S = S / s
            else:
                T = T * s
            for i in range(1, frames):               # keep quaternions in one hemisphere
                if np.dot(R[i], R[i - 1]) < 0:
                    R[i] = -R[i]
            tracks.append((clean_name(g.nodes[node].get("name", f"bone{node}")), T, R, S))
        clips.append(Clip(name, fps, float(duration), frames, tracks))
    return clips


# --------------------------------------------------------------------------- writers
def _s(buf: bytearray, text: str):
    b = text.encode("utf-8")
    buf += struct.pack("<H", len(b)) + b


def _f32(a) -> bytes:
    return np.ascontiguousarray(a, dtype="<f4").tobytes()


def write_skel(path: Path, sk: Skeleton):
    buf = bytearray(b"SKEL") + struct.pack("<II", 1, len(sk.names))
    for i, name in enumerate(sk.names):
        _s(buf, name)
        buf += struct.pack("<i", sk.parent[i]) + _f32(sk.t[i]) + _f32(sk.q[i]) + _f32(sk.s[i]) + _f32(sk.ibm[i].T)
    path.write_bytes(bytes(buf))


def write_mesh(path: Path, verts, inds, subs, bone_count):
    buf = bytearray(b"MESH") + struct.pack("<IIIII", 1, len(verts), len(inds), len(subs), bone_count)
    for name, mat, color, tex, first, count in subs:
        _s(buf, name), _s(buf, mat)
        buf += _f32(color)
        _s(buf, tex)
        buf += struct.pack("<II", first, count)
    buf += verts.tobytes() + inds.astype("<u4").tobytes()
    path.write_bytes(bytes(buf))


def write_anim(path: Path, clip: Clip):
    buf = bytearray(b"ANIM") + struct.pack("<I", 1) + struct.pack("<ff", clip.fps, clip.duration)
    buf += struct.pack("<II", clip.frames, len(clip.tracks))
    for name, T, R, S in clip.tracks:
        const = [bool(np.allclose(a, a[0], atol=1e-5)) for a in (T, R, S)]
        _s(buf, name)
        buf += struct.pack("<B", sum(1 << i for i, c in enumerate(const) if c))
        for a, c in zip((T, R, S), const):
            buf += _f32(a[:1] if c else a)
    path.write_bytes(bytes(buf))


# --------------------------------------------------------------------------- orchestration
def find_blender():
    exe = shutil.which("blender")
    if exe:
        return exe
    if sys.platform.startswith("win"):
        pf = os.environ.get("ProgramFiles", r"C:\Program Files")
        cands = glob.glob(os.path.join(pf, "Blender Foundation", "Blender *", "blender.exe"))
    elif sys.platform == "darwin":
        cands = ["/Applications/Blender.app/Contents/MacOS/Blender"]
    else:
        cands = ["/usr/bin/blender", "/snap/bin/blender", "/usr/local/bin/blender"]
    cands = [c for c in cands if os.path.exists(c)]
    return sorted(cands)[-1] if cands else None


def run_blender(blender, pairs, log, on_file=lambda: None):
    """Convert [(fbx, glb), ...] in ONE Blender session. Returns the set of GLBs written."""
    job = Path(tempfile.mkdtemp(prefix="mixamo_job_")) / "jobs.json"
    job.write_text(json.dumps([[str(a), str(b)] for a, b in pairs]), encoding="utf-8")
    cmd = [blender, "-b", "--factory-startup", "--python-exit-code", "1",
           "--python", str(BLENDER_SCRIPT), "--", str(job)]
    ok = set()
    try:
        proc = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace",
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        for line in proc.stdout:
            line = line.rstrip()
            if line.startswith("BAKE_OK\t"):
                ok.add(Path(line.split("\t", 1)[1]))
                on_file()
            elif line.startswith("BAKE_FAIL\t"):
                _, src, why = (line.split("\t") + ["", ""])[:3]
                log(f"  Blender failed on {Path(src).name}: {why}")
                on_file()
            elif "error" in line.lower() or "traceback" in line.lower():
                log(f"  [blender] {line}")
        proc.wait()
        if proc.returncode not in (0, None):
            log(f"  Blender exited with code {proc.returncode}")
    finally:
        shutil.rmtree(job.parent, ignore_errors=True)
    return ok


def split_glb(glb: Path, out_dir: Path, stem: str, opts: Options, log):
    g = Gltf(glb)
    skel = extract_skeleton(g, log)
    wrote = []
    if opts.write_skel:
        if skel:
            write_skel(out_dir / f"{stem}.skel", skel)
            wrote.append(f"{stem}.skel ({len(skel.names)} bones)")
        else:
            log("  no skin in this file -> no .skel")
    if opts.write_mesh:
        res = extract_mesh(g, skel, out_dir, stem, log)
        if res:
            verts, inds, subs, nb = res
            write_mesh(out_dir / f"{stem}.mesh", verts, inds, subs, nb)
            lo, hi = verts["pos"].min(axis=0), verts["pos"].max(axis=0)
            wrote.append(f"{stem}.mesh ({len(verts)} verts, {len(inds) // 3} tris, "
                         f"height {hi[1] - lo[1]:.2f} units)")
        else:
            log("  no mesh in this file -> no .mesh")
    if opts.write_anim:
        for clip in extract_animations(g, skel, stem, opts.fps, log):
            write_anim(out_dir / f"{clip.name}.anim", clip)
            wrote.append(f"{clip.name}.anim ({clip.frames} frames, {len(clip.tracks)} tracks, {clip.duration:.2f}s)")
    for w in wrote:
        log(f"  wrote {w}")
    return bool(wrote)


def bake_batch(inputs, out_dir, blender, opts: Options, log=print, progress=lambda f: None):
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="mixamo_bake_"))
    glb_dir = (out_dir / "glb") if opts.keep_glb else work
    glb_dir.mkdir(parents=True, exist_ok=True)
    ok_count = fail_count = 0
    try:
        glbs, pairs = [], []
        for p in map(Path, inputs):
            ext = p.suffix.lower()
            if ext == ".glb":
                glbs.append((p, p))
            elif ext == ".fbx":
                pairs.append((p, glb_dir / f"{p.stem}.glb"))
            else:
                log(f"skipping {p.name} (not .fbx/.glb)")
        total = max(1, len(glbs) + len(pairs))

        converted = set()
        if pairs:
            if not blender or not os.path.exists(blender):
                raise RuntimeError("Blender executable not found - set its path first")
            log(f"Converting {len(pairs)} FBX file(s) with Blender...")
            done = [0]

            def tick():
                done[0] += 1
                progress(0.5 * done[0] / len(pairs))
            converted = run_blender(blender, pairs, log, tick)
        progress(0.5)

        jobs = glbs + [(src, dst) for src, dst in pairs if dst in converted]
        fail_count += len(pairs) - len([1 for _, d in pairs if d in converted])
        for k, (src, glb) in enumerate(jobs):
            log(f"{src.name}")
            try:
                if split_glb(glb, out_dir, src.stem, opts, log):
                    ok_count += 1
                else:
                    log("  nothing written")
            except Exception as e:
                fail_count += 1
                log(f"  FAILED: {e}")
                log("  " + traceback.format_exc().strip().splitlines()[-2].strip())
            progress(0.5 + 0.5 * (k + 1) / max(1, len(jobs)))
    finally:
        shutil.rmtree(work, ignore_errors=True)
    log(f"Done: {ok_count} converted, {fail_count} failed. Output: {out_dir}")
    return ok_count, fail_count


def main():
    ap = argparse.ArgumentParser(description="Mixamo FBX/GLB -> .skel/.mesh/.anim")
    ap.add_argument("inputs", nargs="+")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--blender", default=None)
    ap.add_argument("--fps", type=float, default=30.0)
    ap.add_argument("--no-skel", action="store_true")
    ap.add_argument("--no-mesh", action="store_true")
    ap.add_argument("--no-anim", action="store_true")
    ap.add_argument("--keep-glb", action="store_true")
    a = ap.parse_args()
    opts = Options(not a.no_skel, not a.no_mesh, not a.no_anim, a.fps, a.keep_glb)
    ok, bad = bake_batch(a.inputs, a.out, a.blender or find_blender(), opts)
    sys.exit(1 if bad and not ok else 0)


if __name__ == "__main__":
    main()
