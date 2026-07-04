"""CLI entry point for the Elysium SVG exporter.

Parses an SVG file and writes an Elysium prefab XML + any embedded images
under an assets root, using the same `SvgParser` / `write_prefab_file` the
GUI (main.py) uses — main.py calls `export()` below rather than duplicating
this glue, so the GUI is a thin wrapper over this CLI's logic. This is also
the entry point CMake invokes as a build step to regenerate SVG-derived
prefabs into the runtime Assets directory.
"""
from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path
from typing import List, Optional, Tuple

from prefab_writer import write_prefab_file
from svg_parser import SvgParser


def _sanitize_name(name: str) -> str:
    return re.sub(r"[^A-Za-z0-9_]", "_", name)


def export(
    svg_path: str,
    assets_root: str,
    prefab_name: Optional[str] = None,
    layer_name: str = "default",
    curve_segments: int = 16,
) -> Tuple[List[str], List[str]]:
    """Parses svg_path and writes a prefab + images under assets_root.

    Returns (warnings, written_image_paths). Raises on parse/export failure.
    """
    if prefab_name is None:
        prefab_name = _sanitize_name(Path(svg_path).stem)

    parser = SvgParser(curve_segments=curve_segments)
    parsed_root = parser.parse_file(svg_path)

    output_xml_path = os.path.join(assets_root, "Scenes", "Prefabs", f"{prefab_name}.xml")
    warnings, images = write_prefab_file(
        parsed_root, prefab_name, output_xml_path, assets_root, layer_name=layer_name,
    )
    return parser.warnings + warnings, images


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="svg_export",
        description="Export an SVG file to an Elysium prefab XML + textures.",
    )
    parser.add_argument("svg", metavar="SVG", type=Path, help="Path to the source .svg file")
    parser.add_argument(
        "--output-dir", "--assets-root", dest="assets_root", metavar="DIR", type=Path, required=True,
        help="Assets root to write into (prefab goes to <DIR>/Scenes/Prefabs/, textures to <DIR>/Textures/<name>/)",
    )
    parser.add_argument(
        "--name", metavar="NAME", default=None,
        help="Prefab name. Default: sanitized SVG filename (without extension).",
    )
    parser.add_argument("--layer", metavar="NAME", default="default", help="Root layer name. Default: default.")
    parser.add_argument(
        "--curve-quality", dest="curve_segments", metavar="N", type=int, default=16,
        help="Bezier/arc flattening segment count. Default: 16.",
    )
    parser.add_argument("--verbose", "-v", action="store_true", help="Print parsed warnings verbosely.")
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if not args.svg.is_file():
        print(f"error: SVG file not found: {args.svg}", file=sys.stderr)
        return 1

    try:
        warnings, images = export(
            str(args.svg), str(args.assets_root),
            prefab_name=args.name, layer_name=args.layer, curve_segments=args.curve_segments,
        )
    except Exception as e:  # noqa: BLE001 - report any parse/export failure to the caller
        print(f"error: export failed: {e}", file=sys.stderr)
        return 1

    print(f"Exported prefab for {args.svg} into {args.assets_root}")
    for img in images:
        print(f"  wrote image: {img}")
    if args.verbose or warnings:
        for w in warnings:
            print(f"  WARNING: {w}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
