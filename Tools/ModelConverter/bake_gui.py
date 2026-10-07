"""
bake_gui.py - Tkinter front end for bake_core.

    pip install numpy
    python bake_gui.py

Add Mixamo .fbx (or .glb) files, pick an output folder, click Convert.
"""
import json
import queue
import threading
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, scrolledtext, ttk

import bake_core as core

CFG_PATH = Path.home() / ".mixamo_baker.json"


def load_cfg():
    try:
        return json.loads(CFG_PATH.read_text(encoding="utf-8"))
    except Exception:
        return {}


def save_cfg(cfg):
    try:
        CFG_PATH.write_text(json.dumps(cfg, indent=2), encoding="utf-8")
    except Exception:
        pass


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Mixamo Baker  (.fbx -> .skel / .mesh / .anim)")
        self.geometry("820x680")
        self.minsize(640, 520)

        cfg = load_cfg()
        self.q = queue.Queue()
        self.worker = None
        self.files = []

        self.blender_var = tk.StringVar(value=cfg.get("blender") or core.find_blender() or "")
        self.out_var = tk.StringVar(value=cfg.get("out", ""))
        self.skel_var = tk.BooleanVar(value=cfg.get("skel", True))
        self.mesh_var = tk.BooleanVar(value=cfg.get("mesh", True))
        self.anim_var = tk.BooleanVar(value=cfg.get("anim", True))
        self.keep_var = tk.BooleanVar(value=cfg.get("keep_glb", False))
        self.fps_var = tk.StringVar(value=str(cfg.get("fps", 30)))

        self._build()
        self.after(100, self._poll)

    # ------------------------------------------------------------------ layout
    def _build(self):
        pad = {"padx": 8, "pady": 4}
        root = ttk.Frame(self, padding=8)
        root.pack(fill="both", expand=True)
        root.columnconfigure(1, weight=1)

        ttk.Label(root, text="Blender").grid(row=0, column=0, sticky="w", **pad)
        ttk.Entry(root, textvariable=self.blender_var).grid(row=0, column=1, sticky="ew", **pad)
        ttk.Button(root, text="Browse...", command=self._pick_blender).grid(row=0, column=2, **pad)

        ttk.Label(root, text="Output folder").grid(row=1, column=0, sticky="w", **pad)
        ttk.Entry(root, textvariable=self.out_var).grid(row=1, column=1, sticky="ew", **pad)
        ttk.Button(root, text="Browse...", command=self._pick_out).grid(row=1, column=2, **pad)

        box = ttk.LabelFrame(root, text="Input files (.fbx / .glb)")
        box.grid(row=2, column=0, columnspan=3, sticky="nsew", **pad)
        box.columnconfigure(0, weight=1)
        box.rowconfigure(0, weight=1)
        root.rowconfigure(2, weight=1)

        self.listbox = tk.Listbox(box, selectmode="extended", height=8, activestyle="none")
        self.listbox.grid(row=0, column=0, sticky="nsew", padx=(6, 0), pady=6)
        sb = ttk.Scrollbar(box, orient="vertical", command=self.listbox.yview)
        sb.grid(row=0, column=1, sticky="ns", pady=6)
        self.listbox.configure(yscrollcommand=sb.set)

        btns = ttk.Frame(box)
        btns.grid(row=0, column=2, sticky="n", padx=6, pady=6)
        for text, cmd in (("Add files...", self._add_files), ("Add folder...", self._add_folder),
                          ("Remove selected", self._remove), ("Clear", self._clear)):
            ttk.Button(btns, text=text, command=cmd).pack(fill="x", pady=2)

        opt = ttk.LabelFrame(root, text="Options")
        opt.grid(row=3, column=0, columnspan=3, sticky="ew", **pad)
        ttk.Checkbutton(opt, text="Skeleton (.skel)", variable=self.skel_var).pack(side="left", padx=8, pady=6)
        ttk.Checkbutton(opt, text="Mesh (.mesh)", variable=self.mesh_var).pack(side="left", padx=8)
        ttk.Checkbutton(opt, text="Animation (.anim)", variable=self.anim_var).pack(side="left", padx=8)
        ttk.Label(opt, text="Anim FPS").pack(side="left", padx=(16, 2))
        ttk.Spinbox(opt, from_=1, to=240, width=5, textvariable=self.fps_var).pack(side="left")
        ttk.Checkbutton(opt, text="Keep intermediate .glb", variable=self.keep_var).pack(side="left", padx=16)

        run = ttk.Frame(root)
        run.grid(row=4, column=0, columnspan=3, sticky="ew", **pad)
        run.columnconfigure(1, weight=1)
        self.go = ttk.Button(run, text="Convert", command=self._start)
        self.go.grid(row=0, column=0, padx=(0, 8))
        self.bar = ttk.Progressbar(run, maximum=1.0)
        self.bar.grid(row=0, column=1, sticky="ew")

        self.log = scrolledtext.ScrolledText(root, height=12, state="disabled", font=("Consolas", 9))
        self.log.grid(row=5, column=0, columnspan=3, sticky="nsew", **pad)
        root.rowconfigure(5, weight=1)

    # ------------------------------------------------------------------ actions
    def _pick_blender(self):
        p = filedialog.askopenfilename(title="Select the Blender executable")
        if p:
            self.blender_var.set(p)

    def _pick_out(self):
        p = filedialog.askdirectory(title="Select output folder")
        if p:
            self.out_var.set(p)

    def _add_paths(self, paths):
        for p in paths:
            p = str(Path(p))
            if p not in self.files and Path(p).suffix.lower() in (".fbx", ".glb"):
                self.files.append(p)
                self.listbox.insert("end", p)

    def _add_files(self):
        self._add_paths(filedialog.askopenfilenames(
            title="Select Mixamo files", filetypes=[("Mixamo / GLB", "*.fbx *.glb"), ("All files", "*.*")]))

    def _add_folder(self):
        d = filedialog.askdirectory(title="Add every .fbx/.glb in a folder")
        if d:
            found = sorted(list(Path(d).glob("*.fbx")) + list(Path(d).glob("*.glb")))
            self._add_paths(found)

    def _remove(self):
        for i in reversed(self.listbox.curselection()):
            self.listbox.delete(i)
            del self.files[i]

    def _clear(self):
        self.listbox.delete(0, "end")
        self.files.clear()

    def _write(self, text):
        self.log.configure(state="normal")
        self.log.insert("end", text + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def _start(self):
        if self.worker and self.worker.is_alive():
            return
        if not self.files:
            messagebox.showwarning("Nothing to do", "Add at least one .fbx or .glb file.")
            return
        out = self.out_var.get().strip()
        if not out:
            messagebox.showwarning("Output folder", "Choose an output folder.")
            return
        blender = self.blender_var.get().strip()
        needs_blender = any(f.lower().endswith(".fbx") for f in self.files)
        if needs_blender and not (blender and Path(blender).exists()):
            messagebox.showwarning("Blender", "Set the path to the Blender executable (needed for .fbx input).")
            return
        try:
            fps = float(self.fps_var.get())
            if fps <= 0:
                raise ValueError
        except ValueError:
            messagebox.showwarning("FPS", "Anim FPS must be a positive number.")
            return
        if not (self.skel_var.get() or self.mesh_var.get() or self.anim_var.get()):
            messagebox.showwarning("Options", "Enable at least one of skeleton / mesh / animation.")
            return

        opts = core.Options(self.skel_var.get(), self.mesh_var.get(), self.anim_var.get(), fps, self.keep_var.get())
        save_cfg({"blender": blender, "out": out, "skel": opts.write_skel, "mesh": opts.write_mesh,
                  "anim": opts.write_anim, "fps": fps, "keep_glb": opts.keep_glb})

        self.bar["value"] = 0
        self.go.configure(state="disabled")
        files = list(self.files)

        def work():
            try:
                core.bake_batch(files, out, blender, opts,
                                log=lambda m: self.q.put(("log", m)),
                                progress=lambda f: self.q.put(("progress", f)))
            except Exception as e:
                self.q.put(("log", f"ERROR: {e}"))
            finally:
                self.q.put(("done", None))

        self.worker = threading.Thread(target=work, daemon=True)
        self.worker.start()

    def _poll(self):
        try:
            while True:
                kind, val = self.q.get_nowait()
                if kind == "log":
                    self._write(val)
                elif kind == "progress":
                    self.bar["value"] = val
                elif kind == "done":
                    self.bar["value"] = 1.0
                    self.go.configure(state="normal")
        except queue.Empty:
            pass
        self.after(100, self._poll)


if __name__ == "__main__":
    App().mainloop()
