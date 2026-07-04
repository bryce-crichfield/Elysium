"""2D affine transform parsing, composition, and decomposition for the SVG exporter.

Matrices are represented as the 6-tuple (a, b, c, d, e, f) matching SVG's own
`matrix(a,b,c,d,e,f)` convention:

    x' = a*x + c*y + e
    y' = b*x + d*y + f
"""
from __future__ import annotations

import math
import re
from dataclasses import dataclass
from typing import List, Tuple

Matrix = Tuple[float, float, float, float, float, float]

IDENTITY: Matrix = (1.0, 0.0, 0.0, 1.0, 0.0, 0.0)


def multiply(m1: Matrix, m2: Matrix) -> Matrix:
    """Returns m1 * m2 (m2 is applied to the point first, then m1)."""
    a1, b1, c1, d1, e1, f1 = m1
    a2, b2, c2, d2, e2, f2 = m2
    return (
        a1 * a2 + c1 * b2,
        b1 * a2 + d1 * b2,
        a1 * c2 + c1 * d2,
        b1 * c2 + d1 * d2,
        a1 * e2 + c1 * f2 + e1,
        b1 * e2 + d1 * f2 + f1,
    )


def apply(m: Matrix, x: float, y: float) -> Tuple[float, float]:
    a, b, c, d, e, f = m
    return (a * x + c * y + e, b * x + d * y + f)


def translate(tx: float, ty: float = 0.0) -> Matrix:
    return (1.0, 0.0, 0.0, 1.0, tx, ty)


def scale(sx: float, sy: float | None = None) -> Matrix:
    if sy is None:
        sy = sx
    return (sx, 0.0, 0.0, sy, 0.0, 0.0)


def rotate(angle_deg: float, cx: float = 0.0, cy: float = 0.0) -> Matrix:
    rad = math.radians(angle_deg)
    cs, sn = math.cos(rad), math.sin(rad)
    rot: Matrix = (cs, sn, -sn, cs, 0.0, 0.0)
    if cx == 0.0 and cy == 0.0:
        return rot
    return multiply(multiply(translate(cx, cy), rot), translate(-cx, -cy))


def skew_x(angle_deg: float) -> Matrix:
    return (1.0, 0.0, math.tan(math.radians(angle_deg)), 1.0, 0.0, 0.0)


def skew_y(angle_deg: float) -> Matrix:
    return (1.0, math.tan(math.radians(angle_deg)), 0.0, 1.0, 0.0, 0.0)


_TOKEN_RE = re.compile(r"(\w+)\s*\(([^)]*)\)")
_NUM_RE = re.compile(r"[-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?")


def _parse_args(raw: str) -> List[float]:
    return [float(n) for n in _NUM_RE.findall(raw)]


def parse_transform(transform_str: str | None) -> Matrix:
    """Parses an SVG `transform` attribute string into a single composed Matrix."""
    if not transform_str:
        return IDENTITY

    acc = IDENTITY
    for name, raw_args in _TOKEN_RE.findall(transform_str):
        args = _parse_args(raw_args)
        name = name.strip().lower()
        token: Matrix
        if name == "translate":
            tx = args[0] if len(args) > 0 else 0.0
            ty = args[1] if len(args) > 1 else 0.0
            token = translate(tx, ty)
        elif name == "scale":
            sx = args[0] if len(args) > 0 else 1.0
            sy = args[1] if len(args) > 1 else sx
            token = scale(sx, sy)
        elif name == "rotate":
            angle = args[0] if len(args) > 0 else 0.0
            cx = args[1] if len(args) > 1 else 0.0
            cy = args[2] if len(args) > 2 else 0.0
            token = rotate(angle, cx, cy)
        elif name == "skewx":
            token = skew_x(args[0] if args else 0.0)
        elif name == "skewy":
            token = skew_y(args[0] if args else 0.0)
        elif name == "matrix":
            if len(args) != 6:
                continue
            token = tuple(args)  # type: ignore[assignment]
        else:
            continue
        acc = multiply(acc, token)
    return acc


@dataclass
class Decomposed:
    x: float
    y: float
    scale_x: float
    scale_y: float
    rotation_deg: float
    has_skew: bool
    skew_x: float = 0.0


def bake_point(
    x: float, y: float, rotation_deg: float, scale_x: float, scale_y: float, skew_x: float = 0.0,
) -> Tuple[float, float]:
    """Applies accumulated shear-then-scale-then-rotate distortion to a local offset vector.

    Mirrors TransformSystem::ComposeRecursive's own composition order exactly
    (for the rotate/scale part). Used to bake ancestor+own rotation/scale
    directly into a shape's own geometry, since the renderers for Rectangle/
    Ellipse/Line/Polygon only ever read an entity's world *position* — they
    never apply its worldRotation or worldScaleX/Y to their own draw calls.
    Position hierarchy (ParentComponent) still composes correctly at runtime;
    shape geometry does not, so it must be pre-distorted at export time.

    skew_x applies in the point's own pristine local frame, i.e. before scale/
    rotate (this matches decompose()'s M = R * S * ShearX factorization).
    Unlike rotation/scale, there is no TransformComponent-level equivalent for
    skew at runtime, so any ancestor skew reaching a leaf must also be baked
    in here (accumulated the same additive way as rotation).
    """
    if skew_x != 0.0:
        x = x + skew_x * y
    sx, sy = x * scale_x, y * scale_y
    rad = math.radians(rotation_deg)
    cs, sn = math.cos(rad), math.sin(rad)
    return (sx * cs - sy * sn, sx * sn + sy * cs)


def shear_offset(x: float, y: float, skew_x: float) -> Tuple[float, float]:
    """Applies a skewX-style shear correction to a position *offset* from an
    entity's immediate parent (not a shape's own relative geometry — see
    bake_point for that).

    When an ancestor <g>/<use> has real shear on its own transform, that shear
    can't be represented by TransformComponent (no skew field), so the real
    ParentComponent hierarchy can never apply it at runtime. The only place
    left to apply it is here: pre-shearing the child's local offset from that
    ancestor at export time, exactly the amount the ancestor's own shear would
    have contributed. Composes additively with bake_point's per-shape skew_x,
    since shear applies uniformly to every point measured from a common origin
    (verified against a directly-computed true-affine reference case).
    """
    if skew_x == 0.0:
        return x, y
    return x + skew_x * y, y


def decompose(m: Matrix) -> Decomposed:
    """Decomposes a Matrix into TransformComponent-compatible fields, plus a
    skew_x shear factor for callers (bake_point) that can fold shear directly
    into baked point geometry.

    Factorization used: M = R(rotation) * S(scale_x, scale_y) * ShearX(skew_x),
    i.e. a QR-style decomposition. This covers skewX, skewY, and general
    matrix() shear uniformly — skewY, for instance, decomposes into a nonzero
    rotation + scale + skew_x rather than a separate "skew_y" field, which is
    fine since bake_point only ever needs to reproduce the final matrix, not
    the original SVG token that produced it.
    """
    a, b, c, d, e, f = m

    scale_x = math.hypot(a, b)
    rotation_rad = math.atan2(b, a)

    cs, sn = math.cos(rotation_rad), math.sin(rotation_rad)
    # Un-rotate the (c, d) basis vector to check it lands on the y-axis alone.
    rx = c * cs + d * sn
    ry = -c * sn + d * cs

    has_skew = abs(rx) > 1e-3 * max(1.0, abs(ry))
    skew_x = (rx / scale_x) if abs(scale_x) > 1e-9 else 0.0

    return Decomposed(
        x=e,
        y=f,
        scale_x=scale_x,
        scale_y=ry,
        rotation_deg=math.degrees(rotation_rad),
        has_skew=has_skew,
        skew_x=skew_x,
    )
