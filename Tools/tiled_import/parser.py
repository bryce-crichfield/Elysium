import xml.etree.ElementTree as ET
from pathlib import Path
from PIL import Image
from typing import Union
from model import TiledTileset, TiledObject, TiledObjectLayer, TiledTileLayer


def ParseTileset(tsx_path: str) -> TiledTileset:
    """Parse a .tsx tileset file."""
    tree = ET.parse(tsx_path)
    root = tree.getroot()

    name = root.attrib["name"]
    tile_width = int(root.attrib["tilewidth"])
    tile_height = int(root.attrib["tileheight"])
    tile_count = int(root.attrib["tilecount"])
    columns = int(root.attrib["columns"])

    # Load tilesheet image
    image_el = root.find("image")
    tilesheet = None
    tiles: dict[int, Image.Image] = {}

    if image_el is not None:
        img_path = Path(tsx_path).parent / image_el.attrib["source"]
        tilesheet = Image.open(img_path).convert("RGBA")
        tiles = _slice_tilesheet(tilesheet, tile_width, tile_height, columns, tile_count)

    return TiledTileset(
        name=name,
        tileWidth=tile_width,
        tileHeight=tile_height,
        tileCount=tile_count,
        columns=columns,
        tilesheet=tilesheet,
        tiles=tiles,
    )


def _slice_tilesheet(
    sheet: Image.Image,
    tile_w: int,
    tile_h: int,
    columns: int,
    tile_count: int,
) -> dict[int, Image.Image]:
    """Cut a tilesheet into individual tile images keyed by GID-local index."""
    tiles = {}
    for i in range(tile_count):
        col = i % columns
        row = i // columns
        box = (col * tile_w, row * tile_h, (col + 1) * tile_w, (row + 1) * tile_h)
        tiles[i] = sheet.crop(box)
    return tiles


def ParseCsvData(data_el: ET.Element, width: int, height: int) -> list[list[int]]:
    """Parse a CSV-encoded <data> block into a 2D list."""
    raw = data_el.text.strip()
    flat = [int(v) for v in raw.split(",")]
    return [flat[row * width:(row + 1) * width] for row in range(height)]


def ParseTileLayer(el: ET.Element) -> TiledTileLayer:
    name = el.attrib["name"]
    layer_id = int(el.attrib["id"])
    width = int(el.attrib["width"])
    height = int(el.attrib["height"])

    data_el = el.find("data")
    encoding = data_el.attrib.get("encoding", "xml")
    if encoding != "csv":
        raise NotImplementedError(f"Tile layer encoding '{encoding}' is not supported; use CSV.")

    data = ParseCsvData(data_el, width, height)
    return TiledTileLayer(name=name, id=layer_id, width=width, height=height, data=data)


def ParseObject(el: ET.Element) -> TiledObject:
    return TiledObject(
        id=int(el.attrib["id"]),
        gid=int(el.attrib.get("gid", 0)),
        x=float(el.attrib["x"]),
        y=float(el.attrib["y"]),
        width=int(float(el.attrib.get("width", 0))),
        height=int(float(el.attrib.get("height", 0))),
    )


def ParseObjectLayer(el: ET.Element) -> TiledObjectLayer:
    return TiledObjectLayer(
        name=el.attrib["name"],
        id=int(el.attrib["id"]),
        draworder=el.attrib.get("draworder", "topdown"),
        objects=[ParseObject(obj) for obj in el.findall("object")],
    )


def ParseMap(tmx_path: str) -> tuple[list[TiledTileLayer], list[TiledObjectLayer], dict[str, TiledTileset]]:
    """
    Parse a .tmx map file.
    Returns (tile_layers, object_layers, tilesets) where tilesets is keyed by name.
    """
    tree = ET.parse(tmx_path)
    root = tree.getroot()
    base_dir = Path(tmx_path).parent

    # Load referenced tilesets
    tilesets: dict[str, TiledTileset] = {}
    for ts_el in root.findall("tileset"):
        source = ts_el.attrib.get("source")
        if source:
            tsx_path = base_dir / source
            ts = ParseTileset(str(tsx_path))
            tilesets[ts.name] = ts

    tile_layers: list[TiledTileLayer] = []
    object_layers: list[TiledObjectLayer] = []

    for child in root:
        if child.tag == "layer":
            tile_layers.append(ParseTileLayer(child))
        elif child.tag == "objectgroup":
            object_layers.append(ParseObjectLayer(child))

    return tile_layers, object_layers, tilesets