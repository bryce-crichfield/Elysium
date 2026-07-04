"""Walks an SvgNode tree into Elysium prefab XML text, writing any collected
images alongside it as PNGs under Assets/Textures/<PrefabName>/.

Output is hand-indented (4 spaces, one component per line) to match the
existing hand-authored prefabs (see Assets/Scenes/Prefabs/*.xml) rather than
using a generic XML serializer. All entities are flat siblings under a single
<Entities> root — hierarchy is expressed purely through ParentComponent,
matching the confirmed prefab schema (no XML nesting for parent/child).
"""
from __future__ import annotations

import hashlib
import io
import math
import os
from typing import Dict, List, Optional, Tuple

from svg_model import SvgNode


def _fmt(v: float) -> str:
    if abs(v - round(v)) < 1e-6:
        return str(int(round(v)))
    s = f"{v:.4f}".rstrip("0").rstrip(".")
    return s if s not in ("", "-") else "0"


class PrefabWriter:
    def __init__(self, prefab_name: str, assets_root: str, layer_name: str = "default"):
        self.prefab_name = prefab_name
        self.assets_root = assets_root
        self.layer_name = layer_name
        self.warnings: List[str] = []
        self.written_images: List[str] = []
        self._image_counter = 0
        self._lines: List[str] = []
        # Keyed by content hash so the same source image data (e.g. one <image>
        # reached through several <use> instances) is written once and every
        # TextureComponent just references the same texture path by name.
        self._image_cache: Dict[str, Tuple[str, float, float]] = {}

    def write(self, root: SvgNode) -> str:
        self._lines = ["<Entities>"]
        self._emit_entity(root, parent_name=None)
        self._lines.append("</Entities>")
        return "\n".join(self._lines) + "\n"

    def _emit_entity(self, node: SvgNode, parent_name: Optional[str]) -> None:
        pad = "    "
        inner = "        "
        g = node.geometry

        self._lines.append(f"{pad}<Entity>")
        self._lines.append(f'{inner}<LayerComponent name="{self.layer_name}" />')
        self._lines.append(f'{inner}<NameComponent name="{node.name}" />')

        x, y = g.get("x", 0.0), g.get("y", 0.0)
        image_placement = None
        if node.kind == "image":
            image_placement = self._place_image(g)

        if node.kind == "group" or node.kind == "image":
            if image_placement is not None:
                _, scale_x, scale_y, _, _ = image_placement
            else:
                scale_x, scale_y = g.get("scaleX", 1.0), g.get("scaleY", 1.0)
            rotation = g.get("rotation", 0.0)
            attrs = f'x="{_fmt(x)}" y="{_fmt(y)}"'
            if abs(scale_x - 1.0) > 1e-4:
                attrs += f' scaleX="{_fmt(scale_x)}"'
            if abs(scale_y - 1.0) > 1e-4:
                attrs += f' scaleY="{_fmt(scale_y)}"'
            if abs(rotation) > 1e-4:
                attrs += f' rotation="{_fmt(rotation)}"'
            self._lines.append(f"{inner}<TransformComponent {attrs} />")
        else:
            self._lines.append(f'{inner}<TransformComponent x="{_fmt(x)}" y="{_fmt(y)}" />')

        if parent_name is not None:
            self._lines.append(f'{inner}<ParentComponent target="{parent_name}" />')

        if node.kind == "rect":
            self._lines.append(
                f'{inner}<RectangleComponent width="{_fmt(g["width"])}" height="{_fmt(g["height"])}" '
                f'background="{node.style.fill_hex()}" border="{node.style.stroke_hex()}" '
                f'strokeWidth="{_fmt(g["strokeWidth"])}" cornerRadius="{_fmt(g["cornerRadius"])}" />'
            )
        elif node.kind == "circle":
            self._lines.append(
                f'{inner}<CircleComponent radius="{_fmt(g["radius"])}" '
                f'fill="{node.style.fill_hex()}" border="{node.style.stroke_hex()}" />'
            )
        elif node.kind == "ellipse":
            self._lines.append(
                f'{inner}<EllipseComponent radiusH="{_fmt(g["radiusH"])}" radiusV="{_fmt(g["radiusV"])}" '
                f'fill="{node.style.fill_hex()}" border="{node.style.stroke_hex()}" />'
            )
        elif node.kind == "line":
            self._lines.append(
                f'{inner}<LineComponent x1="{_fmt(g["x1"])}" y1="{_fmt(g["y1"])}" '
                f'x2="{_fmt(g["x2"])}" y2="{_fmt(g["y2"])}" '
                f'color="{node.style.stroke_hex()}" thickness="{_fmt(g["thickness"])}" />'
            )
        elif node.kind == "polygon":
            points_str = " ".join(f"{_fmt(px)},{_fmt(py)}" for px, py in g["points"])
            self._lines.append(
                f'{inner}<PolygonComponent points="{points_str}" '
                f'fill="{node.style.fill_hex()}" border="{node.style.stroke_hex()}" />'
            )
        elif node.kind == "image":
            rel_path, _, _, src_w, src_h = image_placement
            self._lines.append(
                f'{inner}<TextureComponent texture="{rel_path}" '
                f'sourceX="0" sourceY="0" sourceWidth="{_fmt(src_w)}" sourceHeight="{_fmt(src_h)}" />'
            )

        self._lines.append(f"{pad}</Entity>")

        for child in node.children:
            self._emit_entity(child, parent_name=node.name)

    def _place_image(self, g: dict) -> Tuple[str, float, float, float, float]:
        """Writes (or reuses) the PNG for an <image> node and returns
        (rel_path, scaleX, scaleY, sourceWidth, sourceHeight) for TransformComponent/
        TextureComponent. scaleX/Y fold this element's own transform-decomposed
        scale together with the ratio of its SVG-requested display size to the
        texture's original (pre-shear) pixel size, since sourceRect doubles as
        both the source crop rect and (via TransformComponent.worldScaleX/Y,
        which the engine DOES apply to sprites) the base size before scaling.
        """
        skew_x = g.get("skewX", 0.0)
        rel_path, orig_w, orig_h, out_w = self._write_image(g["image_bytes"], g["image_ext"], skew_x)
        own_scale_x, own_scale_y = g.get("scaleX", 1.0), g.get("scaleY", 1.0)
        display_w, display_h = g["displayWidth"], g["displayHeight"]
        # Scale ratio uses the ORIGINAL (pre-shear) pixel size: the padding pixels
        # added by shearing are extra sheared-overflow content, not a resize, so
        # they must be scaled by the same per-pixel ratio as the rest of the image.
        scale_x = own_scale_x * (display_w / orig_w) if orig_w > 0 else own_scale_x
        scale_y = own_scale_y * (display_h / orig_h) if orig_h > 0 else own_scale_y
        return rel_path, scale_x, scale_y, out_w, orig_h

    def _write_image(self, image_bytes: bytes, image_ext: str, skew_x: float = 0.0) -> Tuple[str, float, float, float]:
        """Writes image_bytes as a PNG (deduped by content hash + skew_x, since the
        same source image sheared by different amounts needs distinct output
        files) and returns (rel_path, original_width, original_height, output_width).

        When skew_x is nonzero, this element's own transform has real shear that
        TransformComponent can't represent at runtime (no skew field, and
        DrawTexturePro can't shear a quad) - unlike scale/rotation, which the
        engine's real hierarchy applies for us, shear has to be pre-baked directly
        into the pixels here instead. The shear is applied about the image's own
        vertical center (matching bake_point's center-relative convention for
        other shape kinds), so the padded output's center lands exactly on the
        original center - no position correction is needed by callers.
        """
        cache_key = hashlib.sha1(image_bytes).hexdigest()
        if abs(skew_x) > 1e-6:
            cache_key += f"_skew{skew_x:.6f}"
        cached = self._image_cache.get(cache_key)
        if cached is not None:
            return cached

        self._image_counter += 1
        base_name = f"image_{self._image_counter}"
        out_dir = os.path.join(self.assets_root, "Textures", self.prefab_name)
        os.makedirs(out_dir, exist_ok=True)

        dest_name = f"{base_name}.png"
        dest_path = os.path.join(out_dir, dest_name)
        try:
            from PIL import Image
            img = Image.open(io.BytesIO(image_bytes)).convert("RGBA")
            orig_w, orig_h = float(img.width), float(img.height)
            out_w = orig_w

            if abs(skew_x) > 1e-6:
                pad = abs(skew_x) * orig_h
                new_w = int(math.ceil(orig_w + pad))
                x_offset = (new_w - orig_w) / 2.0
                c = x_offset - skew_x * orig_h / 2.0
                # PIL AFFINE data maps dest -> source: src_x = a*dst_x + b*dst_y + c
                data = (1.0, -skew_x, -c, 0.0, 1.0, 0.0)
                img = img.transform((new_w, int(orig_h)), Image.AFFINE, data, resample=Image.BICUBIC)
                out_w = float(new_w)

            img.save(dest_path, format="PNG")
        except Exception as e:  # noqa: BLE001 - fall back to raw bytes for any decode/encode failure
            self.warnings.append(f"{base_name}: could not re-encode via Pillow ({e}); wrote raw bytes instead")
            dest_name = f"{base_name}.{image_ext}"
            dest_path = os.path.join(out_dir, dest_name)
            with open(dest_path, "wb") as f:
                f.write(image_bytes)
            orig_w, orig_h, out_w = 0.0, 0.0, 0.0

        self.written_images.append(dest_path)
        result = (f"Textures/{self.prefab_name}/{dest_name}", orig_w, orig_h, out_w)
        self._image_cache[cache_key] = result
        return result


def write_prefab_file(
    root: SvgNode, prefab_name: str, output_xml_path: str, assets_root: str, layer_name: str = "default",
) -> Tuple[List[str], List[str]]:
    """Writes the prefab XML and any images to disk. Returns (warnings, written_image_paths)."""
    writer = PrefabWriter(prefab_name, assets_root, layer_name)
    xml_text = writer.write(root)
    out_dir = os.path.dirname(os.path.abspath(output_xml_path))
    os.makedirs(out_dir, exist_ok=True)
    with open(output_xml_path, "w", encoding="utf-8") as f:
        f.write(xml_text)
    return writer.warnings, writer.written_images
