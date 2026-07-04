"""Parser + flattener for SVG `<path d="...">` data.

Supports M/L/H/V/C/S/Q/T/A/Z (absolute and relative). Cubic/quadratic beziers
and elliptical arcs are flattened into straight line segments so the rest of
the exporter only ever deals with polylines/polygons.
"""
from __future__ import annotations

import math
import re
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

Point = Tuple[float, float]


@dataclass
class Subpath:
    points: List[Point] = field(default_factory=list)
    closed: bool = False


class PathSyntaxError(ValueError):
    pass


class _Scanner:
    _NUM_RE = re.compile(r"[-+]?(?:[0-9]*\.[0-9]+|[0-9]+\.?)(?:[eE][-+]?[0-9]+)?")

    def __init__(self, s: str):
        self.s = s
        self.i = 0
        self.n = len(s)

    def _skip_ws(self) -> None:
        while self.i < self.n and (self.s[self.i].isspace() or self.s[self.i] == ","):
            self.i += 1

    def at_end(self) -> bool:
        self._skip_ws()
        return self.i >= self.n

    def peek(self) -> str:
        self._skip_ws()
        return self.s[self.i] if self.i < self.n else ""

    def read_command(self) -> str:
        self._skip_ws()
        c = self.s[self.i]
        self.i += 1
        return c

    def read_number(self) -> float:
        self._skip_ws()
        m = self._NUM_RE.match(self.s, self.i)
        if not m:
            raise PathSyntaxError(f"expected number at position {self.i} in path data: ...{self.s[self.i:self.i+20]!r}")
        self.i = m.end()
        return float(m.group())

    def read_flag(self) -> int:
        self._skip_ws()
        if self.i < self.n and self.s[self.i] in "01":
            v = int(self.s[self.i])
            self.i += 1
            return v
        raise PathSyntaxError(f"expected arc flag (0/1) at position {self.i} in path data")


def _lerp(p0: Point, p1: Point, t: float) -> Point:
    return (p0[0] + (p1[0] - p0[0]) * t, p0[1] + (p1[1] - p0[1]) * t)


def _flatten_cubic(out: List[Point], p0: Point, p1: Point, p2: Point, p3: Point, segments: int) -> None:
    for i in range(1, segments + 1):
        t = i / segments
        mt = 1.0 - t
        x = (mt ** 3) * p0[0] + 3 * (mt ** 2) * t * p1[0] + 3 * mt * (t ** 2) * p2[0] + (t ** 3) * p3[0]
        y = (mt ** 3) * p0[1] + 3 * (mt ** 2) * t * p1[1] + 3 * mt * (t ** 2) * p2[1] + (t ** 3) * p3[1]
        out.append((x, y))


def _flatten_quadratic(out: List[Point], p0: Point, p1: Point, p2: Point, segments: int) -> None:
    for i in range(1, segments + 1):
        t = i / segments
        mt = 1.0 - t
        x = (mt ** 2) * p0[0] + 2 * mt * t * p1[0] + (t ** 2) * p2[0]
        y = (mt ** 2) * p0[1] + 2 * mt * t * p1[1] + (t ** 2) * p2[1]
        out.append((x, y))


def _angle_between(u: Point, v: Point) -> float:
    dot = u[0] * v[0] + u[1] * v[1]
    lu = math.hypot(*u)
    lv = math.hypot(*v)
    cos_a = max(-1.0, min(1.0, dot / (lu * lv))) if lu > 0 and lv > 0 else 1.0
    angle = math.acos(cos_a)
    if u[0] * v[1] - u[1] * v[0] < 0:
        angle = -angle
    return angle


def _flatten_arc(
    out: List[Point],
    start: Point,
    rx: float,
    ry: float,
    x_axis_rotation_deg: float,
    large_arc_flag: int,
    sweep_flag: int,
    end: Point,
    segments: int,
) -> None:
    x1, y1 = start
    x2, y2 = end

    if rx == 0 or ry == 0 or (x1 == x2 and y1 == y2):
        out.append(end)
        return

    rx, ry = abs(rx), abs(ry)
    phi = math.radians(x_axis_rotation_deg)
    cos_phi, sin_phi = math.cos(phi), math.sin(phi)

    dx2, dy2 = (x1 - x2) / 2.0, (y1 - y2) / 2.0
    x1p = cos_phi * dx2 + sin_phi * dy2
    y1p = -sin_phi * dx2 + cos_phi * dy2

    lam = (x1p ** 2) / (rx ** 2) + (y1p ** 2) / (ry ** 2)
    if lam > 1:
        s = math.sqrt(lam)
        rx *= s
        ry *= s

    sign = -1.0 if large_arc_flag == sweep_flag else 1.0
    num = (rx ** 2) * (ry ** 2) - (rx ** 2) * (y1p ** 2) - (ry ** 2) * (x1p ** 2)
    den = (rx ** 2) * (y1p ** 2) + (ry ** 2) * (x1p ** 2)
    co = sign * math.sqrt(max(0.0, num / den)) if den > 0 else 0.0

    cxp = co * (rx * y1p / ry)
    cyp = co * (-ry * x1p / rx)

    cx = cos_phi * cxp - sin_phi * cyp + (x1 + x2) / 2.0
    cy = sin_phi * cxp + cos_phi * cyp + (y1 + y2) / 2.0

    u = ((x1p - cxp) / rx, (y1p - cyp) / ry)
    v = ((-x1p - cxp) / rx, (-y1p - cyp) / ry)
    theta1 = _angle_between((1.0, 0.0), u)
    dtheta = _angle_between(u, v)

    if sweep_flag == 0 and dtheta > 0:
        dtheta -= 2 * math.pi
    elif sweep_flag == 1 and dtheta < 0:
        dtheta += 2 * math.pi

    for i in range(1, segments + 1):
        t = i / segments
        angle = theta1 + dtheta * t
        ex = rx * math.cos(angle)
        ey = ry * math.sin(angle)
        px = cos_phi * ex - sin_phi * ey + cx
        py = sin_phi * ex + cos_phi * ey + cy
        out.append((px, py))
    # Ensure the exact endpoint is hit despite float drift in the sampled arc.
    out[-1] = end


def parse_path(d: str, curve_segments: int = 16) -> List[Subpath]:
    scanner = _Scanner(d)
    subpaths: List[Subpath] = []
    cur: List[Point] = []
    cur_x = cur_y = 0.0
    start_x = start_y = 0.0
    last_cmd: Optional[str] = None
    last_cubic_ctrl: Optional[Point] = None
    last_quad_ctrl: Optional[Point] = None

    def start_new_subpath(x: float, y: float) -> None:
        nonlocal cur, cur_x, cur_y, start_x, start_y
        if cur:
            subpaths.append(Subpath(cur, False))
        cur = [(x, y)]
        cur_x, cur_y = x, y
        start_x, start_y = x, y

    def close_current() -> None:
        nonlocal cur
        if cur:
            subpaths.append(Subpath(cur, True))
            cur = []

    while not scanner.at_end():
        if scanner.peek().isalpha():
            cmd = scanner.read_command()
        else:
            if last_cmd is None:
                raise PathSyntaxError("path data must start with a command letter")
            cmd = "L" if last_cmd == "M" else "l" if last_cmd == "m" else last_cmd

        if cmd in ("M", "m"):
            nx, ny = scanner.read_number(), scanner.read_number()
            if cmd == "m":
                nx += cur_x
                ny += cur_y
            start_new_subpath(nx, ny)
        elif cmd in ("L", "l"):
            nx, ny = scanner.read_number(), scanner.read_number()
            if cmd == "l":
                nx += cur_x
                ny += cur_y
            cur.append((nx, ny))
            cur_x, cur_y = nx, ny
        elif cmd in ("H", "h"):
            nx = scanner.read_number()
            if cmd == "h":
                nx += cur_x
            cur.append((nx, cur_y))
            cur_x = nx
        elif cmd in ("V", "v"):
            ny = scanner.read_number()
            if cmd == "v":
                ny += cur_y
            cur.append((cur_x, ny))
            cur_y = ny
        elif cmd in ("C", "c"):
            x1, y1 = scanner.read_number(), scanner.read_number()
            x2, y2 = scanner.read_number(), scanner.read_number()
            x, y = scanner.read_number(), scanner.read_number()
            if cmd == "c":
                x1 += cur_x; y1 += cur_y
                x2 += cur_x; y2 += cur_y
                x += cur_x; y += cur_y
            _flatten_cubic(cur, (cur_x, cur_y), (x1, y1), (x2, y2), (x, y), curve_segments)
            last_cubic_ctrl = (x2, y2)
            cur_x, cur_y = x, y
        elif cmd in ("S", "s"):
            x2, y2 = scanner.read_number(), scanner.read_number()
            x, y = scanner.read_number(), scanner.read_number()
            if cmd == "s":
                x2 += cur_x; y2 += cur_y
                x += cur_x; y += cur_y
            if last_cmd in ("C", "c", "S", "s") and last_cubic_ctrl is not None:
                x1, y1 = 2 * cur_x - last_cubic_ctrl[0], 2 * cur_y - last_cubic_ctrl[1]
            else:
                x1, y1 = cur_x, cur_y
            _flatten_cubic(cur, (cur_x, cur_y), (x1, y1), (x2, y2), (x, y), curve_segments)
            last_cubic_ctrl = (x2, y2)
            cur_x, cur_y = x, y
        elif cmd in ("Q", "q"):
            x1, y1 = scanner.read_number(), scanner.read_number()
            x, y = scanner.read_number(), scanner.read_number()
            if cmd == "q":
                x1 += cur_x; y1 += cur_y
                x += cur_x; y += cur_y
            _flatten_quadratic(cur, (cur_x, cur_y), (x1, y1), (x, y), curve_segments)
            last_quad_ctrl = (x1, y1)
            cur_x, cur_y = x, y
        elif cmd in ("T", "t"):
            x, y = scanner.read_number(), scanner.read_number()
            if cmd == "t":
                x += cur_x; y += cur_y
            if last_cmd in ("Q", "q", "T", "t") and last_quad_ctrl is not None:
                x1, y1 = 2 * cur_x - last_quad_ctrl[0], 2 * cur_y - last_quad_ctrl[1]
            else:
                x1, y1 = cur_x, cur_y
            _flatten_quadratic(cur, (cur_x, cur_y), (x1, y1), (x, y), curve_segments)
            last_quad_ctrl = (x1, y1)
            cur_x, cur_y = x, y
        elif cmd in ("A", "a"):
            rx, ry = scanner.read_number(), scanner.read_number()
            rot = scanner.read_number()
            large, sweep = scanner.read_flag(), scanner.read_flag()
            x, y = scanner.read_number(), scanner.read_number()
            if cmd == "a":
                x += cur_x; y += cur_y
            _flatten_arc(cur, (cur_x, cur_y), rx, ry, rot, large, sweep, (x, y), curve_segments)
            cur_x, cur_y = x, y
        elif cmd in ("Z", "z"):
            close_current()
            cur_x, cur_y = start_x, start_y
        else:
            raise PathSyntaxError(f"unsupported path command '{cmd}'")

        last_cmd = cmd

    if cur:
        subpaths.append(Subpath(cur, False))

    return subpaths
