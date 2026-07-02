import argparse
import json
import sys
from dataclasses import asdict
from pathlib import Path

try:
    from gooey import Gooey, GooeyParser
    GOOEY_AVAILABLE = True
except ImportError:
    GOOEY_AVAILABLE = False

from parser import ParseMap


def build_parser(parser_class):
    parser = parser_class(
        prog="tiled_inspect",
        description="Parse a Tiled .tmx map file and display its structure",
    )
    parser.add_argument(
        "input",
        metavar="INPUT_TMX",
        help="Path to the .tmx map file",
        widget="FileChooser" if GOOEY_AVAILABLE else None,
    )
    parser.add_argument(
        "--output",
        metavar="OUTPUT_JSON",
        default=None,
        help="Optional: write structured output to a JSON file instead of stdout",
    )
    parser.add_argument(
        "--pretty",
        action="store_true",
        default=True,
        help="Pretty-print JSON output (default: true)",
    )
    return parser


def summarize(tile_layers, object_layers, tilesets) -> dict:
    """Convert parsed map data into a JSON-serialisable dict."""

    def object_layer_to_dict(ol):
        return {
            "name": ol.name,
            "id": ol.id,
            "draworder": ol.draworder,
            "objects": [
                {
                    "id": o.id,
                    "gid": o.gid,
                    "x": o.x,
                    "y": o.y,
                    "width": o.width,
                    "height": o.height,
                }
                for o in ol.objects
            ],
        }

    def tile_layer_to_dict(tl):
        return {
            "name": tl.name,
            "id": tl.id,
            "width": tl.width,
            "height": tl.height,
            # Render first row only to keep output readable; full data below
            "data_preview": tl.data[0] if tl.data else [],
            "data": tl.data,
        }

    def tileset_to_dict(ts):
        return {
            "name": ts.name,
            "tileWidth": ts.tileWidth,
            "tileHeight": ts.tileHeight,
            "tileCount": ts.tileCount,
            "columns": ts.columns,
            "tilesheet_loaded": ts.tilesheet is not None,
            "tiles_sliced": len(ts.tiles),
        }

    return {
        "tilesets": {name: tileset_to_dict(ts) for name, ts in tilesets.items()},
        "tile_layers": [tile_layer_to_dict(tl) for tl in tile_layers],
        "object_layers": [object_layer_to_dict(ol) for ol in object_layers],
    }


def run(args):
    tmx_path = Path(args.input)
    if not tmx_path.exists():
        print(f"ERROR: file not found: {tmx_path}", file=sys.stderr)
        sys.exit(1)
    if tmx_path.suffix.lower() != ".tmx":
        print(f"WARNING: expected a .tmx file, got '{tmx_path.suffix}'", file=sys.stderr)

    print(f"Parsing: {tmx_path}")
    tile_layers, object_layers, tilesets = ParseMap(str(tmx_path))

    result = summarize(tile_layers, object_layers, tilesets)
    indent = 2 if args.pretty else None
    output_str = json.dumps(result, indent=indent)

    if args.output:
        out_path = Path(args.output)
        out_path.write_text(output_str, encoding="utf-8")
        print(f"Output written to: {out_path}")
    else:
        print(output_str)


def main():
    if GOOEY_AVAILABLE:
        @Gooey(
            program_name="Tiled Map Inspector",
            program_description="Parse a Tiled .tmx map file and display its structure",
            default_size=(600, 400),
        )
        def gooey_main():
            parser = build_parser(GooeyParser)
            args = parser.parse_args()
            run(args)

        gooey_main()
    else:
        parser = build_parser(argparse.ArgumentParser)
        args = parser.parse_args()
        run(args)


if __name__ == "__main__":
    main()