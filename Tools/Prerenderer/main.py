"""
Spritesheet Renderer GUI (v3 -- split into app / blender script / stitch module)
----------------------------------------------------------------------------------
Lets you pick ONE character FBX (mesh + armature) and ONE OR MORE animation
FBX files (each containing an action to steal). For each animation, this app:

    1. Writes a small JSON config describing the render job.
    2. Runs Blender headless against blender_render_script.py, which imports
       the character, cross-assigns the animation action, and renders
       top-down/isometric frames to disk (one folder per camera direction).
    3. Calls stitch.stitch_spritesheet() (from stitch.py) to pack those
       frames into a single spritesheet PNG.

This file only orchestrates -- it contains no bpy code and no Pillow
image-manipulation code; those live in blender_render_script.py and
stitch.py respectively, in the same folder as this file.

Requires:
    - Blender installed somewhere on disk (you point to blender.exe in the GUI)
    - Pillow installed in THIS Python environment (the one running this GUI)
        pip install pillow

Usage:
    python app.py
"""

import json
import os
import queue
import re
import subprocess
import threading
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

from stitch import stitch_spritesheet, find_animation_sets, sanitize_name, PILLOW_AVAILABLE

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG_FILE = os.path.join(SCRIPT_DIR, "spritesheet_gui_config.json")
BLENDER_SCRIPT_PATH = os.path.join(SCRIPT_DIR, "blender_render_script.py")
TEMP_JOB_CONFIG_PATH = os.path.join(SCRIPT_DIR, "_blender_job_config_tmp.json")


CAMERA_FOLLOW_MODE_LABELS = {
    "Follow XY, lock height (recommended)": "xy",
    "Static (no recenter)": "static",
    "Follow XYZ (legacy, cancels vertical motion)": "xyz",
}


def build_job_config(character_path, animation_path, output_dir, sprite_size, num_directions,
                      camera_height_angle, frame_step, render_engine, camera_follow_mode):
    """Assemble the JSON config that blender_render_script.py reads for one render job."""
    return {
        "character_path": character_path,
        "animation_path": animation_path,
        "output_dir": output_dir,
        "sprite_size": int(sprite_size),
        "num_directions": int(num_directions),
        "camera_height_angle": float(camera_height_angle),
        "frame_step": int(frame_step),
        "render_engine": render_engine or "",
        "camera_follow_mode": CAMERA_FOLLOW_MODE_LABELS.get(camera_follow_mode, "xy"),
    }


class SpritesheetApp:
    def __init__(self, root):
        self.root = root
        root.title("FBX \u2192 Spritesheet Renderer (Model + Animations)")
        root.geometry("780x720")
        root.minsize(720, 620)

        self.msg_queue = queue.Queue()
        self.worker_thread = None
        self.cancel_requested = threading.Event()
        self._current_proc = None

        self.vars = {
            "blender_exe": tk.StringVar(),
            "character_path": tk.StringVar(),
            "render_dir": tk.StringVar(),
            "sheet_dir": tk.StringVar(),
            "sprite_size": tk.StringVar(value="256"),
            "num_directions": tk.StringVar(value="8"),
            "camera_height_angle": tk.StringVar(value="55"),
            "frame_step": tk.StringVar(value="1"),
            "render_engine": tk.StringVar(value="Auto"),
            "camera_follow_mode": tk.StringVar(value="Follow XY, lock height (recommended)"),
        }
        self.animation_paths = []  # list of full paths, shown in the listbox

        self._load_config()
        self._build_ui()
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)
        self.root.after(100, self._poll_queue)

    # ── UI construction ────────────────────────────────────────────────
    def _build_ui(self):
        pad = {"padx": 8, "pady": 4}

        outer = ttk.Frame(self.root)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(0, weight=1)

        # --- Paths section ---
        paths_frame = ttk.Frame(outer)
        paths_frame.grid(row=0, column=0, sticky="ew")
        paths_frame.columnconfigure(1, weight=1)

        row = 0
        ttk.Label(paths_frame, text="Blender & Character Setup", font=("", 10, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", pady=(0, 4))
        row += 1

        def add_path_row(parent, r, label, var, is_dir=False, filetypes=None):
            ttk.Label(parent, text=label).grid(row=r, column=0, sticky="w", **pad)
            ttk.Entry(parent, textvariable=var).grid(row=r, column=1, sticky="ew", **pad)

            def browse():
                path = filedialog.askdirectory() if is_dir else filedialog.askopenfilename(
                    filetypes=filetypes or [("All files", "*.*")])
                if path:
                    var.set(path)

            ttk.Button(parent, text="Browse...", command=browse).grid(row=r, column=2, **pad)

        add_path_row(paths_frame, row, "Blender executable:", self.vars["blender_exe"],
                     filetypes=[("blender.exe", "blender.exe"), ("All files", "*.*")])
        row += 1
        add_path_row(paths_frame, row, "Character FBX (mesh + rig):", self.vars["character_path"],
                     filetypes=[("FBX files", "*.fbx"), ("All files", "*.*")])
        row += 1
        add_path_row(paths_frame, row, "Render output root folder:", self.vars["render_dir"], is_dir=True)
        row += 1
        add_path_row(paths_frame, row, "Spritesheet output folder:", self.vars["sheet_dir"], is_dir=True)
        row += 1

        # --- Animation list section ---
        ttk.Separator(outer, orient="horizontal").grid(row=1, column=0, sticky="ew", pady=8)

        anim_frame = ttk.Frame(outer)
        anim_frame.grid(row=2, column=0, sticky="ew")
        anim_frame.columnconfigure(0, weight=1)

        ttk.Label(anim_frame, text="Animation Source FBX Files", font=("", 10, "bold")).grid(
            row=0, column=0, sticky="w")
        ttk.Label(anim_frame,
                  text="Each file's action is applied to the character above and rendered/stitched separately.\n"
                       "Add the character FBX itself here too if you want to render its own embedded animation.",
                  foreground="gray", justify="left").grid(row=1, column=0, sticky="w", pady=(0, 4))

        list_container = ttk.Frame(anim_frame)
        list_container.grid(row=2, column=0, sticky="ew")
        list_container.columnconfigure(0, weight=1)

        self.anim_listbox = tk.Listbox(list_container, height=6, selectmode="extended")
        anim_scroll = ttk.Scrollbar(list_container, command=self.anim_listbox.yview)
        self.anim_listbox.configure(yscrollcommand=anim_scroll.set)
        self.anim_listbox.grid(row=0, column=0, sticky="ew")
        anim_scroll.grid(row=0, column=1, sticky="ns")

        anim_btns = ttk.Frame(anim_frame)
        anim_btns.grid(row=3, column=0, sticky="w", pady=4)
        ttk.Button(anim_btns, text="Add Animation FBX(s)...", command=self._add_animations).pack(side="left", padx=2)
        ttk.Button(anim_btns, text="Add Character's Own Animation", command=self._add_character_as_anim).pack(
            side="left", padx=2)
        ttk.Button(anim_btns, text="Remove Selected", command=self._remove_selected_animations).pack(
            side="left", padx=2)
        ttk.Button(anim_btns, text="Clear All", command=self._clear_animations).pack(side="left", padx=2)

        # --- Render parameters ---
        ttk.Separator(outer, orient="horizontal").grid(row=3, column=0, sticky="ew", pady=8)

        params_frame = ttk.Frame(outer)
        params_frame.grid(row=4, column=0, sticky="ew")

        ttk.Label(params_frame, text="Render Parameters", font=("", 10, "bold")).grid(
            row=0, column=0, columnspan=3, sticky="w", pady=(0, 4))

        def add_simple_row(r, label, var, widget="entry", options=None, tooltip=None):
            ttk.Label(params_frame, text=label).grid(row=r, column=0, sticky="w", **pad)
            if widget == "entry":
                ttk.Entry(params_frame, textvariable=var, width=15).grid(row=r, column=1, sticky="w", **pad)
            elif widget == "combo":
                ttk.Combobox(params_frame, textvariable=var, values=options, width=13,
                             state="readonly").grid(row=r, column=1, sticky="w", **pad)
            if tooltip:
                ttk.Label(params_frame, text=tooltip, foreground="gray").grid(row=r, column=2, sticky="w", **pad)

        add_simple_row(1, "Sprite size (px):", self.vars["sprite_size"],
                       tooltip="Square render resolution, e.g. 256")
        add_simple_row(2, "Number of directions:", self.vars["num_directions"], widget="combo",
                        options=["1", "4", "8"], tooltip="1=single facing, 4=N/E/S/W, 8=full compass")
        add_simple_row(3, "Camera height angle (deg):", self.vars["camera_height_angle"],
                       tooltip="90 = straight top-down, ~45-60 = isometric-ish")
        add_simple_row(4, "Frame step:", self.vars["frame_step"],
                       tooltip="Render every Nth frame (1 = every frame)")
        add_simple_row(5, "Render engine:", self.vars["render_engine"], widget="combo",
                        options=["Auto", "BLENDER_EEVEE", "BLENDER_EEVEE_NEXT", "CYCLES"],
                        tooltip="Auto picks the best Eevee variant available")
        add_simple_row(6, "Camera follow mode:", self.vars["camera_follow_mode"], widget="combo",
                        options=list(CAMERA_FOLLOW_MODE_LABELS.keys()),
                        tooltip="XY-locked height keeps vertical bob/jump visible")

        # --- Buttons ---
        ttk.Separator(outer, orient="horizontal").grid(row=5, column=0, sticky="ew", pady=8)

        btn_frame = ttk.Frame(outer)
        btn_frame.grid(row=6, column=0, sticky="ew")
        self.run_button = ttk.Button(btn_frame, text="Render + Stitch All Animations", command=self._start_run)
        self.run_button.pack(side="left", padx=4)
        self.stitch_only_button = ttk.Button(btn_frame, text="Stitch Only (skip Blender)",
                                              command=self._start_stitch_only)
        self.stitch_only_button.pack(side="left", padx=4)
        self.cancel_button = ttk.Button(btn_frame, text="Cancel", command=self._cancel, state="disabled")
        self.cancel_button.pack(side="left", padx=4)

        # --- Status / progress ---
        status_frame = ttk.Frame(outer)
        status_frame.grid(row=7, column=0, sticky="ew", pady=(8, 0))
        status_frame.columnconfigure(0, weight=1)

        self.overall_status_var = tk.StringVar(value="Idle.")
        ttk.Label(status_frame, textvariable=self.overall_status_var).grid(row=0, column=0, sticky="w")

        self.status_var = tk.StringVar(value="")
        ttk.Label(status_frame, textvariable=self.status_var).grid(row=1, column=0, sticky="w")

        self.progress = ttk.Progressbar(status_frame, orient="horizontal", mode="determinate")
        self.progress.grid(row=2, column=0, sticky="ew", pady=4)

        # --- Log box ---
        outer.rowconfigure(8, weight=1)
        log_label_frame = ttk.Frame(outer)
        log_label_frame.grid(row=8, column=0, sticky="nsew", pady=(4, 0))
        log_label_frame.columnconfigure(0, weight=1)
        log_label_frame.rowconfigure(1, weight=1)

        ttk.Label(log_label_frame, text="Log:").grid(row=0, column=0, sticky="w")
        log_container = ttk.Frame(log_label_frame)
        log_container.grid(row=1, column=0, sticky="nsew")
        log_container.columnconfigure(0, weight=1)
        log_container.rowconfigure(0, weight=1)

        self.log_text = tk.Text(log_container, height=12, wrap="word", state="disabled")
        scrollbar = ttk.Scrollbar(log_container, command=self.log_text.yview)
        self.log_text.configure(yscrollcommand=scrollbar.set)
        self.log_text.grid(row=0, column=0, sticky="nsew")
        scrollbar.grid(row=0, column=1, sticky="ns")

    # ── Animation list management ──────────────────────────────────────
    def _refresh_anim_listbox(self):
        self.anim_listbox.delete(0, "end")
        for path in self.animation_paths:
            self.anim_listbox.insert("end", path)

    def _add_animations(self):
        paths = filedialog.askopenfilenames(filetypes=[("FBX files", "*.fbx"), ("All files", "*.*")])
        for p in paths:
            if p not in self.animation_paths:
                self.animation_paths.append(p)
        self._refresh_anim_listbox()

    def _add_character_as_anim(self):
        char_path = self.vars["character_path"].get().strip()
        if not char_path:
            messagebox.showwarning("No character selected", "Set the Character FBX field first.")
            return
        if char_path not in self.animation_paths:
            self.animation_paths.append(char_path)
        self._refresh_anim_listbox()

    def _remove_selected_animations(self):
        selected = list(self.anim_listbox.curselection())
        for idx in reversed(selected):
            del self.animation_paths[idx]
        self._refresh_anim_listbox()

    def _clear_animations(self):
        self.animation_paths = []
        self._refresh_anim_listbox()

    # ── Config persistence ─────────────────────────────────────────────
    def _load_config(self):
        if os.path.exists(CONFIG_FILE):
            try:
                with open(CONFIG_FILE, "r") as f:
                    data = json.load(f)
                for key, val in data.get("vars", {}).items():
                    if key in self.vars:
                        self.vars[key].set(val)
                self.animation_paths = data.get("animation_paths", [])
            except Exception:
                pass

    def _save_config(self):
        try:
            data = {
                "vars": {key: var.get() for key, var in self.vars.items()},
                "animation_paths": self.animation_paths,
            }
            with open(CONFIG_FILE, "w") as f:
                json.dump(data, f, indent=2)
        except Exception:
            pass

    def _on_close(self):
        self._save_config()
        self.root.destroy()

    # ── Logging helpers (thread-safe via queue) ────────────────────────
    def _log(self, text):
        self.msg_queue.put(("log", text))

    def _set_overall_status(self, text):
        self.msg_queue.put(("overall_status", text))

    def _set_status(self, text):
        self.msg_queue.put(("status", text))

    def _poll_queue(self):
        try:
            while True:
                kind, payload = self.msg_queue.get_nowait()
                if kind == "log":
                    self.log_text.configure(state="normal")
                    self.log_text.insert("end", payload + "\n")
                    self.log_text.see("end")
                    self.log_text.configure(state="disabled")
                elif kind == "overall_status":
                    self.overall_status_var.set(payload)
                elif kind == "status":
                    self.status_var.set(payload)
                elif kind == "progress":
                    value, maximum = payload
                    if maximum is not None:
                        self.progress.configure(maximum=maximum, mode="determinate")
                    self.progress["value"] = value
                elif kind == "done":
                    self._on_worker_done(payload)
        except queue.Empty:
            pass
        self.root.after(100, self._poll_queue)

    def _on_worker_done(self, error):
        self.run_button.configure(state="normal")
        self.stitch_only_button.configure(state="normal")
        self.cancel_button.configure(state="disabled")
        self.progress.stop()
        if error:
            self._log(f"ERROR: {error}")
            self.overall_status_var.set("Failed.")
            messagebox.showerror("Error", str(error))
        else:
            self.overall_status_var.set("All done.")

    # ── Validation ──────────────────────────────────────────────────────
    def _validate_common(self, require_blender=True):
        v = {k: var.get().strip() for k, var in self.vars.items()}

        if require_blender:
            if not v["blender_exe"] or not os.path.isfile(v["blender_exe"]):
                raise ValueError("Please select a valid Blender executable.")
            if not v["character_path"] or not os.path.isfile(v["character_path"]):
                raise ValueError("Please select a valid character FBX file.")
            if not self.animation_paths:
                raise ValueError("Add at least one animation FBX (or the character's own animation).")

        if not v["render_dir"]:
            raise ValueError("Please choose a render output root folder.")
        if not v["sheet_dir"]:
            raise ValueError("Please choose a spritesheet output folder.")

        try:
            int(v["sprite_size"])
            int(v["num_directions"])
            float(v["camera_height_angle"])
            int(v["frame_step"])
        except ValueError:
            raise ValueError("Sprite size, directions, camera angle, and frame step must be numeric.")

        return v

    # ── Run actions ─────────────────────────────────────────────────────
    def _start_run(self):
        try:
            v = self._validate_common(require_blender=True)
        except ValueError as e:
            messagebox.showwarning("Missing/invalid input", str(e))
            return

        self._save_config()
        self._prep_run_ui()
        self.cancel_requested.clear()
        anim_paths_snapshot = list(self.animation_paths)
        self.worker_thread = threading.Thread(
            target=self._run_full_pipeline, args=(v, anim_paths_snapshot), daemon=True)
        self.worker_thread.start()

    def _start_stitch_only(self):
        try:
            v = self._validate_common(require_blender=False)
        except ValueError as e:
            messagebox.showwarning("Missing/invalid input", str(e))
            return

        self._save_config()
        self._prep_run_ui()
        self.cancel_requested.clear()
        self.worker_thread = threading.Thread(target=self._run_stitch_only, args=(v,), daemon=True)
        self.worker_thread.start()

    def _prep_run_ui(self):
        self.run_button.configure(state="disabled")
        self.stitch_only_button.configure(state="disabled")
        self.cancel_button.configure(state="normal")
        self.progress.configure(mode="indeterminate")
        self.progress.start(10)
        self.log_text.configure(state="normal")
        self.log_text.delete("1.0", "end")
        self.log_text.configure(state="disabled")

    def _cancel(self):
        self.cancel_requested.set()
        if self._current_proc and self._current_proc.poll() is None:
            self._current_proc.terminate()
        self._log("Cancel requested...")

    # ── Worker: full pipeline (Blender render + stitch, per animation) ──
    def _run_full_pipeline(self, v, anim_paths):
        error = None
        try:
            engine = "" if v["render_engine"] == "Auto" else v["render_engine"]
            total_anims = len(anim_paths)

            for i, anim_path in enumerate(anim_paths, start=1):
                if self.cancel_requested.is_set():
                    raise RuntimeError("Cancelled by user.")

                anim_name = sanitize_name(anim_path)
                self._set_overall_status(f"Animation {i}/{total_anims}: {os.path.basename(anim_path)}")
                self._log(f"\n=== [{i}/{total_anims}] {os.path.basename(anim_path)} ===")

                per_anim_render_dir = os.path.join(v["render_dir"], anim_name)

                job_config = build_job_config(
                    character_path=v["character_path"],
                    animation_path=anim_path,
                    output_dir=per_anim_render_dir,
                    sprite_size=v["sprite_size"],
                    num_directions=v["num_directions"],
                    camera_height_angle=v["camera_height_angle"],
                    frame_step=v["frame_step"],
                    render_engine=engine,
                    camera_follow_mode=v["camera_follow_mode"],
                )
                with open(TEMP_JOB_CONFIG_PATH, "w", encoding="utf-8") as f:
                    json.dump(job_config, f, indent=2)

                self._set_status("Rendering in Blender...")
                self.msg_queue.put(("progress", (0, None)))

                cmd = [v["blender_exe"], "--background", "--python", BLENDER_SCRIPT_PATH,
                       "--", TEMP_JOB_CONFIG_PATH]
                self._log("Running: " + " ".join(f'"{c}"' if " " in c else c for c in cmd))

                self._current_proc = subprocess.Popen(
                    cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1
                )

                switched_to_determinate = False

                for line in self._current_proc.stdout:
                    line = line.rstrip()
                    if not line:
                        continue

                    if self.cancel_requested.is_set():
                        self._current_proc.terminate()
                        raise RuntimeError("Cancelled by user.")

                    m = re.match(r"TOTAL_FRAMES (\d+)", line)
                    if m:
                        total = int(m.group(1))
                        self._log(f"Total frames to render: {total}")
                        self.msg_queue.put(("progress", (0, total)))
                        switched_to_determinate = True
                        continue

                    m = re.match(r"FRAME_DONE (\d+) (\d+)", line)
                    if m:
                        done, total = int(m.group(1)), int(m.group(2))
                        if not switched_to_determinate:
                            self.msg_queue.put(("progress", (0, total)))
                            switched_to_determinate = True
                        self.msg_queue.put(("progress", (done, None)))
                        self._set_status(f"Rendering frame {done}/{total}...")
                        continue

                    m = re.match(r"DIRECTION_DONE (\d+) (\S+) (\d+)", line)
                    if m:
                        self._log(f"Direction {m.group(1)} ({m.group(2)}\u00b0): {m.group(3)} frames rendered.")
                        continue

                    if line.startswith("ANIM_SOURCE"):
                        self._log(line)
                        continue

                    if line.startswith("RENDER_COMPLETE"):
                        self._log("Blender render complete for this animation.")
                        continue

                    self._log(line)

                returncode = self._current_proc.wait()
                if returncode != 0:
                    raise RuntimeError(f"Blender exited with code {returncode} while rendering "
                                        f"'{os.path.basename(anim_path)}'. Check the log above for details.")

                if self.cancel_requested.is_set():
                    raise RuntimeError("Cancelled by user.")

                # ── Stitch this animation's frames ──
                self._set_status("Stitching spritesheet...")
                self.msg_queue.put(("progress", (0, 1)))

                def progress_cb(done, total):
                    self.msg_queue.put(("progress", (done, total)))
                    self._set_status(f"Stitching frame {done}/{total}...")

                sheet_path = os.path.join(v["sheet_dir"], f"{anim_name}_spritesheet.png")
                result = stitch_spritesheet(per_anim_render_dir, sheet_path,
                                             progress_cb=progress_cb, log_cb=self._log)

                self._log(f"Spritesheet saved: {result['output_sheet']}")
                self._log(f"Grid: {result['num_cols']} columns x {result['num_rows']} rows, "
                          f"frame size {result['frame_w']}x{result['frame_h']}")

            self._set_overall_status(f"Completed {total_anims} animation(s).")
            self._set_status("")

        except Exception as e:
            error = e
        finally:
            self.msg_queue.put(("done", str(error) if error else None))

    # ── Worker: stitch only (all existing per-animation render sets found) ──
    def _run_stitch_only(self, v):
        error = None
        try:
            anim_sets = find_animation_sets(v["render_dir"])
            if not anim_sets:
                raise RuntimeError(f"No completed render sets (folders containing dir_XX subfolders) "
                                    f"found under {v['render_dir']}")

            total = len(anim_sets)
            for i, anim_name in enumerate(anim_sets, start=1):
                if self.cancel_requested.is_set():
                    raise RuntimeError("Cancelled by user.")

                self._set_overall_status(f"Stitching {i}/{total}: {anim_name}")
                self._log(f"\n=== [{i}/{total}] {anim_name} ===")

                per_anim_render_dir = os.path.join(v["render_dir"], anim_name)
                sheet_path = os.path.join(v["sheet_dir"], f"{anim_name}_spritesheet.png")

                def progress_cb(done, total_frames):
                    self.msg_queue.put(("progress", (done, total_frames)))
                    self._set_status(f"Stitching frame {done}/{total_frames}...")

                result = stitch_spritesheet(per_anim_render_dir, sheet_path,
                                             progress_cb=progress_cb, log_cb=self._log)

                self._log(f"Spritesheet saved: {result['output_sheet']}")
                self._log(f"Grid: {result['num_cols']} columns x {result['num_rows']} rows, "
                          f"frame size {result['frame_w']}x{result['frame_h']}")

            self._set_overall_status(f"Completed {total} spritesheet(s).")
            self._set_status("")

        except Exception as e:
            error = e
        finally:
            self.msg_queue.put(("done", str(error) if error else None))


def main():
    if not PILLOW_AVAILABLE:
        print("WARNING: Pillow is not installed in this Python environment.")
        print("Run: pip install pillow")

    root = tk.Tk()
    try:
        style = ttk.Style()
        if "vista" in style.theme_names():
            style.theme_use("vista")
        elif "clam" in style.theme_names():
            style.theme_use("clam")
    except Exception:
        pass

    app = SpritesheetApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()