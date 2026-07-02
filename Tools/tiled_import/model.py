from __future__ import annotations
from typing import Optional, Dict, List, Literal
from dataclasses import dataclass
from PIL import Image

@dataclass 
class TiledTileset:
    name: str
    tileWidth: int
    tileHeight: int
    tileCount: int
    columns: int
    tilesheet: Optional[Image.Image]
    tiles: Dict[int, Image.Image]

@dataclass 
class TiledObject:
    id: int
    gid: int
    x: float
    y: float
    width: int
    height: int

@dataclass
class TiledObjectLayer:
    name: str
    id: int
    draworder: Literal['topdown', 'index']
    objects: List[TiledObject]

@dataclass
class TiledTileLayer:
    name: str
    id: int
    width: int
    height: int
    data: List[List[int]]