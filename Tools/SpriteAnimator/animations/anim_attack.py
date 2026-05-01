"""
anim_attack.py  —  targeting reticle lock-on iso cursor
API: draw(canvas: PIL.Image.Image, t: float) -> None
     FRAME_W, FRAME_H, FPS
"""

import math
from PIL import Image
import Tools.SpriteAnimator.sprite_graphics as sprite_graphics

FRAME_W = 64
FRAME_H = 32
FPS     = 12

TOTAL_FRAMES = 16
DURATION     = TOTAL_FRAMES / FPS

# ── palette ───────────────────────────────────────────────────────────────────
RED_BRIGHT = (220,  50,  50, 255)
YELLOW     = (255, 220,  40, 255)

# ── geometry ──────────────────────────────────────────────────────────────────
DIAMOND     = [(32, 0), (63, 15), (32, 31), (0, 15)]
CORNERS     = [(32, 0), (63, 15), (32, 31), (0, 15)]
CENTER      = (32, 15)
BRACKET_LEN = 6

# iso edge directions for each corner's two bracket arms
_ISO_DIRS = [
    [(2, 1),  (-2, 1) ],   # top
    [(-2, 1), (-2, -1)],   # right
    [(2, -1), (-2, -1)],   # bottom
    [(2, 1),  (2, -1) ],   # left
]


def _draw_bracket(canvas, bx, by, corner_idx, color, length=BRACKET_LEN):
    for arm_dx, arm_dy in _ISO_DIRS[corner_idx]:
        x, y = bx, by
        for _ in range(length):
            sprite_graphics.draw_pixel(canvas, x, y, color)
            # one pixel of thickness perpendicular to the arm
            if corner_idx in (0, 2):
                sprite_graphics.draw_pixel(canvas, x, y + (1 if corner_idx == 0 else -1), color)
            else:
                sprite_graphics.draw_pixel(canvas, x + (1 if corner_idx == 3 else -1), y, color)
            x += arm_dx; y += arm_dy


def draw(canvas: Image.Image, t: float) -> None:
    sprite_graphics.clear(canvas)

    frame_index = int((t % DURATION) * FPS) % TOTAL_FRAMES
    cx, cy = CENTER

    # ── phase 0–5: brackets slide in ─────────────────────────────────────────
    if frame_index <= 5:
        slide_t = frame_index / 5.0
        OFFSET  = 12
        for i, (px, py) in enumerate(CORNERS):
            odx = px - cx; ody = py - cy
            dist = math.hypot(odx, ody)
            nx = odx / dist; ny = ody / dist
            off = OFFSET * (1.0 - slide_t)
            bx  = round(px + nx * off)
            by  = round(py + ny * off)
            _draw_bracket(canvas, bx, by, i, (220, 50, 50, int(100 + 155 * slide_t)))

    # ── phase 6–9: locked, center dot pulses ─────────────────────────────────
    elif frame_index <= 9:
        for i, (px, py) in enumerate(CORNERS):
            _draw_bracket(canvas, px, py, i, RED_BRIGHT)
        pulse = math.sin((frame_index - 6) / 3.0 * math.pi)
        dot_r = 2 + round(pulse * 1.5)
        sprite_graphics.draw_iso_ellipse(canvas, cx, cy, dot_r, (220, 50, 50, int(200 + 55 * abs(pulse))))
        # thin crosshair
        for x in range(cx - 6, cx + 7):
            sprite_graphics.draw_pixel(canvas, x, cy, (220, 50, 50, 140))
        for y in range(cy - 3, cy + 4):
            sprite_graphics.draw_pixel(canvas, cx,     y, (220, 50, 50, 140))
            sprite_graphics.draw_pixel(canvas, cx + 1, y, (220, 50, 50, 140))

    # ── phase 10–12: impact flash ─────────────────────────────────────────────
    elif frame_index <= 12:
        flash_t = (frame_index - 10) / 2.0
        sprite_graphics.fill_polygon(canvas, DIAMOND, (220, 50, 50, int(180 * (1.0 - flash_t * 0.5))))
        for i, (px, py) in enumerate(CORNERS):
            _draw_bracket(canvas, px, py, i, YELLOW if frame_index == 10 else RED_BRIGHT)
        if frame_index == 10:
            for angle in range(0, 360, 30):
                rad = math.radians(angle)
                for r in range(3, 9):
                    sprite_graphics.draw_pixel(canvas,
                                   round(cx + math.cos(rad) * r * 2),
                                   round(cy + math.sin(rad) * r),
                                   YELLOW)

    # ── phase 13–15: disperse / aftermath ────────────────────────────────────
    else:
        fade_t = (frame_index - 13) / 2.0
        alpha  = int(200 * (1.0 - fade_t))
        OFFSET = round(fade_t * 8)
        for i, (px, py) in enumerate(CORNERS):
            odx = px - cx; ody = py - cy
            dist = math.hypot(odx, ody)
            nx = odx / dist; ny = ody / dist
            bx = round(px + nx * OFFSET)
            by = round(py + ny * OFFSET)
            _draw_bracket(canvas, bx, by, i, (220, 50, 50, alpha))
        sprite_graphics.fill_polygon(canvas, DIAMOND, (220, 50, 50, int(40 * (1.0 - fade_t))))