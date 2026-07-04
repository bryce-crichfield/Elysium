"""Data model shared between the SVG parser and the prefab writer.

Style handling approximates SVG's cascade: `fill`/`stroke`/`fill-opacity`/
`stroke-opacity`/`stroke-width` are inherited down `<g>` ancestry the way CSS
would resolve them. `opacity` is not itself inheritable in SVG (it composites
the whole subtree as a layer) — since the target engine has no per-entity
group compositing, group `opacity` is approximated by multiplying it into
descendants' final alpha as it cascades down, which is the same approximation
most simple SVG-to-native-shape converters make.
"""
from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

from xform import IDENTITY, Matrix

RGB = Tuple[int, int, int]

NAMED_COLORS: Dict[str, RGB] = {
    "black": (0, 0, 0), "white": (255, 255, 255), "red": (255, 0, 0),
    "green": (0, 128, 0), "blue": (0, 0, 255), "yellow": (255, 255, 0),
    "orange": (255, 165, 0), "purple": (128, 0, 128), "pink": (255, 192, 203),
    "gray": (128, 128, 128), "grey": (128, 128, 128), "cyan": (0, 255, 255),
    "magenta": (255, 0, 255), "brown": (165, 42, 42), "lime": (0, 255, 0),
    "navy": (0, 0, 128), "teal": (0, 128, 128), "olive": (128, 128, 0),
    "maroon": (128, 0, 0), "silver": (192, 192, 192), "gold": (255, 215, 0),
    "indigo": (75, 0, 130), "violet": (238, 130, 238),
}

_HEX3_RE = re.compile(r"^#([0-9a-fA-F])([0-9a-fA-F])([0-9a-fA-F])$")
_HEX6_RE = re.compile(r"^#([0-9a-fA-F]{2})([0-9a-fA-F]{2})([0-9a-fA-F]{2})$")
_RGB_RE = re.compile(r"^rgb\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)$")


def parse_color(value: Optional[str]) -> Optional[RGB]:
    """Returns an (r, g, b) triple, or None for 'none'/unrecognized/unset."""
    if not value:
        return None
    v = value.strip().lower()
    if v in ("none", "transparent"):
        return None
    m = _HEX6_RE.match(v)
    if m:
        return tuple(int(g, 16) for g in m.groups())  # type: ignore[return-value]
    m = _HEX3_RE.match(v)
    if m:
        return tuple(int(g * 2, 16) for g in m.groups())  # type: ignore[return-value]
    m = _RGB_RE.match(v)
    if m:
        return tuple(int(g) for g in m.groups())  # type: ignore[return-value]
    if v in NAMED_COLORS:
        return NAMED_COLORS[v]
    return None


def to_hex8(rgb: Optional[RGB], alpha: float) -> str:
    """Formats an (r,g,b) + [0,1] alpha into the engine's #RRGGBBAA hex format."""
    if rgb is None:
        return "#00000000"
    r, g, b = rgb
    a = max(0, min(255, round(alpha * 255)))
    return f"#{r:02X}{g:02X}{b:02X}{a:02X}"


@dataclass
class StyleAttrs:
    """Raw style values parsed off one element. None means 'not specified here'."""
    fill: Optional[str] = None
    fill_opacity: Optional[float] = None
    stroke: Optional[str] = None
    stroke_opacity: Optional[float] = None
    stroke_width: Optional[float] = None
    opacity: Optional[float] = None


@dataclass
class ResolvedStyle:
    fill: Optional[str] = "black"
    fill_opacity: float = 1.0
    stroke: Optional[str] = None
    stroke_opacity: float = 1.0
    stroke_width: float = 1.0
    group_opacity: float = 1.0

    def resolve(self, attrs: StyleAttrs) -> "ResolvedStyle":
        return ResolvedStyle(
            fill=attrs.fill if attrs.fill is not None else self.fill,
            fill_opacity=attrs.fill_opacity if attrs.fill_opacity is not None else self.fill_opacity,
            stroke=attrs.stroke if attrs.stroke is not None else self.stroke,
            stroke_opacity=attrs.stroke_opacity if attrs.stroke_opacity is not None else self.stroke_opacity,
            stroke_width=attrs.stroke_width if attrs.stroke_width is not None else self.stroke_width,
            group_opacity=self.group_opacity * (attrs.opacity if attrs.opacity is not None else 1.0),
        )

    def fill_hex(self) -> str:
        return to_hex8(parse_color(self.fill), self.fill_opacity * self.group_opacity)

    def stroke_hex(self) -> str:
        return to_hex8(parse_color(self.stroke), self.stroke_opacity * self.group_opacity)


ROOT_STYLE = ResolvedStyle()


@dataclass
class SvgNode:
    """A node in the parsed SVG tree, ready for the prefab writer.

    `kind` is one of: "group", "rect", "circle", "ellipse", "line", "polygon",
    "polyline", "path_closed", "path_open", "image". `geometry` holds
    kind-specific payload (documented at each producer in svg_parser.py).
    `local_matrix` is this node's own transform only (not flattened with
    ancestors) — nested groups compose at runtime via ParentComponent/
    TransformSystem, matching how the engine's hierarchy already works.
    """
    kind: str
    name: str
    local_matrix: Matrix
    style: ResolvedStyle
    geometry: dict = field(default_factory=dict)
    children: List["SvgNode"] = field(default_factory=list)


class NameAllocator:
    """Sanitizes SVG ids into valid, unique NameComponent values."""

    _INVALID_RE = re.compile(r"[^A-Za-z0-9_]")

    def __init__(self) -> None:
        self._used: set[str] = set()

    def allocate(self, preferred: Optional[str], fallback_prefix: str) -> str:
        base = self._INVALID_RE.sub("_", preferred) if preferred else ""
        if not base or not base[0].isalpha() and base[0] != "_":
            base = f"{fallback_prefix}_{base}" if base else fallback_prefix
        candidate = base
        n = 2
        while candidate in self._used:
            candidate = f"{base}_{n}"
            n += 1
        self._used.add(candidate)
        return candidate
