"""
sprite_viewer.py  —  Isometric cursor animation viewer

Usage:
    python sprite_viewer.py anim_march.py anim_attack.py anim_pulse.py
    python sprite_viewer.py                              # auto-discovers anim_*.py in cwd
    python sprite_viewer.py path/to/my_anim.py          # any combination

Each animation module must expose:
    FRAME_W : int           — frame width in pixels
    FRAME_H : int           — frame height in pixels
    FPS     : int | float   — playback rate
    draw(canvas: PIL.Image.Image, t: float) -> None
                            — render one frame at absolute time t (seconds)

Layout:
    One row per loaded animation.  Each row shows:
      [label col] | [live preview] | [spritesheet strip] …

Controls:
    Space       — pause / resume
    R           — reset time to 0
    +/-         — zoom in / out (1x … 8x)
    S           — export all spritesheets to ./exported_sheets/
    Q / Esc     — quit
"""

import importlib.util
import math
import os
import sys
import time
import tkinter as tk
from tkinter import filedialog, messagebox
from pathlib import Path

from PIL import Image, ImageTk

# ── config ────────────────────────────────────────────────────────────────────
SHEET_FRAMES   = 16          # how many frames to bake into each sheet strip
BG_COLOR       = "#1a1a1a"
ROW_BG_COLOR   = "#222222"
LABEL_BG       = "#111111"
LABEL_FG       = "#aaaaaa"
ACCENT_COLOR   = "#444444"
LIVE_BORDER    = "#555555"
BAR_BG         = "#151515"
BAR_FG         = "#888888"
DEFAULT_ZOOM   = 4
MIN_ZOOM       = 1
MAX_ZOOM       = 8
TICK_MS        = 16          # ~60 Hz UI tick; each anim uses its own FPS internally
SHEET_PAD      = 2           # pixels between frames on the sheet strip display
ROW_PAD        = 8           # vertical padding per row


# ── module loader ─────────────────────────────────────────────────────────────

def load_anim_module(path: str):
    """Load an animation module from a file path and validate its interface."""
    # Use a unique name each time so reloading the same path picks up changes
    unique_name = f"_anim_{abs(hash(path + str(os.path.getmtime(path))))}"
    spec = importlib.util.spec_from_file_location(unique_name, path)
    mod  = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)

    missing = [attr for attr in ("FRAME_W", "FRAME_H", "FPS", "draw")
               if not hasattr(mod, attr)]
    if missing:
        raise AttributeError(f"{path}: missing attributes {missing}")

    return mod


# ── animation state ───────────────────────────────────────────────────────────

class AnimRow:
    """Holds all state and cached images for one animation row."""

    def __init__(self, mod, path: str, zoom: int):
        self.mod    = mod
        self.name   = Path(path).stem
        self.path   = path
        self.zoom   = zoom

        self.fw     = mod.FRAME_W
        self.fh     = mod.FRAME_H
        self.fps    = mod.FPS

        # PIL canvas reused every draw call
        self._pil   = Image.new("RGBA", (self.fw, self.fh), (0, 0, 0, 0))

        # baked spritesheet frames (PIL RGBA images, original size)
        self._sheet_frames: list[Image.Image] = []
        self._bake_sheet()

        # cached zoomed PhotoImages for tkinter
        self._live_photo: ImageTk.PhotoImage | None  = None
        self._sheet_photos: list[ImageTk.PhotoImage] = []
        self._build_sheet_photos()

    # ── baking ────────────────────────────────────────────────────────────────

    def _bake_sheet(self):
        self._sheet_frames.clear()
        duration = SHEET_FRAMES / self.fps
        for i in range(SHEET_FRAMES):
            t = i / self.fps
            canvas = Image.new("RGBA", (self.fw, self.fh), (0, 0, 0, 0))
            self.mod.draw(canvas, t)
            self._sheet_frames.append(canvas.copy())

    def _build_sheet_photos(self):
        self._sheet_photos.clear()
        for frame in self._sheet_frames:
            big = frame.resize(
                (self.fw * self.zoom, self.fh * self.zoom), Image.NEAREST)
            self._sheet_photos.append(ImageTk.PhotoImage(big))

    # ── live frame ────────────────────────────────────────────────────────────

    def render_live(self, t: float) -> ImageTk.PhotoImage:
        self._pil.paste((0, 0, 0, 0), [0, 0, self.fw, self.fh])
        self.mod.draw(self._pil, t)
        big = self._pil.resize(
            (self.fw * self.zoom, self.fh * self.zoom), Image.NEAREST)
        self._live_photo = ImageTk.PhotoImage(big)
        return self._live_photo

    # ── zoom change ───────────────────────────────────────────────────────────

    def set_zoom(self, zoom: int):
        self.zoom = zoom
        self._build_sheet_photos()

    # ── export ────────────────────────────────────────────────────────────────

    def get_sheet_image(self) -> Image.Image:
        """Return a PIL RGBA strip of all baked frames (unsaved)."""
        sheet = Image.new("RGBA", (self.fw * SHEET_FRAMES, self.fh))
        for i, frame in enumerate(self._sheet_frames):
            sheet.paste(frame, (i * self.fw, 0), frame)
        return sheet


# ── main application ──────────────────────────────────────────────────────────

class SpriteViewer:

    def __init__(self, root: tk.Tk, anim_paths: list[str]):
        self.root     = root
        self.zoom     = DEFAULT_ZOOM
        self.paused   = False
        self.t_start  = time.perf_counter()
        self.t_paused = 0.0
        self.t_offset = 0.0   # accumulated pause time

        root.title("Sprite Viewer")
        root.configure(bg=BG_COLOR)
        root.resizable(True, True)

        self._build_toolbar()
        self._build_scroll_area()

        # load modules
        self.rows: list[AnimRow] = []
        errors = []
        for p in anim_paths:
            try:
                mod = load_anim_module(p)
                self.rows.append(AnimRow(mod, p, self.zoom))
            except Exception as e:
                errors.append(f"  {p}: {e}")

        if errors:
            print("Warnings — could not load:\n" + "\n".join(errors))

        self._row_widgets = []
        if not self.rows:
            self._show_empty()
        else:
            self._build_rows()

        self._bind_keys()
        self._tick()

    # ── toolbar ───────────────────────────────────────────────────────────────

    def _build_toolbar(self):
        bar = tk.Frame(self.root, bg=BAR_BG, height=32)
        bar.pack(side=tk.TOP, fill=tk.X)
        bar.pack_propagate(False)

        def btn(text, cmd, tip=""):
            b = tk.Button(bar, text=text, command=cmd,
                          bg=BAR_BG, fg=BAR_FG, relief=tk.FLAT,
                          activebackground=ACCENT_COLOR, activeforeground="#ffffff",
                          padx=8, pady=0, font=("Courier", 10, "bold"), bd=0)
            b.pack(side=tk.LEFT, padx=2, pady=4)
            return b

        self._btn_pause = btn("⏸ PAUSE", self._toggle_pause)
        btn("↺ RESET",  self._reset_time)
        btn("+ ZOOM",   self._zoom_in)
        btn("- ZOOM",   self._zoom_out)
        btn("📂 LOAD",  self._load_files)
        btn("⬇ EXPORT", self._export_all)
        btn("✕ QUIT",   self.root.destroy)

        self._zoom_label = tk.Label(bar, text=f"  {self.zoom}x",
                                    bg=BAR_BG, fg=LABEL_FG,
                                    font=("Courier", 10))
        self._zoom_label.pack(side=tk.LEFT)

        self._status = tk.Label(bar, text="", bg=BAR_BG, fg="#666666",
                                font=("Courier", 9))
        self._status.pack(side=tk.RIGHT, padx=8)

    # ── scroll area ───────────────────────────────────────────────────────────

    def _build_scroll_area(self):
        frame = tk.Frame(self.root, bg=BG_COLOR)
        frame.pack(fill=tk.BOTH, expand=True)

        self._canvas = tk.Canvas(frame, bg=BG_COLOR, highlightthickness=0)
        scrollbar    = tk.Scrollbar(frame, orient=tk.VERTICAL,
                                    command=self._canvas.yview)
        self._canvas.configure(yscrollcommand=scrollbar.set)

        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self._canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self._inner = tk.Frame(self._canvas, bg=BG_COLOR)
        self._inner_id = self._canvas.create_window(
            (0, 0), window=self._inner, anchor="nw")

        self._inner.bind("<Configure>", self._on_inner_configure)
        self._canvas.bind("<Configure>", self._on_canvas_configure)

        # mouse wheel scroll
        self._canvas.bind_all("<MouseWheel>",
                               lambda e: self._canvas.yview_scroll(
                                   -1 if e.delta > 0 else 1, "units"))
        self._canvas.bind_all("<Button-4>",
                               lambda e: self._canvas.yview_scroll(-1, "units"))
        self._canvas.bind_all("<Button-5>",
                               lambda e: self._canvas.yview_scroll(1, "units"))

    def _on_inner_configure(self, _event):
        self._canvas.configure(scrollregion=self._canvas.bbox("all"))

    def _on_canvas_configure(self, event):
        self._canvas.itemconfig(self._inner_id, width=event.width)

    # ── row widgets ───────────────────────────────────────────────────────────

    def _build_rows(self):
        for widget in self._inner.winfo_children():
            widget.destroy()

        self._row_widgets = []   # list of dicts per row

        for row in self.rows:
            rw = self._build_one_row(row)
            self._row_widgets.append(rw)

            # divider
            tk.Frame(self._inner, bg=ACCENT_COLOR, height=1).pack(
                fill=tk.X, padx=0, pady=0)

    def _build_one_row(self, row: AnimRow) -> dict:
        container = tk.Frame(self._inner, bg=ROW_BG_COLOR, pady=ROW_PAD)
        container.pack(fill=tk.X, padx=0, pady=0)

        # ── left: label column ────────────────────────────────────────────────
        label_col = tk.Frame(container, bg=LABEL_BG, width=120)
        label_col.pack(side=tk.LEFT, fill=tk.Y)
        label_col.pack_propagate(False)

        tk.Label(label_col, text=row.name, bg=LABEL_BG, fg=LABEL_FG,
                 font=("Courier", 9, "bold"), wraplength=110,
                 justify=tk.LEFT).pack(padx=6, pady=(8, 2), anchor="w")

        tk.Label(label_col, text=f"{row.fw}×{row.fh}  {row.fps}fps",
                 bg=LABEL_BG, fg="#555555",
                 font=("Courier", 8)).pack(padx=6, anchor="w")

        # ── center: live preview ──────────────────────────────────────────────
        live_frame = tk.Frame(container, bg=ROW_BG_COLOR, padx=8)
        live_frame.pack(side=tk.LEFT, fill=tk.Y)

        tk.Label(live_frame, text="LIVE", bg=ROW_BG_COLOR, fg="#444444",
                 font=("Courier", 7)).pack()

        live_border = tk.Frame(live_frame,
                               bg=LIVE_BORDER,
                               width=row.fw * row.zoom + 2,
                               height=row.fh * row.zoom + 2)
        live_border.pack()
        live_border.pack_propagate(False)

        live_label = tk.Label(live_border, bg="#000000",
                              width=row.fw * row.zoom,
                              height=row.fh * row.zoom)
        live_label.place(x=1, y=1)

        # ── right: sheet strip ────────────────────────────────────────────────
        sheet_outer = tk.Frame(container, bg=ROW_BG_COLOR)
        sheet_outer.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(8, 8))

        tk.Label(sheet_outer, text="BAKED SHEET", bg=ROW_BG_COLOR, fg="#444444",
                 font=("Courier", 7)).pack(anchor="w")

        sheet_strip = tk.Frame(sheet_outer, bg="#000000")
        sheet_strip.pack(anchor="w")

        frame_labels = []
        for photo in row._sheet_photos:
            lbl = tk.Label(sheet_strip, image=photo, bg="#000000",
                           padx=SHEET_PAD // 2, pady=0)
            lbl.pack(side=tk.LEFT)
            frame_labels.append(lbl)

        return {
            "row":          row,
            "live_label":   live_label,
            "live_border":  live_border,
            "frame_labels": frame_labels,
            "sheet_strip":  sheet_strip,
        }

    def _show_empty(self):
        tk.Label(self._inner,
                 text="No animation modules loaded.\n\nDrop anim_*.py files or pass paths as arguments.",
                 bg=BG_COLOR, fg="#555555",
                 font=("Courier", 12), justify=tk.CENTER).pack(expand=True, pady=60)

    # ── key bindings ──────────────────────────────────────────────────────────

    def _bind_keys(self):
        self.root.bind("<space>",  lambda _: self._toggle_pause())
        self.root.bind("r",        lambda _: self._reset_time())
        self.root.bind("R",        lambda _: self._reset_time())
        self.root.bind("=",        lambda _: self._zoom_in())
        self.root.bind("+",        lambda _: self._zoom_in())
        self.root.bind("-",        lambda _: self._zoom_out())
        self.root.bind("s",        lambda _: self._export_all())
        self.root.bind("S",        lambda _: self._export_all())
        self.root.bind("q",        lambda _: self.root.destroy())
        self.root.bind("<Escape>", lambda _: self.root.destroy())

    # ── controls ──────────────────────────────────────────────────────────────

    def _current_t(self) -> float:
        if self.paused:
            return self.t_paused
        return time.perf_counter() - self.t_start - self.t_offset

    def _toggle_pause(self):
        if self.paused:
            # resuming: adjust offset so time continues from where it paused
            self.t_offset += time.perf_counter() - self.t_start - self.t_paused
            self.paused = False
            self._btn_pause.config(text="⏸ PAUSE")
        else:
            self.t_paused = self._current_t()
            self.paused = True
            self._btn_pause.config(text="▶ PLAY")

    def _reset_time(self):
        self.t_start  = time.perf_counter()
        self.t_offset = 0.0
        self.t_paused = 0.0

    def _zoom_in(self):
        self._set_zoom(min(self.zoom + 1, MAX_ZOOM))

    def _zoom_out(self):
        self._set_zoom(max(self.zoom - 1, MIN_ZOOM))

    def _set_zoom(self, zoom: int):
        if zoom == self.zoom:
            return
        self.zoom = zoom
        self._zoom_label.config(text=f"  {zoom}x")
        for row in self.rows:
            row.set_zoom(zoom)
        # rebuild to resize the live border + re-bind sheet photos
        self._build_rows()

    def _export_all(self):
        if not self.rows:
            return

        dest = filedialog.asksaveasfilename(
            parent=self.root,
            title="Export spritesheet",
            defaultextension=".png",
            filetypes=[("PNG image", "*.png"), ("All files", "*.*")],
            initialfile="spritesheet.png",
        )
        if not dest:
            return

        # build one row strip per animation then stack them vertically
        strips   = [row.get_sheet_image() for row in self.rows]
        total_w  = max(s.width  for s in strips)
        total_h  = sum(s.height for s in strips)
        combined = Image.new("RGBA", (total_w, total_h), (0, 0, 0, 0))
        y = 0
        for strip in strips:
            combined.paste(strip, (0, y), strip)
            y += strip.height

        combined.save(dest)
        msg = f"Exported {len(strips)} row(s) → {dest}"
        print(msg)
        self._status.config(text=msg)
        self.root.after(4000, lambda: self._status.config(text=""))

    def _load_files(self):
        """Open a file dialog and append selected animation modules as new rows."""
        paths = filedialog.askopenfilenames(
            parent=self.root,
            title="Load animation module(s)",
            filetypes=[("Python files", "*.py"), ("All files", "*.*")],
        )
        if not paths:
            return

        loaded = 0
        errors = []
        # track already-loaded paths so we skip exact duplicates
        existing = {row.path for row in self.rows}

        for p in paths:
            p = os.path.abspath(p)
            if p in existing:
                errors.append(f"{os.path.basename(p)}: already loaded")
                continue
            try:
                mod = load_anim_module(p)
                self.rows.append(AnimRow(mod, p, self.zoom))
                existing.add(p)
                loaded += 1
            except Exception as e:
                errors.append(f"{os.path.basename(p)}: {e}")

        if errors:
            messagebox.showwarning(
                "Load warnings",
                "\n".join(errors),
                parent=self.root,
            )

        if loaded:
            # clear any empty-state placeholder widgets before first real build
            for w in self._inner.winfo_children():
                w.destroy()
            self._build_rows()
            msg = f"Loaded {loaded} animation(s)"
            self._status.config(text=msg)
            self.root.after(3000, lambda: self._status.config(text=""))
            # scroll to the bottom so new rows are visible
            self.root.after(50, lambda: self._canvas.yview_moveto(1.0))

    # ── animation tick ────────────────────────────────────────────────────────

    def _tick(self):
        t = self._current_t()

        for rw in self._row_widgets:
            row = rw["row"]

            # live preview
            photo = row.render_live(t)
            rw["live_label"].config(image=photo)
            rw["live_label"]._photo_ref = photo   # prevent GC

            # highlight current sheet frame
            fi = int(t * row.fps) % SHEET_FRAMES
            for i, lbl in enumerate(rw["frame_labels"]):
                lbl.config(bg="#1a1a1a" if i == fi else "#000000")

        # update status bar fps estimate (rough)
        self._status.config(text=f"t={t:.2f}s  zoom={self.zoom}x"
                            + ("  [PAUSED]" if self.paused else ""))

        self.root.after(TICK_MS, self._tick)


# ── entry point ───────────────────────────────────────────────────────────────

def discover_anims(cwd: str) -> list[str]:
    """Find anim_*.py files in cwd, sorted."""
    return sorted(str(p) for p in Path(cwd).glob("anim_*.py"))


def main():
    args = sys.argv[1:]

    if args:
        paths = [os.path.abspath(a) for a in args]
    else:
        cwd   = os.path.dirname(os.path.abspath(__file__))
        paths = discover_anims(cwd)

    root = tk.Tk()
    app  = SpriteViewer(root, paths)
    root.mainloop()


if __name__ == "__main__":
    main()