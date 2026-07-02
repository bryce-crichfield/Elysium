"""
Spritesheet Stitcher
---------------------
Packs rendered per-direction frame folders (dir_00, dir_01, ...), each full
of frame_000.png, frame_001.png, ..., into a single spritesheet PNG -- one
row per camera direction, one column per animation frame.

This module only touches files on disk; it knows nothing about Blender or
the GUI. It's imported by app.py, and can also be run standalone:

    python stitch.py <render_dir> <output_sheet.png>

Requires Pillow in whichever Python environment imports/runs this file:
    pip install pillow
"""

import os
import re
import sys

try:
    from PIL import Image
except ImportError:
    Image = None  # Checked at call time so importing this module never hard-fails.

PILLOW_AVAILABLE = Image is not None


def natural_key(s):
    return [int(t) if t.isdigit() else t for t in re.split(r"(\d+)", s)]


def sanitize_name(name):
    """Turn an arbitrary filename into a safe folder/file component."""
    base = os.path.splitext(os.path.basename(name))[0]
    return re.sub(r"[^A-Za-z0-9_\-]+", "_", base).strip("_") or "anim"


def stitch_spritesheet(render_dir, output_sheet, progress_cb=None, log_cb=None):
    """Pack rendered per-direction frame folders (dir_00, dir_01, ...) into one spritesheet PNG."""
    if Image is None:
        raise RuntimeError("Pillow is not installed in this Python environment. Run: pip install pillow")

    if not os.path.isdir(render_dir):
        raise RuntimeError(f"Render directory not found: {render_dir}")

    direction_folders = sorted(
        [d for d in os.listdir(render_dir) if d.startswith("dir_") and os.path.isdir(os.path.join(render_dir, d))],
        key=natural_key
    )
    if not direction_folders:
        raise RuntimeError(f"No 'dir_XX' folders found in {render_dir}")

    rows = []
    for d in direction_folders:
        folder = os.path.join(render_dir, d)
        frame_files = sorted(
            [f for f in os.listdir(folder) if f.lower().endswith(".png")],
            key=natural_key
        )
        if not frame_files:
            if log_cb:
                log_cb(f"Warning: no PNG frames found in {folder}, skipping.")
            continue
        rows.append([os.path.join(folder, f) for f in frame_files])

    if not rows:
        raise RuntimeError("No rendered frames found in any direction folder.")

    with Image.open(rows[0][0]) as sample:
        frame_w, frame_h = sample.size

    num_cols = max(len(r) for r in rows)
    num_rows = len(rows)

    sheet = Image.new("RGBA", (frame_w * num_cols, frame_h * num_rows), (0, 0, 0, 0))

    total_pastes = sum(len(r) for r in rows)
    done = 0

    for row_idx, frame_paths in enumerate(rows):
        for col_idx, frame_path in enumerate(frame_paths):
            with Image.open(frame_path) as frame:
                if frame.size != (frame_w, frame_h):
                    frame = frame.resize((frame_w, frame_h))
                sheet.paste(frame, (col_idx * frame_w, row_idx * frame_h))
            done += 1
            if progress_cb:
                progress_cb(done, total_pastes)

    os.makedirs(os.path.dirname(output_sheet) or ".", exist_ok=True)
    sheet.save(output_sheet)

    return {
        "output_sheet": output_sheet,
        "num_cols": num_cols,
        "num_rows": num_rows,
        "frame_w": frame_w,
        "frame_h": frame_h,
    }


def find_animation_sets(render_dir):
    """Find subfolders of render_dir that look like completed per-animation render sets
    (i.e. contain at least one dir_XX folder)."""
    if not os.path.isdir(render_dir):
        return []
    sets = []
    for name in sorted(os.listdir(render_dir)):
        full = os.path.join(render_dir, name)
        if not os.path.isdir(full):
            continue
        if any(sub.startswith("dir_") for sub in os.listdir(full)):
            sets.append(name)
    return sets


def _main():
    if len(sys.argv) != 3:
        print("Usage: python stitch.py <render_dir> <output_sheet.png>")
        sys.exit(1)

    render_dir, output_sheet = sys.argv[1], sys.argv[2]
    result = stitch_spritesheet(render_dir, output_sheet, log_cb=print)
    print(f"Spritesheet saved: {result['output_sheet']}")
    print(f"Grid: {result['num_cols']} columns x {result['num_rows']} rows, "
          f"frame size {result['frame_w']}x{result['frame_h']}")


if __name__ == "__main__":
    _main()