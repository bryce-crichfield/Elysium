"""
anim_tile.py  —  desert ground tile (isometric), single static frame
API: draw(canvas: PIL.Image.Image, t: float) -> None
     FRAME_W, FRAME_H, FPS
"""

from PIL import Image
import Tools.SpriteAnimator.sprite_graphics as sprite_graphics

FRAME_W = 64
FRAME_H = 32
FPS     = 12

TOTAL_FRAMES = 1
DURATION     = TOTAL_FRAMES / FPS

# ── palette ───────────────────────────────────────────────────────────────────
SAND_BASE   = (210, 170,  90, 255)
SAND_LIGHT  = (230, 195, 120, 255)
SAND_SHADOW = (160, 120,  50, 255)
CRACK       = (130,  95,  35, 200)
PEBBLE      = (160, 120,  50, 255)
RIM_LIGHT   = (245, 215, 140, 255)
RIM_SHADOW  = (130,  90,  30, 255)

# ── geometry ──────────────────────────────────────────────────────────────────
DIAMOND = [(32, 0), (63, 15), (32, 31), (0, 15)]
CENTER  = (32, 15)

PEBBLES = [(22, 12), (40, 10), (48, 18), (30, 20), (20, 22)]
CRACKS  = [(28, 13, 24, 16), (28, 13, 32, 17), (42, 11, 38, 14)]


def draw(canvas: Image.Image, t: float) -> None:
    sprite_graphics.clear(canvas)

    cx, cy = CENTER

    # filled base
    sprite_graphics.fill_polygon(canvas, DIAMOND, SAND_BASE)

    # simple directional shading: brighten top-right half, darken bottom-left
    for y in range(FRAME_H):
        for x in range(FRAME_W):
            if abs(x - cx) / 31 + abs(y - cy) / 15 > 1.0:
                continue
            if x > cx:
                sprite_graphics.draw_pixel(canvas, x, y, SAND_LIGHT)
            elif x < cx - 4:
                sprite_graphics.draw_pixel(canvas, x, y, SAND_SHADOW)

    # rim
    sprite_graphics.draw_line(canvas, 32,  0, 63, 15, RIM_LIGHT)
    sprite_graphics.draw_line(canvas,  0, 15, 32, 31, RIM_SHADOW)

    # cracks
    for x0, y0, x1, y1 in CRACKS:
        sprite_graphics.draw_line(canvas, x0, y0, x1, y1, CRACK)

    # pebbles
    for px, py in PEBBLES:
        sprite_graphics.draw_pixel(canvas, px,     py,     PEBBLE)
        sprite_graphics.draw_pixel(canvas, px + 1, py,     PEBBLE)
        sprite_graphics.draw_pixel(canvas, px,     py + 1, PEBBLE)