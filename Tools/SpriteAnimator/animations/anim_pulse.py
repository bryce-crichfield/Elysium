"""
anim_pulse.py  —  grow / pulse iso cursor
API: draw(canvas: PIL.Image.Image, t: float) -> None
     FRAME_W, FRAME_H, FPS
"""

import math
from PIL import Image
import Tools.SpriteAnimator.sprite_graphics as sprite_graphics

FRAME_W = 64
FRAME_H = 32
FPS     = 12

LOOP_PERIOD = 16 / FPS

# ── geometry ──────────────────────────────────────────────────────────────────
_BASE_DIAMOND = [(32, 0), (63, 15), (32, 31), (0, 15)]
CENTER        = (32, 15)


def draw(canvas: Image.Image, t: float) -> None:
    sprite_graphics.clear(canvas)

    cx, cy = CENTER
    phase  = (t % LOOP_PERIOD) / LOOP_PERIOD

    ease        = (1.0 - math.cos(phase * 2 * math.pi)) / 2.0
    scale       = 1.0 + ease * 0.28

    trail_phase = ((t - 0.12) % LOOP_PERIOD) / LOOP_PERIOD
    trail_ease  = (1.0 - math.cos(trail_phase * 2 * math.pi)) / 2.0
    trail_scale = 1.0 + trail_ease * 0.18

    # outer trail ring
    trail_pts   = sprite_graphics.scale_polygon(_BASE_DIAMOND, cx, cy, trail_scale)
    trail_alpha = int(80 * (1.0 - abs(trail_ease - ease)))
    sprite_graphics.draw_polygon(canvas, trail_pts, (255, 255, 255, max(0, trail_alpha)))

    # main fill + outline
    main_pts      = sprite_graphics.scale_polygon(_BASE_DIAMOND, cx, cy, scale)
    fill_alpha    = int(40 + ease * 80)
    outline_alpha = int(180 + ease * 75)

    sprite_graphics.fill_polygon(canvas, main_pts, (255, 255, 255, fill_alpha))
    sprite_graphics.draw_polygon_thick(canvas, main_pts,
                           (255, 255, 255, min(255, outline_alpha)),
                           thickness=2)

    # center dot
    dot_r = round(ease * 3)
    if dot_r > 0:
        sprite_graphics.draw_iso_ellipse(canvas, cx, cy, dot_r,
                             (255, 255, 255, int(200 * ease)))