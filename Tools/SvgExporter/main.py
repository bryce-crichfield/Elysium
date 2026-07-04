"""Elysium SVG Exporter — Tkinter GUI.

Parses an SVG file, previews the parsed shapes on a Canvas (using the same
world-transform composition TransformSystem::ComposeRecursive uses, so the
preview matches what the engine will actually show), and exports an Elysium
prefab XML + any embedded images.
"""
from __future__ import annotations

import io
import math
import os
import re
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from typing import Optional, Tuple

import cli
from svg_model import SvgNode
from svg_parser import SvgParser

WorldTransform = Tuple[float, float, float, float, float]  # x, y, rotationDeg, scaleX, scaleY


class App:
    def __init__(self, root: tk.Tk):
        self.root = root
        root.title("Elysium SVG Exporter")
        root.geometry("1150x720")

        self.svg_path: Optional[str] = None
        self.parsed_root: Optional[SvgNode] = None
        self.parse_warnings: list[str] = []
        self._preview_images: list = []  # keep PhotoImage refs alive against GC

        self._build_ui()

    # ---- UI construction -------------------------------------------------

    def _build_ui(self) -> None:
        top = ttk.Frame(self.root)
        top.pack(side=tk.TOP, fill=tk.X, padx=6, pady=6)

        ttk.Button(top, text="Open SVG...", command=self._on_open).pack(side=tk.LEFT)

        ttk.Label(top, text="Curve Quality:").pack(side=tk.LEFT, padx=(12, 2))
        self.curve_quality = tk.IntVar(value=16)
        ttk.Spinbox(
            top, from_=4, to=64, textvariable=self.curve_quality, width=5, command=self._on_reparse,
        ).pack(side=tk.LEFT)

        ttk.Label(top, text="Root Layer:").pack(side=tk.LEFT, padx=(12, 2))
        self.layer_name = tk.StringVar(value="default")
        ttk.Entry(top, textvariable=self.layer_name, width=12).pack(side=tk.LEFT)

        self.export_btn = ttk.Button(top, text="Export...", command=self._on_export, state=tk.DISABLED)
        self.export_btn.pack(side=tk.LEFT, padx=(12, 2))

        self.status_label = ttk.Label(top, text="No file loaded")
        self.status_label.pack(side=tk.LEFT, padx=(12, 2))

        body = ttk.Panedwindow(self.root, orient=tk.HORIZONTAL)
        body.pack(fill=tk.BOTH, expand=True, padx=6, pady=(0, 6))

        canvas_frame = ttk.Frame(body)
        self.canvas = tk.Canvas(canvas_frame, background="#dddddd")
        hbar = ttk.Scrollbar(canvas_frame, orient=tk.HORIZONTAL, command=self.canvas.xview)
        vbar = ttk.Scrollbar(canvas_frame, orient=tk.VERTICAL, command=self.canvas.yview)
        self.canvas.configure(xscrollcommand=hbar.set, yscrollcommand=vbar.set)
        self.canvas.grid(row=0, column=0, sticky="nsew")
        vbar.grid(row=0, column=1, sticky="ns")
        hbar.grid(row=1, column=0, sticky="ew")
        canvas_frame.rowconfigure(0, weight=1)
        canvas_frame.columnconfigure(0, weight=1)
        body.add(canvas_frame, weight=3)

        log_frame = ttk.Frame(body)
        ttk.Label(log_frame, text="Log").pack(anchor=tk.W)
        self.log_text = tk.Text(log_frame, width=50, wrap=tk.WORD, state=tk.DISABLED)
        log_scroll = ttk.Scrollbar(log_frame, orient=tk.VERTICAL, command=self.log_text.yview)
        self.log_text.configure(yscrollcommand=log_scroll.set)
        self.log_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        log_scroll.pack(side=tk.LEFT, fill=tk.Y)
        body.add(log_frame, weight=2)

    def _log(self, msg: str) -> None:
        self.log_text.configure(state=tk.NORMAL)
        self.log_text.insert(tk.END, msg + "\n")
        self.log_text.configure(state=tk.DISABLED)
        self.log_text.see(tk.END)

    def _clear_log(self) -> None:
        self.log_text.configure(state=tk.NORMAL)
        self.log_text.delete("1.0", tk.END)
        self.log_text.configure(state=tk.DISABLED)

    # ---- parsing -----------------------------------------------------------

    def _on_open(self) -> None:
        path = filedialog.askopenfilename(
            title="Open SVG", filetypes=[("SVG files", "*.svg"), ("All files", "*.*")],
        )
        if not path:
            return
        self.svg_path = path
        self._reparse()

    def _on_reparse(self) -> None:
        if self.svg_path:
            self._reparse()

    def _reparse(self) -> None:
        self._clear_log()
        self._log(f"Parsing: {self.svg_path}")
        try:
            parser = SvgParser(curve_segments=int(self.curve_quality.get()))
            self.parsed_root = parser.parse_file(self.svg_path)
            self.parse_warnings = parser.warnings
        except Exception as e:  # noqa: BLE001 - report any parse failure to the user, don't crash the GUI
            messagebox.showerror("Parse error", str(e))
            self._log(f"ERROR: {e}")
            self.export_btn.configure(state=tk.DISABLED)
            return

        for w in self.parse_warnings:
            self._log(f"WARNING: {w}")
        self._log(f"Parsed OK. {len(self.parse_warnings)} warning(s).")
        self.status_label.configure(text=os.path.basename(self.svg_path))
        self.export_btn.configure(state=tk.NORMAL)
        self._render_preview()

    # ---- preview -----------------------------------------------------------

    @staticmethod
    def _compose(parent_world: WorldTransform, local_x: float, local_y: float, local_rot: float, local_sx: float, local_sy: float) -> WorldTransform:
        pwx, pwy, pwrot, pwsx, pwsy = parent_world
        rad = math.radians(pwrot)
        sx, sy = local_x * pwsx, local_y * pwsy
        cs, sn = math.cos(rad), math.sin(rad)
        wx = pwx + (sx * cs - sy * sn)
        wy = pwy + (sx * sn + sy * cs)
        return (wx, wy, pwrot + local_rot, pwsx * local_sx, pwsy * local_sy)

    @staticmethod
    def _tk_color(hex8: str) -> str:
        if len(hex8) == 9 and hex8[7:9] == "00":
            return ""
        return hex8[:7] if len(hex8) >= 7 else "#000000"

    def _render_preview(self) -> None:
        self.canvas.delete("all")
        self._preview_images.clear()
        if self.parsed_root is None:
            return
        self._draw_node(self.parsed_root, (0.0, 0.0, 0.0, 1.0, 1.0))
        bbox = self.canvas.bbox("all")
        if bbox:
            pad = 20
            self.canvas.configure(scrollregion=(bbox[0] - pad, bbox[1] - pad, bbox[2] + pad, bbox[3] + pad))

    def _draw_node(self, node: SvgNode, parent_world: WorldTransform) -> None:
        g = node.geometry
        local_x, local_y = g.get("x", 0.0), g.get("y", 0.0)
        local_rot = g.get("rotation", 0.0)
        local_sx, local_sy = g.get("scaleX", 1.0), g.get("scaleY", 1.0)
        wx, wy, wrot, wsx, wsy = self._compose(parent_world, local_x, local_y, local_rot, local_sx, local_sy)

        if node.kind in ("rect", "circle", "ellipse", "polygon"):
            fill = self._tk_color(node.style.fill_hex())
            outline = self._tk_color(node.style.stroke_hex())
        else:
            fill = outline = ""

        if node.kind == "rect":
            w, h = g["width"], g["height"]
            self.canvas.create_rectangle(
                wx - w / 2, wy - h / 2, wx + w / 2, wy + h / 2,
                fill=fill, outline=outline, width=max(1, int(g.get("strokeWidth", 1))),
            )
        elif node.kind == "circle":
            r = g["radius"]
            self.canvas.create_oval(wx - r, wy - r, wx + r, wy + r, fill=fill, outline=outline)
        elif node.kind == "ellipse":
            rh, rv = g["radiusH"], g["radiusV"]
            self.canvas.create_oval(wx - rh, wy - rv, wx + rh, wy + rv, fill=fill, outline=outline)
        elif node.kind == "line":
            color = self._tk_color(node.style.stroke_hex()) or "#000000"
            self.canvas.create_line(
                wx + g["x1"], wy + g["y1"], wx + g["x2"], wy + g["y2"],
                fill=color, width=max(1, int(g.get("thickness", 1))),
            )
        elif node.kind == "polygon":
            pts = []
            for px, py in g["points"]:
                pts.extend([wx + px, wy + py])
            self.canvas.create_polygon(*pts, fill=fill, outline=outline, width=2)
        elif node.kind == "image":
            self._draw_image_preview(node, wx, wy, wsx, wsy)

        for child in node.children:
            self._draw_node(child, (wx, wy, wrot, wsx, wsy))

    def _draw_image_preview(self, node: SvgNode, wx: float, wy: float, wsx: float, wsy: float) -> None:
        g = node.geometry
        w = max(1, int(abs(g["displayWidth"] * wsx)))
        h = max(1, int(abs(g["displayHeight"] * wsy)))
        try:
            from PIL import Image, ImageTk
            img = Image.open(io.BytesIO(g["image_bytes"])).convert("RGBA").resize((w, h))
            photo = ImageTk.PhotoImage(img)
            self._preview_images.append(photo)
            self.canvas.create_image(wx, wy, image=photo)
        except Exception:  # noqa: BLE001 - preview-only fallback, export path re-decodes independently
            self.canvas.create_rectangle(wx - w / 2, wy - h / 2, wx + w / 2, wy + h / 2, fill="#cccccc", outline="#888888")
            self.canvas.create_text(wx, wy, text="IMG")

    # ---- export -----------------------------------------------------------

    def _on_export(self) -> None:
        if self.parsed_root is None or self.svg_path is None:
            return

        default_name = re.sub(r"[^A-Za-z0-9_]", "_", os.path.splitext(os.path.basename(self.svg_path))[0])
        tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # .../Tools
        repo_root = os.path.dirname(tools_dir)
        default_dir = os.path.join(repo_root, "Assets", "Scenes", "Prefabs")
        if not os.path.isdir(default_dir):
            default_dir = os.path.dirname(self.svg_path)

        out_path = filedialog.asksaveasfilename(
            title="Export Prefab As",
            initialdir=default_dir,
            initialfile=f"{default_name}.xml",
            defaultextension=".xml",
            filetypes=[("Elysium prefab XML", "*.xml")],
        )
        if not out_path:
            return

        prefab_name = re.sub(r"[^A-Za-z0-9_]", "_", os.path.splitext(os.path.basename(out_path))[0])
        assets_root = self._find_assets_root(out_path) or os.path.join(repo_root, "Assets")

        try:
            warnings, images = cli.export(
                self.svg_path, assets_root, prefab_name=prefab_name,
                layer_name=self.layer_name.get() or "default",
                curve_segments=int(self.curve_quality.get()),
            )
        except Exception as e:  # noqa: BLE001
            messagebox.showerror("Export failed", str(e))
            self._log(f"EXPORT ERROR: {e}")
            return

        self._log(f"Exported prefab: {out_path}")
        for img_path in images:
            self._log(f"Wrote image: {img_path}")
        for w in warnings:
            self._log(f"WARNING: {w}")
        messagebox.showinfo("Export complete", f"Prefab written to:\n{out_path}\n\n{len(images)} image(s) written.")

    @staticmethod
    def _find_assets_root(out_path: str) -> Optional[str]:
        d = os.path.dirname(os.path.abspath(out_path))
        for _ in range(10):
            if os.path.basename(d) == "Assets":
                return d
            parent = os.path.dirname(d)
            if parent == d:
                break
            d = parent
        return None


def main() -> None:
    root = tk.Tk()
    App(root)
    root.mainloop()


if __name__ == "__main__":
    main()
