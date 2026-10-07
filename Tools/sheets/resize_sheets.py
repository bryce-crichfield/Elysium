#!/usr/bin/env python
"""Re-export Elysium sprite sheets at a smaller frame size and/or fewer frames.

A sheet is a rows x columns grid of equal frames (Sprites/<Name>.xml declares the grid).
This slices a sheet into its frames, optionally keeps every Nth frame, resizes each to a
target size, and reassembles the grid -- then rewrites the columns= attribute in the
sprite definition to match.

Two details that matter and are easy to get wrong:

  * Albedo is resized in PREMULTIPLIED alpha. Resizing straight-alpha RGBA blends the
    colour of fully transparent texels (usually black) into the edge, which shows up as a
    dark fringe around every sprite. Premultiply, resize, then un-premultiply.

  * Normal maps are RENORMALIZED after resizing. Averaging two unit vectors gives a
    shorter one, and a short normal reads as a dim surface. (Sdf/Material/Texture.glsl
    also normalizes on sample; doing it here too keeps 8-bit precision.)

Usage:
    python Tools/sheets/resize_sheets.py Projects/DemoGame --frame 128 --keep-every 2 \
        --sprites Knight Archer Vampire [--sheets Idle] [--dry-run]
"""
import argparse
import os
import re
import sys

from PIL import Image


def is_normal_map(key):
    return key == "normal"


def resize_frame(frame, size, normal):
    if frame.size == (size, size):
        out = frame
    elif normal:
        out = frame.resize((size, size), Image.LANCZOS)
    else:
        # Premultiply -> resize -> un-premultiply, so transparent texels cannot bleed
        # their colour into the silhouette edge.
        r, g, b, a = frame.split()
        pm = Image.merge("RGBA", [
            Image.fromarray((_np(ch) * _np(a) / 255.0).round().astype("uint8"))
            for ch in (r, g, b)
        ] + [a]).resize((size, size), Image.LANCZOS)
        pr, pg, pb, pa = pm.split()
        an = _np(pa).clip(1, 255)
        out = Image.merge("RGBA", [
            Image.fromarray((_np(ch) * 255.0 / an).clip(0, 255).round().astype("uint8"))
            for ch in (pr, pg, pb)
        ] + [pa])
    if normal:
        out = renormalize(out)
    return out


def renormalize(img):
    import numpy as np
    a = np.asarray(img).astype("float32")
    v = a[..., :3] / 127.5 - 1.0
    length = np.sqrt((v * v).sum(axis=-1, keepdims=True))
    # A fully flat or empty texel has no direction to preserve; leave it pointing at the
    # camera rather than dividing by zero.
    flat = length < 1e-4
    v = np.where(flat, np.array([0.0, 0.0, 1.0], "float32"), v / np.maximum(length, 1e-4))
    a[..., :3] = (v + 1.0) * 127.5
    return Image.fromarray(a.round().clip(0, 255).astype("uint8"), "RGBA")


def _np(ch):
    import numpy as np
    return np.asarray(ch).astype("float32")


def process(path, rows, cols, frame_size, keep_every, normal, dry_run):
    with Image.open(path) as im:
        im = im.convert("RGBA")
        fw, fh = im.width // cols, im.height // rows
        if fw * cols != im.width or fh * rows != im.height:
            print("  ! %s is %dx%d, not divisible by the %dx%d grid; skipped"
                  % (os.path.basename(path), im.width, im.height, rows, cols))
            return None
        keep = list(range(0, cols, keep_every))
        out = Image.new("RGBA", (frame_size * len(keep), frame_size * rows), (0, 0, 0, 0))
        for row in range(rows):
            for dst, src in enumerate(keep):
                box = (src * fw, row * fh, (src + 1) * fw, (row + 1) * fh)
                cell = resize_frame(im.crop(box), frame_size, normal)
                out.paste(cell, (dst * frame_size, row * frame_size))
    before = os.path.getsize(path)
    print("  %-46s %5dx%-5d -> %5dx%-5d  cols %2d -> %2d  %5.1f MP -> %4.1f MP"
          % (os.path.relpath(path).replace(os.sep, "/"), im.width, im.height,
             out.width, out.height, cols, len(keep),
             im.width * im.height / 1e6, out.width * out.height / 1e6))
    if not dry_run:
        out.save(path, optimize=True)
        print("  %-46s %5.1f MB -> %4.1f MB on disk"
              % ("", before / 1048576, os.path.getsize(path) / 1048576))
    return len(keep)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("project", help="project root, e.g. Projects/DemoGame")
    ap.add_argument("--frame", type=int, required=True, help="target frame size in pixels (square)")
    ap.add_argument("--keep-every", type=int, default=1, help="keep every Nth frame (1 = all)")
    ap.add_argument("--sprites", nargs="+", required=True, help="sprite names, without .xml")
    ap.add_argument("--sheets", nargs="*", help="only these sheet names (default: all)")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    for name in args.sprites:
        xml_path = os.path.join(args.project, "Sprites", name + ".xml")
        if not os.path.exists(xml_path):
            print("missing sprite definition: %s" % xml_path)
            return 1
        text = open(xml_path, encoding="latin-1", newline="").read()
        print("%s" % xml_path.replace(os.sep, "/"))

        for tag in re.findall(r"<Sheet\b[^>]*/>", text):
            attrs = dict(re.findall(r'(\w+)="([^"]*)"', tag))
            if args.sheets and attrs.get("name") not in args.sheets:
                continue
            rows, cols = int(attrs["rows"]), int(attrs["columns"])
            new_cols = None
            for key in ("path", "normal", "emission"):
                rel = attrs.get(key)
                if not rel:
                    continue
                target = os.path.join(args.project, rel.replace("/", os.sep))
                if not os.path.exists(target):
                    print("  ! missing %s" % rel)
                    continue
                result = process(target, rows, cols, args.frame, args.keep_every,
                                 is_normal_map(key), args.dry_run)
                new_cols = result if result is not None else new_cols
            if new_cols is not None and new_cols != cols:
                updated = tag.replace('columns="%d"' % cols, 'columns="%d"' % new_cols)
                text = text.replace(tag, updated)

        if not args.dry_run:
            with open(xml_path, "wb") as f:
                f.write(text.encode("latin-1"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
