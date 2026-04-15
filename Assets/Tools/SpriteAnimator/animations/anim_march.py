"""
anim_march.py  —  marching dashes iso cursor
API: draw(canvas: PIL.Image.Image, t: float) -> None
     FRAME_W, FRAME_H, FPS
"""

import math
from PIL import Image
import sprite_graphics as sprite_graphics

FRAME_W = 64
FRAME_H = 32
FPS     = 12

# ── palette ───────────────────────────────────────────────────────────────────
WHITE = (255, 255, 255, 160)
BLACK = (0,   0,   0,   255)

# ── geometry ──────────────────────────────────────────────────────────────────
DIAMOND   = [(32, 0), (63, 15), (32, 31), (0, 15)]
CENTER    = (32, 15)
DASH_ON   = 7
DASH_OFF  = 4
THICKNESS = 2

_PERIMETER = sprite_graphics.polygon_perimeter(DIAMOND)


def draw(canvas: Image.Image, t: float) -> None:
    sprite_graphics.clear(canvas)

    period = DASH_ON + DASH_OFF
    phase  = int((t * FPS) % len(_PERIMETER))
    cx, cy = CENTER

    sprite_graphics.fill_polygon(canvas, DIAMOND, WHITE)

    for i, (x, y) in enumerate(_PERIMETER):
        if (i + phase) % period < DASH_ON:
            # thicken each dash pixel inward toward the center
            sprite_graphics.draw_pixel(canvas, x, y, BLACK)
            dx = cx - x; dy = cy - y
            dist = math.hypot(dx, dy)
            if dist > 0:
                nx = dx / dist; ny = dy / dist
                for tk in range(1, THICKNESS):
                    sprite_graphics.draw_pixel(canvas,
                                   round(x + nx * tk),
                                   round(y + ny * tk), BLACK)