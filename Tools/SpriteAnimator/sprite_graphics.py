"""
gfx.py  —  shared pixel-art drawing primitives for animation modules.

All functions operate on a PIL RGBA Image.  No state is kept here;
callers manage their own canvases.

Public API
──────────
  clear(img)
  draw_pixel(img, x, y, color)
  draw_line(img, x0, y0, x1, y1, color)
  draw_line_thick(img, x0, y0, x1, y1, color, thickness)
  fill_polygon(img, points, color)
  polygon_perimeter(points)  -> list[(x, y)]
  draw_polygon(img, points, color)
  draw_polygon_thick(img, points, color, thickness)
  scale_polygon(points, cx, cy, scale) -> list[(x, y)]
  draw_iso_ellipse(img, cx, cy, rx, color)   iso-space filled ellipse
"""

import math
from PIL import Image, ImageDraw

# ── canvas ────────────────────────────────────────────────────────────────────

def clear(img: Image.Image) -> None:
    """Erase the canvas to fully transparent."""
    img.paste((0, 0, 0, 0), [0, 0, img.width, img.height])


# ── pixel ─────────────────────────────────────────────────────────────────────

def draw_pixel(img: Image.Image, x: int, y: int, color: tuple) -> None:
    """Plot a single pixel, clipping silently if out of bounds."""
    if 0 <= x < img.width and 0 <= y < img.height:
        img.putpixel((x, y), color)


# ── line ──────────────────────────────────────────────────────────────────────

def _bresenham(x0: int, y0: int, x1: int, y1: int) -> list:
    """Return all integer points on the line from (x0,y0) to (x1,y1)."""
    pts = []
    dx = abs(x1 - x0); sx = 1 if x0 < x1 else -1
    dy = -abs(y1 - y0); sy = 1 if y0 < y1 else -1
    err = dx + dy
    x, y = x0, y0
    while True:
        pts.append((x, y))
        if x == x1 and y == y1:
            break
        e2 = 2 * err
        if e2 >= dy: err += dy; x += sx
        if e2 <= dx: err += dx; y += sy
    return pts


def draw_line(img: Image.Image,
              x0: int, y0: int, x1: int, y1: int,
              color: tuple) -> None:
    """Draw a 1-pixel Bresenham line."""
    for x, y in _bresenham(x0, y0, x1, y1):
        draw_pixel(img, x, y, color)


def draw_line_thick(img: Image.Image,
                    x0: int, y0: int, x1: int, y1: int,
                    color: tuple, thickness: int = 2) -> None:
    """Draw a line thickened by pushing extra pixels toward the canvas centre."""
    cx = img.width  // 2
    cy = img.height // 2
    for x, y in _bresenham(x0, y0, x1, y1):
        draw_pixel(img, x, y, color)
        if thickness > 1:
            odx = cx - x; ody = cy - y
            dist = math.hypot(odx, ody)
            if dist > 0:
                nx = odx / dist; ny = ody / dist
                for t in range(1, thickness):
                    draw_pixel(img, round(x + nx * t), round(y + ny * t), color)


# ── polygon ───────────────────────────────────────────────────────────────────

def polygon_perimeter(points: list) -> list:
    """
    Return all integer perimeter pixels for a closed polygon (no duplicates
    at join points).  Points is a list of (x, y) tuples.
    """
    out = []
    n = len(points)
    for i in range(n):
        seg = _bresenham(*points[i], *points[(i + 1) % n])
        out.extend(seg[:-1])   # exclude endpoint to avoid duplicates
    return out


def fill_polygon(img: Image.Image, points: list, color: tuple) -> None:
    """Filled polygon via PIL (anti-aliasing off, pixel-exact)."""
    ImageDraw.Draw(img).polygon(points, fill=color)


def draw_polygon(img: Image.Image, points: list, color: tuple) -> None:
    """Draw the 1-pixel outline of a polygon."""
    for x, y in polygon_perimeter(points):
        draw_pixel(img, x, y, color)


def draw_polygon_thick(img: Image.Image, points: list,
                       color: tuple, thickness: int = 2) -> None:
    """Draw a polygon outline thickened inward toward the canvas centre."""
    cx = img.width  // 2
    cy = img.height // 2
    for x, y in polygon_perimeter(points):
        draw_pixel(img, x, y, color)
        if thickness > 1:
            odx = cx - x; ody = cy - y
            dist = math.hypot(odx, ody)
            if dist > 0:
                nx = odx / dist; ny = ody / dist
                for t in range(1, thickness):
                    draw_pixel(img, round(x + nx * t), round(y + ny * t), color)


# ── transform ─────────────────────────────────────────────────────────────────

def scale_polygon(points: list, cx: float, cy: float, scale: float) -> list:
    """
    Scale a polygon uniformly around an arbitrary centre (cx, cy).
    Returns a new list of rounded integer (x, y) tuples.
    """
    return [(round(cx + (px - cx) * scale),
             round(cy + (py - cy) * scale))
            for px, py in points]


# ── iso ellipse ───────────────────────────────────────────────────────────────

def draw_iso_ellipse(img: Image.Image,
                     cx: int, cy: int,
                     rx: int, color: tuple) -> None:
    """
    Draw a filled isometric-space ellipse centred at (cx, cy).
    In iso projection the y-radius is half the x-radius (rx), matching
    the standard 2:1 tile ratio.  Uses the diamond-distance test:
        |dx/rx| + |dy/(rx/2)| <= 1
    which is equivalent to  |dx| * 2 + |dy| * 4 <= rx * 2 * 2.
    """
    ry = max(1, rx // 2)
    for dy in range(-ry, ry + 1):
        for dx in range(-rx, rx + 1):
            if abs(dx) / rx + abs(dy) / ry <= 1.0:
                draw_pixel(img, cx + dx, cy + dy, color)