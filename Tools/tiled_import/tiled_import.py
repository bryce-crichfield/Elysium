#!/usr/bin/env python3
"""
tiled_import.py — Convert Tiled .tmx maps to Elysium2 scene prefabs.

Usage:
    python tiled_import.py <map.tmx> [--assets <path>] [--scene]

Outputs (relative to --assets, default: Assets/):
    Tiles/<TilesetName>/<TilesetName>.xml     one per referenced tileset
    Prefabs/<MapName>_<LayerName>.xml         one per tile layer
    Scenes/<MapName>Scene.xml                 skeleton scene (--scene flag)

Empty cells (GID 0 in Tiled) are written as -1 in the Tilemask and skipped
by the engine loader (SceneLoader now guards on tileDefinitions.find).

Supported tile data encodings: csv, base64, base64+zlib, base64+gzip.
Supports both embedded and external (.tsx) tilesets.
"""

import argparse
import base64
import gzip
import re
import struct
import sys
import xml.etree.ElementTree as ET
import zlib
from pathlib import Path


# ---------------------------------------------------------------------------
# Tile data decoding
# ---------------------------------------------------------------------------

def decode_layer_data(data_elem):
    """Return a flat list of GIDs from a Tiled <data> element."""
    encoding    = data_elem.get("encoding", "xml")
    compression = data_elem.get("compression", "")
    raw         = (data_elem.text or "").strip()

    if encoding == "csv":
        return [int(x) for x in raw.split(",") if x.strip()]

    if encoding == "base64":
        decoded = base64.b64decode(raw)
        if compression == "zlib":
            decoded = zlib.decompress(decoded)
        elif compression == "gzip":
            decoded = gzip.decompress(decoded)
        elif compression == "zstd":
            raise ValueError("zstd compression not supported — re-export with zlib or csv")
        elif compression:
            raise ValueError(f"Unknown compression: {compression!r}")
        count = len(decoded) // 4
        return list(struct.unpack(f"<{count}I", decoded))

    raise ValueError(f"Unsupported encoding: {encoding!r} — use csv or base64 in Tiled export")


# ---------------------------------------------------------------------------
# Tileset helpers
# ---------------------------------------------------------------------------

def load_tileset(ts_elem, tmx_dir):
    """Return a tileset dict from an inline or external <tileset> element."""
    src = ts_elem.get("source")
    if src:
        tsx_path = tmx_dir / src
        ts_root  = ET.parse(tsx_path).getroot()
    else:
        ts_root = ts_elem

    cols       = int(ts_root.get("columns", 1))
    tile_count = int(ts_root.get("tilecount", cols))
    rows       = max(1, (tile_count + cols - 1) // cols)

    img_elem   = ts_root.find("image")
    img_src    = img_elem.get("source", "") if img_elem is not None else ""

    return {
        "firstgid":     int(ts_elem.get("firstgid")),
        "name":         ts_root.get("name", "Tileset"),
        "columns":      cols,
        "rows":         rows,
        "image_source": str(tmx_dir / img_src) if img_src else "",
    }


def gid_to_tileset(gid, tilesets):
    """Return (tileset, local_id) for a GID, or (None, -1) for empty (GID 0)."""
    if gid == 0:
        return None, -1
    for ts in reversed(tilesets):   # sorted by firstgid asc, so reversed = desc
        if gid >= ts["firstgid"]:
            return ts, gid - ts["firstgid"]
    return None, -1


# ---------------------------------------------------------------------------
# Output writers
# ---------------------------------------------------------------------------

def write_tile_xml(ts, assets_dir):
    name = ts["name"]
    cols = ts["columns"]
    rows = ts["rows"]

    out_dir = assets_dir / "Tiles" / name
    out_dir.mkdir(parents=True, exist_ok=True)

    lines = [
        f'<Tile name="{name}" originX="0.5" originY="1.0">',
        f'    <Sheet path="{name}/{name}.png" rows="{rows}" cols="{cols}"/>',
    ]
    for i in range(rows * cols):
        r, c = divmod(i, cols)
        lines.append(f'    <Variant name="tile_{i}" index="{r}:{c}"/>')
    lines.append("</Tile>")

    out_path = out_dir / f"{name}.xml"
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"  wrote  {out_path.relative_to(assets_dir.parent)}")

    expected_img = out_dir / f"{name}.png"
    if not expected_img.exists():
        src = ts["image_source"]
        print(f"  WARN   copy tileset image to {expected_img}")
        if src:
            print(f"         source: {src}")


def write_tilemap_xml(map_info, layer, tilesets, assets_dir, map_stem):
    width    = map_info["width"]
    height   = map_info["height"]
    tile_w   = map_info["tilewidth"]
    tile_h   = map_info["tileheight"]
    is_iso   = map_info["orientation"] == "isometric"
    gids     = layer["gids"]
    scene_layer = layer["scene_layer"]

    # Assign a compact sequential id to each unique (tileset, local_id) pair
    tile_defs = {}   # (ts_name, local_id) -> def_id
    next_id   = 0
    for gid in gids:
        ts, local_id = gid_to_tileset(gid, tilesets)
        if ts is None:
            continue
        key = (ts["name"], local_id)
        if key not in tile_defs:
            tile_defs[key] = next_id
            next_id += 1

    # Build flat tilemask; -1 = empty (no TileDefinition → loader skips)
    tilemask = []
    for gid in gids:
        ts, local_id = gid_to_tileset(gid, tilesets)
        if ts is None:
            tilemask.append(-1)
        else:
            tilemask.append(tile_defs[(ts["name"], local_id)])

    # Format as rows of width tokens
    rows_text = []
    for row in range(height):
        chunk = tilemask[row * width : (row + 1) * width]
        rows_text.append(" ".join(str(v) for v in chunk))
    tilemask_str = "\n        ".join(rows_text)

    lines = [
        f'<Tilemap width="{width}" height="{height}"',
        f'         isIsometric="{str(is_iso).lower()}"',
        f'         tileWidth="{tile_w}" tileHeight="{tile_h}">',
        f'    <Tilemask>',
        f'        {tilemask_str}',
        f'    </Tilemask>',
        f'    <TileDefinitions>',
    ]
    for (ts_name, local_id), def_id in sorted(tile_defs.items(), key=lambda x: x[1]):
        tile_path = f"Tiles/{ts_name}/{ts_name}.xml"
        variant   = f"tile_{local_id}"
        lines.append(
            f'        <TileDefinition id="{def_id}" tile="{tile_path}"'
            f' variant="{variant}" layer="{scene_layer}"/>'
        )
    lines += ["    </TileDefinitions>", "</Tilemap>"]

    safe_name = re.sub(r"\W+", "", layer["name"])
    out_path  = assets_dir / "Prefabs" / f"{map_stem}_{safe_name}.xml"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"  wrote  {out_path.relative_to(assets_dir.parent)}")
    return safe_name


def write_scene_xml(map_stem, layer_safe_names, map_info, assets_dir):
    is_iso   = map_info["orientation"] == "isometric"
    includes = "\n    ".join(
        f'<Include src="Prefabs/{map_stem}_{n}.xml" />'
        for n in layer_safe_names
    )
    scene = f"""\
<Scene type="{map_stem}Scene">
    <SceneScript path="Scripts/Scenes/{map_stem}Scene.lua" />

    <Systems>
        <System type="SpatialSystem" />
        <System type="MovementSystem" />
        <System type="TileSystem" />
        <System type="RenderSystem" />
        <System type="ScriptSystem" />
        <System type="UiSystem" />
        <System type="FollowSystem" />
        <System type="DebugSystem" />
    </Systems>

    <SceneConfiguration width="640" height="480">
        <SceneLayer name="tile"      z="0" space="World"  layerBlend="Normal" compositeBlend="Normal" isComposited="false" />
        <SceneLayer name="entity"    z="2" space="World"  layerBlend="Normal" compositeBlend="Normal" isComposited="false" />
        <SceneLayer name="selection" z="3" space="World"  layerBlend="Normal" compositeBlend="Normal" isComposited="false" />
        <SceneLayer name="ui"        z="4" space="Screen" layerBlend="Normal" compositeBlend="Normal" isComposited="false" />
    </SceneConfiguration>

    {includes}

    <Entities>
        <!-- TODO: camera, units, NPCs -->
    </Entities>
</Scene>
"""
    out_path = assets_dir / "Scenes" / f"{map_stem}Scene.xml"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(scene, encoding="utf-8")
    print(f"  wrote  {out_path.relative_to(assets_dir.parent)}")


# ---------------------------------------------------------------------------
# Layer → scene layer name heuristic
# ---------------------------------------------------------------------------

_ENTITY_LAYER_KEYWORDS = {"decor", "object", "detail", "top", "over", "deco", "prop"}

def infer_scene_layer(tiled_layer_name):
    lower = tiled_layer_name.lower()
    if any(k in lower for k in _ENTITY_LAYER_KEYWORDS):
        return "entity"
    return "tile"


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Convert a Tiled .tmx file to Elysium2 scene prefabs"
    )
    parser.add_argument("tmx",              help="Path to the .tmx file")
    parser.add_argument("--assets", "-a",   default="Assets",
                        help="Path to the Assets directory (default: Assets)")
    parser.add_argument("--scene", "-s",    action="store_true",
                        help="Also generate a skeleton Scene XML")
    args = parser.parse_args()

    tmx_path   = Path(args.tmx).resolve()
    assets_dir = Path(args.assets).resolve()
    map_stem   = tmx_path.stem

    if not tmx_path.exists():
        sys.exit(f"error: file not found: {tmx_path}")

    print(f"importing {tmx_path.name}")
    root = ET.parse(tmx_path).getroot()

    map_info = {
        "width":       int(root.get("width")),
        "height":      int(root.get("height")),
        "tilewidth":   int(root.get("tilewidth")),
        "tileheight":  int(root.get("tileheight")),
        "orientation": root.get("orientation", "orthogonal"),
    }
    print(f"  {map_info['width']}x{map_info['height']} tiles  "
          f"{map_info['orientation']}  "
          f"{map_info['tilewidth']}x{map_info['tileheight']}px")

    # Load tilesets
    tilesets = [load_tileset(e, tmx_path.parent) for e in root.findall("tileset")]
    tilesets.sort(key=lambda t: t["firstgid"])
    for ts in tilesets:
        print(f"  tileset  {ts['name']}  {ts['columns']}x{ts['rows']}  firstgid={ts['firstgid']}")

    # Write Tile XMLs (once per unique tileset name)
    seen = set()
    for ts in tilesets:
        if ts["name"] not in seen:
            write_tile_xml(ts, assets_dir)
            seen.add(ts["name"])

    # Process tile layers
    layer_safe_names = []
    for layer_elem in root.findall("layer"):
        name     = layer_elem.get("name", "Layer")
        data_elem = layer_elem.find("data")
        if data_elem is None:
            continue
        try:
            gids = decode_layer_data(data_elem)
        except Exception as exc:
            print(f"  WARN   skipping layer {name!r}: {exc}")
            continue

        layer = {
            "name":         name,
            "gids":         gids,
            "scene_layer":  infer_scene_layer(name),
        }
        safe = write_tilemap_xml(map_info, layer, tilesets, assets_dir, map_stem)
        layer_safe_names.append(safe)

    if args.scene:
        write_scene_xml(map_stem, layer_safe_names, map_info, assets_dir)

    print(f"done — {len(layer_safe_names)} layer(s)")


if __name__ == "__main__":
    main()
