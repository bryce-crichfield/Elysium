"""Walks a parsed SVG document into a tree of SvgNode, ready for prefab_writer.

Key correction baked in here: Elysium's shape renderers (Rectangle/Ellipse/
Line/Polygon) only ever read an entity's world *position* — they never apply
worldRotation/worldScaleX/Y to their own draw calls (confirmed by reading
RenderSystem.cpp). ParentComponent position hierarchy composes correctly at
runtime (same TRS math used below), but a shape's own rotation/scale would be
silently ignored if left for the engine to apply. So every leaf shape's
geometry is pre-distorted here using the *accumulated* rotation/scale (all
ancestor groups' own local rotation/scale plus this element's own, composed
with the exact same additive-rotation/multiplicative-scale rule
TransformSystem::ComposeRecursive uses) via xform.bake_point. Position still
flows through TransformComponent/ParentComponent normally.

Accumulation is tracked as a real composed Matrix (`accum_matrix`), not as
separately-threaded rotation/scale/skew scalars. Decomposing a *single*
matrix (via xform.decompose) is always exact, but decomposing two matrices
separately and then adding their rotations / multiplying their scales /
adding their skews is only exact when the two levels commute — which fails
as soon as two nested levels each contribute rotation together with
non-uniform scale (a common Inkscape pattern: draw a shape once, then reuse
it mirrored/rotated via nested <use>). Threading the real matrix and
decomposing once, at the point a total is actually needed, sidesteps that
error entirely.
"""
from __future__ import annotations

import base64
import math
import os
import re
import xml.etree.ElementTree as ET
from typing import Dict, List, Optional, Tuple

import xform
from path_data import Subpath, parse_path
from svg_model import NameAllocator, ROOT_STYLE, ResolvedStyle, StyleAttrs, SvgNode

Point = Tuple[float, float]

_POINTS_NUM_RE = re.compile(r"[-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?")
_UNSUPPORTED_CONTAINER_TAGS = {"defs", "title", "desc", "metadata", "style", "symbol"}


def _local_name(tag: str) -> str:
    return tag.split("}")[-1]


def _get_attr(elem: ET.Element, name: str, default: Optional[str] = None) -> Optional[str]:
    for k, v in elem.attrib.items():
        if _local_name(k) == name:
            return v
    return default


def _num(v: Optional[str], default: float = 0.0) -> float:
    if v is None:
        return default
    try:
        return float(v)
    except ValueError:
        return default


def _angle_near(a: float, b: float, eps: float = 1e-3) -> bool:
    diff = (a - b + 180.0) % 360.0 - 180.0
    return abs(diff) < eps


def _close(a: float, b: float, eps: float = 1e-4) -> bool:
    return abs(a - b) < eps * max(1.0, abs(a), abs(b))


def _parse_points_attr(s: str) -> List[Point]:
    nums = [float(n) for n in _POINTS_NUM_RE.findall(s)]
    return list(zip(nums[0::2], nums[1::2]))


def _parse_style_attrs(elem: ET.Element) -> StyleAttrs:
    props: Dict[str, str] = {}
    style_attr = _get_attr(elem, "style")
    if style_attr:
        for chunk in style_attr.split(";"):
            if ":" in chunk:
                k, v = chunk.split(":", 1)
                props[k.strip().lower()] = v.strip()
    for key in ("fill", "fill-opacity", "stroke", "stroke-opacity", "stroke-width", "opacity"):
        if key not in props:
            v = _get_attr(elem, key)
            if v is not None:
                props[key] = v

    def fnum(key: str) -> Optional[float]:
        v = props.get(key)
        if v is None:
            return None
        v = v.strip()
        try:
            if v.endswith("%"):
                return float(v[:-1]) / 100.0
            return float(v)
        except ValueError:
            return None

    return StyleAttrs(
        fill=props.get("fill"),
        fill_opacity=fnum("fill-opacity"),
        stroke=props.get("stroke"),
        stroke_opacity=fnum("stroke-opacity"),
        stroke_width=fnum("stroke-width"),
        opacity=fnum("opacity"),
    )


class SvgParser:
    def __init__(self, curve_segments: int = 16):
        self.curve_segments = curve_segments
        self.names = NameAllocator()
        self.warnings: List[str] = []
        self.id_index: Dict[str, ET.Element] = {}
        self.base_dir = "."
        self._use_stack: set = set()

    # ---- entry point ---------------------------------------------------

    def parse_file(self, path: str) -> SvgNode:
        tree = ET.parse(path)
        root_elem = tree.getroot()
        self.base_dir = os.path.dirname(os.path.abspath(path))

        for el in root_elem.iter():
            eid = _get_attr(el, "id")
            if eid:
                self.id_index[eid] = el

        minx, miny = 0.0, 0.0
        vb = _get_attr(root_elem, "viewBox")
        if vb:
            parts = [float(n) for n in _POINTS_NUM_RE.findall(vb)]
            if len(parts) == 4:
                minx, miny = parts[0], parts[1]

        root_name = self.names.allocate("Root", "Root")
        root_matrix = xform.translate(-minx, -miny)
        root_decomp = xform.decompose(root_matrix)
        root_node = SvgNode(kind="group", name=root_name, local_matrix=root_matrix, style=ROOT_STYLE)
        root_node.geometry = {
            "x": root_decomp.x, "y": root_decomp.y,
            "scaleX": 1.0, "scaleY": 1.0, "rotation": 0.0,
        }

        for child in root_elem:
            tag = _local_name(child.tag)
            if tag in _UNSUPPORTED_CONTAINER_TAGS:
                continue
            node = self._walk(child, ROOT_STYLE, xform.IDENTITY)
            if node is not None:
                root_node.children.append(node)

        return root_node

    # ---- dispatch -------------------------------------------------------

    def _walk(self, elem: ET.Element, parent_style: ResolvedStyle, accum_matrix: xform.Matrix) -> Optional[SvgNode]:
        tag = _local_name(elem.tag)
        if tag == "g":
            return self._walk_group(elem, parent_style, accum_matrix)
        if tag == "rect":
            return self._walk_rect(elem, parent_style, accum_matrix)
        if tag == "circle":
            return self._walk_circle(elem, parent_style, accum_matrix)
        if tag == "ellipse":
            return self._walk_ellipse(elem, parent_style, accum_matrix)
        if tag == "line":
            return self._walk_line(elem, parent_style, accum_matrix)
        if tag in ("polygon", "polyline"):
            return self._walk_polyshape(elem, tag, parent_style, accum_matrix)
        if tag == "path":
            return self._walk_path(elem, parent_style, accum_matrix)
        if tag == "use":
            return self._walk_use(elem, parent_style, accum_matrix)
        if tag == "image":
            return self._walk_image(elem, parent_style, accum_matrix)

        eid = _get_attr(elem, "id")
        label = f"<{tag}>" + (f" id={eid}" if eid else "")
        self.warnings.append(f"Unsupported element {label} skipped")
        return None

    # ---- own-transform helper -------------------------------------------

    def _own_transform(self, elem: ET.Element, name: str) -> Tuple[xform.Matrix, "xform.Decomposed"]:
        t_own = xform.parse_transform(_get_attr(elem, "transform"))
        decomp = xform.decompose(t_own)
        return t_own, decomp

    def _total_decomp(self, accum_matrix: xform.Matrix, t_own: xform.Matrix) -> "xform.Decomposed":
        """Decomposes the true, exact composition of every ancestor's own
        transform with this element's own transform - always exact, unlike
        combining each level's separately-decomposed rotation/scale/skew.
        """
        return xform.decompose(xform.multiply(accum_matrix, t_own))

    # ---- group / use ------------------------------------------------------

    def _walk_group(self, elem: ET.Element, parent_style: ResolvedStyle, accum_matrix: xform.Matrix) -> Optional[SvgNode]:
        name = self.names.allocate(_get_attr(elem, "id"), "Group")
        t_own, decomp = self._own_transform(elem, name)
        style = parent_style.resolve(_parse_style_attrs(elem))

        # An ancestor's shear can't be applied by the real hierarchy at runtime
        # (TransformComponent has no skew field), so this group's own offset from
        # its immediate parent must be pre-sheared by whatever accumulated skew
        # is reaching it here.
        accum_decomp = xform.decompose(accum_matrix)
        pos_x, pos_y = xform.shear_offset(decomp.x, decomp.y, accum_decomp.skew_x)

        node = SvgNode(kind="group", name=name, local_matrix=t_own, style=style)
        node.geometry = {
            "x": pos_x, "y": pos_y,
            "scaleX": decomp.scale_x, "scaleY": decomp.scale_y, "rotation": decomp.rotation_deg,
        }

        child_accum_matrix = xform.multiply(accum_matrix, t_own)

        for child in elem:
            tag = _local_name(child.tag)
            if tag in _UNSUPPORTED_CONTAINER_TAGS:
                continue
            child_node = self._walk(child, style, child_accum_matrix)
            if child_node is not None:
                node.children.append(child_node)

        return node

    def _walk_use(self, elem: ET.Element, parent_style: ResolvedStyle, accum_matrix: xform.Matrix) -> Optional[SvgNode]:
        href = _get_attr(elem, "href")
        name = self.names.allocate(_get_attr(elem, "id"), "Use")
        if not href or not href.startswith("#"):
            self.warnings.append(f"<use> '{name}': external/missing href unsupported, skipped")
            return None

        target_id = href[1:]
        target_elem = self.id_index.get(target_id)
        if target_elem is None:
            self.warnings.append(f"<use> '{name}': referenced id '#{target_id}' not found, skipped")
            return None
        if target_id in self._use_stack:
            self.warnings.append(f"<use> '{name}': circular reference to '#{target_id}', skipped")
            return None

        ux, uy = _num(_get_attr(elem, "x"), 0.0), _num(_get_attr(elem, "y"), 0.0)
        use_transform = xform.parse_transform(_get_attr(elem, "transform"))
        t_own = xform.multiply(use_transform, xform.translate(ux, uy))

        # Flatten straight-through <use>-of-<use> chains (a common Inkscape
        # "draw the shape once, reuse it mirrored/rotated" pattern) into a
        # single combined matrix instead of materializing one wrapper entity
        # per <use> in the chain. This matters for correctness, not just entity
        # count: two nested wrapper entities, each with its own exact per-level
        # decomposition, still get recomposed at runtime (and in this tool's
        # preview) via additive rotation / multiplicative scale, which is only
        # exact for a single level. Folding the whole chain into one matrix and
        # decomposing once (below) sidesteps that entirely.
        pushed = [target_id]
        self._use_stack.add(target_id)
        while _local_name(target_elem.tag) == "use":
            inner_href = _get_attr(target_elem, "href")
            if not inner_href or not inner_href.startswith("#"):
                break
            inner_id = inner_href[1:]
            if inner_id in self._use_stack:
                self.warnings.append(f"<use> '{name}': circular reference to '#{inner_id}', skipped")
                for pid in pushed:
                    self._use_stack.discard(pid)
                return None
            inner_elem = self.id_index.get(inner_id)
            if inner_elem is None:
                self.warnings.append(f"<use> '{name}': referenced id '#{inner_id}' not found, skipped")
                for pid in pushed:
                    self._use_stack.discard(pid)
                return None

            inner_ux, inner_uy = _num(_get_attr(target_elem, "x"), 0.0), _num(_get_attr(target_elem, "y"), 0.0)
            inner_transform = xform.parse_transform(_get_attr(target_elem, "transform"))
            inner_t = xform.multiply(inner_transform, xform.translate(inner_ux, inner_uy))
            t_own = xform.multiply(t_own, inner_t)

            self._use_stack.add(inner_id)
            pushed.append(inner_id)
            target_id, target_elem = inner_id, inner_elem

        decomp = xform.decompose(t_own)
        style = parent_style.resolve(_parse_style_attrs(elem))

        accum_decomp = xform.decompose(accum_matrix)
        pos_x, pos_y = xform.shear_offset(decomp.x, decomp.y, accum_decomp.skew_x)

        wrapper = SvgNode(kind="group", name=name, local_matrix=t_own, style=style)
        wrapper.geometry = {
            "x": pos_x, "y": pos_y,
            "scaleX": decomp.scale_x, "scaleY": decomp.scale_y, "rotation": decomp.rotation_deg,
        }

        child_accum_matrix = xform.multiply(accum_matrix, t_own)
        inner = self._walk(target_elem, style, child_accum_matrix)
        for pid in pushed:
            self._use_stack.discard(pid)

        if inner is None:
            return None
        wrapper.children.append(inner)
        return wrapper

    # ---- basic shapes -----------------------------------------------------

    def _walk_rect(self, elem: ET.Element, parent_style: ResolvedStyle, accum_matrix: xform.Matrix) -> Optional[SvgNode]:
        name = self.names.allocate(_get_attr(elem, "id"), "Rect")
        x, y = _num(_get_attr(elem, "x")), _num(_get_attr(elem, "y"))
        w, h = _num(_get_attr(elem, "width")), _num(_get_attr(elem, "height"))
        if w <= 0 or h <= 0:
            self.warnings.append(f"Rect '{name}': non-positive width/height, skipped")
            return None

        rx_attr, ry_attr = _get_attr(elem, "rx"), _get_attr(elem, "ry")
        rx = _num(rx_attr, -1.0) if rx_attr is not None else -1.0
        ry = _num(ry_attr, -1.0) if ry_attr is not None else -1.0
        if rx < 0 and ry >= 0:
            rx = ry
        if ry < 0 and rx >= 0:
            ry = rx
        if rx < 0:
            rx, ry = 0.0, 0.0

        t_own, decomp = self._own_transform(elem, name)
        style = parent_style.resolve(_parse_style_attrs(elem))

        accum_decomp = xform.decompose(accum_matrix)
        cx_raw, cy_raw = x + w / 2.0, y + h / 2.0
        pos_x, pos_y = xform.apply(t_own, cx_raw, cy_raw)
        pos_x, pos_y = xform.shear_offset(pos_x, pos_y, accum_decomp.skew_x)

        total = self._total_decomp(accum_matrix, t_own)
        total_rotation = total.rotation_deg
        total_scale_x = total.scale_x
        total_scale_y = total.scale_y
        total_skew_x = total.skew_x
        has_shear = abs(total_skew_x) > 1e-6

        avg_scale = (abs(total_scale_x) + abs(total_scale_y)) / 2.0
        corner_ratio = min(1.0, rx / (min(w, h) / 2.0)) if min(w, h) > 0 and rx > 0 else 0.0

        if not has_shear and (_angle_near(total_rotation, 0.0) or _angle_near(total_rotation, 180.0)):
            final_w, final_h = abs(w * total_scale_x), abs(h * total_scale_y)
        elif not has_shear and (_angle_near(total_rotation, 90.0) or _angle_near(total_rotation, 270.0)):
            final_w, final_h = abs(w * total_scale_y), abs(h * total_scale_x)
        else:
            reason = "sheared" if has_shear else f"rotated {total_rotation:.1f} deg (not axis-aligned)"
            self.warnings.append(
                f"Rect '{name}': {reason} - degraded to Polygon; corner radius/stroke width not preserved"
            )
            hw, hh = w / 2.0, h / 2.0
            raw_corners = [(-hw, -hh), (hw, -hh), (hw, hh), (-hw, hh)]
            points = [xform.bake_point(dx, dy, total_rotation, total_scale_x, total_scale_y, total_skew_x) for dx, dy in raw_corners]
            node = SvgNode(kind="polygon", name=name, local_matrix=t_own, style=style)
            node.geometry = {"x": pos_x, "y": pos_y, "points": points}
            return node

        node = SvgNode(kind="rect", name=name, local_matrix=t_own, style=style)
        node.geometry = {
            "x": pos_x, "y": pos_y,
            "width": final_w, "height": final_h,
            "cornerRadius": corner_ratio,
            "strokeWidth": style.stroke_width * avg_scale,
        }
        return node

    def _emit_ellipse_or_polygon(
        self, name: str, pos_x: float, pos_y: float, rx: float, ry: float,
        total_rotation: float, total_scale_x: float, total_scale_y: float, total_skew_x: float,
        style: ResolvedStyle, t_own: xform.Matrix,
    ) -> SvgNode:
        rh, rv = rx * total_scale_x, ry * total_scale_y
        has_shear = abs(total_skew_x) > 1e-6

        if not has_shear and (_angle_near(total_rotation, 0.0) or _angle_near(total_rotation, 180.0)):
            final_rh, final_rv = rh, rv
        elif not has_shear and (_angle_near(total_rotation, 90.0) or _angle_near(total_rotation, 270.0)):
            final_rh, final_rv = rv, rh
        else:
            reason = "sheared" if has_shear else f"rotated {total_rotation:.1f} deg with non-uniform scale"
            self.warnings.append(f"'{name}': ellipse/circle {reason} - degraded to Polygon")
            segments = 24
            raw_points = [
                (rx * math.cos(2 * math.pi * i / segments), ry * math.sin(2 * math.pi * i / segments))
                for i in range(segments)
            ]
            points = [xform.bake_point(dx, dy, total_rotation, total_scale_x, total_scale_y, total_skew_x) for dx, dy in raw_points]
            node = SvgNode(kind="polygon", name=name, local_matrix=t_own, style=style)
            node.geometry = {"x": pos_x, "y": pos_y, "points": points}
            return node

        node = SvgNode(kind="ellipse", name=name, local_matrix=t_own, style=style)
        node.geometry = {"x": pos_x, "y": pos_y, "radiusH": abs(final_rh), "radiusV": abs(final_rv)}
        return node

    def _walk_circle(self, elem: ET.Element, parent_style: ResolvedStyle, accum_matrix: xform.Matrix) -> Optional[SvgNode]:
        name = self.names.allocate(_get_attr(elem, "id"), "Circle")
        cx, cy = _num(_get_attr(elem, "cx")), _num(_get_attr(elem, "cy"))
        r = _num(_get_attr(elem, "r"))
        if r <= 0:
            self.warnings.append(f"Circle '{name}': non-positive radius, skipped")
            return None

        t_own, decomp = self._own_transform(elem, name)
        style = parent_style.resolve(_parse_style_attrs(elem))
        accum_decomp = xform.decompose(accum_matrix)
        pos_x, pos_y = xform.apply(t_own, cx, cy)
        pos_x, pos_y = xform.shear_offset(pos_x, pos_y, accum_decomp.skew_x)

        total = self._total_decomp(accum_matrix, t_own)
        total_rotation, total_scale_x, total_scale_y, total_skew_x = (
            total.rotation_deg, total.scale_x, total.scale_y, total.skew_x
        )

        if abs(total_skew_x) <= 1e-6 and _close(total_scale_x, total_scale_y):
            node = SvgNode(kind="circle", name=name, local_matrix=t_own, style=style)
            node.geometry = {"x": pos_x, "y": pos_y, "radius": abs(r * total_scale_x)}
            return node

        return self._emit_ellipse_or_polygon(name, pos_x, pos_y, r, r, total_rotation, total_scale_x, total_scale_y, total_skew_x, style, t_own)

    def _walk_ellipse(self, elem: ET.Element, parent_style: ResolvedStyle, accum_matrix: xform.Matrix) -> Optional[SvgNode]:
        name = self.names.allocate(_get_attr(elem, "id"), "Ellipse")
        cx, cy = _num(_get_attr(elem, "cx")), _num(_get_attr(elem, "cy"))
        rx, ry = _num(_get_attr(elem, "rx")), _num(_get_attr(elem, "ry"))
        if rx <= 0 or ry <= 0:
            self.warnings.append(f"Ellipse '{name}': non-positive radius, skipped")
            return None

        t_own, decomp = self._own_transform(elem, name)
        style = parent_style.resolve(_parse_style_attrs(elem))
        accum_decomp = xform.decompose(accum_matrix)
        pos_x, pos_y = xform.apply(t_own, cx, cy)
        pos_x, pos_y = xform.shear_offset(pos_x, pos_y, accum_decomp.skew_x)

        total = self._total_decomp(accum_matrix, t_own)
        total_rotation, total_scale_x, total_scale_y, total_skew_x = (
            total.rotation_deg, total.scale_x, total.scale_y, total.skew_x
        )

        return self._emit_ellipse_or_polygon(name, pos_x, pos_y, rx, ry, total_rotation, total_scale_x, total_scale_y, total_skew_x, style, t_own)

    def _walk_line(self, elem: ET.Element, parent_style: ResolvedStyle, accum_matrix: xform.Matrix) -> Optional[SvgNode]:
        name = self.names.allocate(_get_attr(elem, "id"), "Line")
        x1, y1 = _num(_get_attr(elem, "x1")), _num(_get_attr(elem, "y1"))
        x2, y2 = _num(_get_attr(elem, "x2")), _num(_get_attr(elem, "y2"))

        t_own, decomp = self._own_transform(elem, name)
        style = parent_style.resolve(_parse_style_attrs(elem))

        accum_decomp = xform.decompose(accum_matrix)
        total = self._total_decomp(accum_matrix, t_own)

        return self._build_shape_from_points(
            name, [(x1, y1), (x2, y2)], False, t_own, style,
            total.rotation_deg, total.scale_x, total.scale_y, total.skew_x, accum_decomp.skew_x,
        )

    def _walk_polyshape(
        self, elem: ET.Element, tag: str, parent_style: ResolvedStyle, accum_matrix: xform.Matrix,
    ) -> Optional[SvgNode]:
        name = self.names.allocate(_get_attr(elem, "id"), "Polygon" if tag == "polygon" else "Polyline")
        raw_points = _parse_points_attr(_get_attr(elem, "points", "") or "")
        if len(raw_points) < 2:
            self.warnings.append(f"{tag} '{name}': fewer than 2 points, skipped")
            return None

        t_own, decomp = self._own_transform(elem, name)
        style = parent_style.resolve(_parse_style_attrs(elem))

        accum_decomp = xform.decompose(accum_matrix)
        total = self._total_decomp(accum_matrix, t_own)

        return self._build_shape_from_points(
            name, raw_points, tag == "polygon", t_own, style,
            total.rotation_deg, total.scale_x, total.scale_y, total.skew_x, accum_decomp.skew_x,
        )

    def _walk_path(self, elem: ET.Element, parent_style: ResolvedStyle, accum_matrix: xform.Matrix) -> Optional[SvgNode]:
        name = self.names.allocate(_get_attr(elem, "id"), "Path")
        d = _get_attr(elem, "d", "") or ""
        if not d:
            return None

        try:
            subpaths = parse_path(d, self.curve_segments)
        except Exception as e:  # noqa: BLE001 - malformed path data is a data problem, not a bug
            self.warnings.append(f"Path '{name}': failed to parse 'd' ({e}), skipped")
            return None
        subpaths = [sp for sp in subpaths if len(sp.points) >= 2]
        if not subpaths:
            return None

        t_own, decomp = self._own_transform(elem, name)
        style = parent_style.resolve(_parse_style_attrs(elem))

        accum_decomp = xform.decompose(accum_matrix)
        total = self._total_decomp(accum_matrix, t_own)
        total_rotation, total_scale_x, total_scale_y, total_skew_x = (
            total.rotation_deg, total.scale_x, total.scale_y, total.skew_x
        )

        if len(subpaths) == 1:
            sp = subpaths[0]
            return self._build_shape_from_points(
                name, sp.points, sp.closed, t_own, style,
                total_rotation, total_scale_x, total_scale_y, total_skew_x, accum_decomp.skew_x,
            )

        anchor = subpaths[0].points[0]
        pos_x, pos_y = xform.apply(t_own, anchor[0], anchor[1])
        pos_x, pos_y = xform.shear_offset(pos_x, pos_y, accum_decomp.skew_x)
        group = SvgNode(kind="group", name=name, local_matrix=xform.IDENTITY, style=style)
        group.geometry = {"x": pos_x, "y": pos_y, "scaleX": 1.0, "scaleY": 1.0, "rotation": 0.0}

        for i, sp in enumerate(subpaths):
            sub_name = self.names.allocate(f"{name}_sub{i}", f"{name}_sub")
            child = self._build_shape_from_points(
                sub_name, sp.points, sp.closed, t_own, style, total_rotation, total_scale_x, total_scale_y, total_skew_x,
                accum_skew_x=0.0,
                shared_local_origin=anchor,
            )
            if child is not None:
                group.children.append(child)

        return group if group.children else None

    def _build_shape_from_points(
        self, name: str, local_points: List[Point], closed: bool,
        t_own: xform.Matrix, style: ResolvedStyle,
        total_rotation: float, total_scale_x: float, total_scale_y: float, total_skew_x: float = 0.0,
        accum_skew_x: float = 0.0, shared_local_origin: Optional[Point] = None,
    ) -> Optional[SvgNode]:
        if len(local_points) < 2:
            return None

        if shared_local_origin is None:
            origin = local_points[0]
            pos_x, pos_y = xform.apply(t_own, origin[0], origin[1])
            pos_x, pos_y = xform.shear_offset(pos_x, pos_y, accum_skew_x)
        else:
            origin = shared_local_origin
            pos_x, pos_y = 0.0, 0.0

        baked = [
            xform.bake_point(p[0] - origin[0], p[1] - origin[1], total_rotation, total_scale_x, total_scale_y, total_skew_x)
            for p in local_points
        ]

        if closed:
            node = SvgNode(kind="polygon", name=name, local_matrix=t_own, style=style)
            node.geometry = {"x": pos_x, "y": pos_y, "points": baked}
            return node

        avg_scale = (abs(total_scale_x) + abs(total_scale_y)) / 2.0
        thickness = style.stroke_width * avg_scale

        if len(baked) == 2:
            node = SvgNode(kind="line", name=name, local_matrix=t_own, style=style)
            node.geometry = {
                "x": pos_x, "y": pos_y,
                "x1": baked[0][0], "y1": baked[0][1], "x2": baked[1][0], "y2": baked[1][1],
                "thickness": thickness,
            }
            return node

        group = SvgNode(kind="group", name=name, local_matrix=xform.IDENTITY, style=style)
        group.geometry = {"x": pos_x, "y": pos_y, "scaleX": 1.0, "scaleY": 1.0, "rotation": 0.0}
        for i in range(len(baked) - 1):
            seg_name = self.names.allocate(f"{name}_seg{i}", f"{name}_seg")
            seg = SvgNode(kind="line", name=seg_name, local_matrix=xform.IDENTITY, style=style)
            seg.geometry = {
                "x": 0.0, "y": 0.0,
                "x1": baked[i][0], "y1": baked[i][1], "x2": baked[i + 1][0], "y2": baked[i + 1][1],
                "thickness": thickness,
            }
            group.children.append(seg)
        return group

    # ---- images -------------------------------------------------------

    def _decode_image_href(self, href: str, name: str) -> Optional[Tuple[bytes, str]]:
        if href.startswith("data:"):
            try:
                header, b64data = href.split(",", 1)
                mime = header.split(";")[0].split(":")[1] if ":" in header else "image/png"
                ext = mime.split("/")[-1]
                return base64.b64decode(b64data), ext
            except Exception as e:  # noqa: BLE001
                self.warnings.append(f"Image '{name}': failed to decode data URI ({e}), skipped")
                return None

        candidate = os.path.join(self.base_dir, href)
        if not os.path.isfile(candidate):
            self.warnings.append(f"Image '{name}': external file '{href}' not found, skipped")
            return None
        with open(candidate, "rb") as f:
            raw = f.read()
        ext = os.path.splitext(href)[1].lstrip(".").lower() or "png"
        return raw, ext

    def _walk_image(self, elem: ET.Element, parent_style: ResolvedStyle, accum_matrix: xform.Matrix) -> Optional[SvgNode]:
        name = self.names.allocate(_get_attr(elem, "id"), "Image")
        href = _get_attr(elem, "href")
        if not href:
            self.warnings.append(f"Image '{name}': no href, skipped")
            return None

        x, y = _num(_get_attr(elem, "x")), _num(_get_attr(elem, "y"))
        w, h = _num(_get_attr(elem, "width")), _num(_get_attr(elem, "height"))
        if w <= 0 or h <= 0:
            self.warnings.append(f"Image '{name}': missing/non-positive width or height, skipped")
            return None

        decoded = self._decode_image_href(href, name)
        if decoded is None:
            return None
        image_bytes, image_ext = decoded

        t_own, decomp = self._own_transform(elem, name)
        style = parent_style.resolve(_parse_style_attrs(elem))
        accum_decomp = xform.decompose(accum_matrix)
        pos_x, pos_y = xform.apply(t_own, x + w / 2.0, y + h / 2.0)
        pos_x, pos_y = xform.shear_offset(pos_x, pos_y, accum_decomp.skew_x)

        # Unlike rotation/scale (own-local only, hierarchy handles ancestors), shear
        # has no TransformComponent/runtime equivalent at all - so the FULL
        # accumulated shear (ancestor <g>/<use> shear plus this element's own) must
        # be baked directly into the exported PNG's pixels, not just this element's
        # own transform. Computed from the true combined matrix (not
        # accum_decomp.skew_x + decomp.skew_x) since two decomposed skews don't add
        # linearly in general - only the composed matrix's own decomposition is exact.
        total_skew_x = self._total_decomp(accum_matrix, t_own).skew_x

        node = SvgNode(kind="image", name=name, local_matrix=t_own, style=style)
        node.geometry = {
            "x": pos_x, "y": pos_y,
            # Own-local-only display size/scale/rotation (NOT pre-multiplied/added with
            # ancestor scale/rotation): TextureComponent rendering, unlike Rectangle/
            # Ellipse/Line/Polygon, honors TransformComponent's worldScaleX/Y and
            # worldRotation via the engine's normal ParentComponent hierarchy
            # composition, so ancestor scale/rotation is applied automatically at
            # runtime. Baking it in here too would double it up. This is exact (not
            # an approximation) as long as any <use>-of-<use> reference chain above
            # this image was flattened to a single wrapper level in _walk_use, since
            # single-level composition is always exact.
            "displayWidth": w, "displayHeight": h,
            "scaleX": decomp.scale_x, "scaleY": decomp.scale_y, "rotation": decomp.rotation_deg,
            # Full accumulated shear (ancestor + own): unlike scale/rotation,
            # TransformComponent has no runtime skew equivalent at all,
            # so this can't be deferred to the hierarchy - prefab_writer pre-shears the
            # exported PNG's pixels via Pillow to bake it in directly.
            "skewX": total_skew_x,
            "image_bytes": image_bytes, "image_ext": image_ext,
        }
        return node
