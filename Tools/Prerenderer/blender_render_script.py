"""
Blender Render Script (spritesheet pipeline)
---------------------------------------------
Runs INSIDE Blender's bundled Python (only bpy/mathutils available -- do not
import anything from the GUI or stitch modules here).

Responsibilities:
    1. Import the character FBX (mesh + armature).
    2. Import the animation FBX (or reuse the character's own embedded
       action if animation_path == character_path).
    3. Copy the animation's action onto the character's armature (requires
       matching bone names/hierarchy -- e.g. both rigged via Mixamo).
    4. Remove the now-redundant animation-source mesh/armature.
    5. Render top-down/isometric frames from N camera directions, with the
       camera re-centering every frame to follow root motion.

Frames are written to OUTPUT_DIR/dir_00, dir_01, ... as frame_000.png,
frame_001.png, etc. Stitching those frames into a spritesheet PNG is handled
separately by stitch.py -- this script only renders to disk.

Invocation (see app.py):
    blender.exe --background --python blender_render_script.py -- <config.json>

<config.json> must contain:
    character_path        (str)  path to the character FBX
    animation_path         (str)  path to the animation FBX (may equal character_path)
    output_dir              (str)  where to write dir_XX/frame_NNN.png
    sprite_size             (int)  square render resolution, e.g. 256
    num_directions           (int)  how many camera yaw angles to render
    camera_height_angle    (float) 90 = top-down, ~45-60 = isometric-ish
    frame_step               (int)  render every Nth frame
    render_engine             (str)  "" (auto) / "BLENDER_EEVEE" / "BLENDER_EEVEE_NEXT" / "CYCLES"
    camera_follow_mode        (str)  "xy" (default) / "static" / "xyz" -- see below

Camera follow modes (fixes vertical bob/jump/crouch getting cancelled out):
    "xy"     -- (default) camera target follows the character's horizontal
                (X/Y) root motion every frame, but its Z height is locked to
                a single reference value computed once before rendering.
                Vertical motion (bob, jump, crouch) shows up as the sprite
                moving up/down in frame, while horizontal root motion still
                keeps the character centered left/right.
    "static" -- camera target is computed once and never updates. Use for
                in-place animations (idle, attack) where you want ALL
                motion, horizontal and vertical, visible in frame.
    "xyz"    -- legacy behavior: camera target fully re-centers on the
                character's bounding-box center every frame (X, Y, and Z).
                This cancels out vertical motion -- only use this if you
                specifically want the character's vertical position pinned
                in frame regardless of animation.

Progress/status is reported via stdout print() lines that the GUI parses:
    TOTAL_FRAMES <n>
    FRAME_DONE <done> <total>
    DIRECTION_DONE <dir_index> <yaw_deg> <frame_count>
    ANIM_SOURCE <embedded|filename>
    RENDER_COMPLETE <output_dir>
"""

import json
import math
import os
import sys

import bpy
import mathutils


def _load_config():
    argv = sys.argv
    if "--" in argv:
        argv = argv[argv.index("--") + 1:]
    else:
        argv = []

    if not argv:
        raise RuntimeError(
            "No config file path provided. Invoke as: "
            "blender --background --python blender_render_script.py -- <config.json>"
        )

    config_path = argv[0]
    with open(config_path, "r", encoding="utf-8") as f:
        return json.load(f)


def _get_scene_bounds():
    min_co = [float("inf")] * 3
    max_co = [-float("inf")] * 3
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        for corner in obj.bound_box:
            world_corner = obj.matrix_world @ mathutils.Vector(corner)
            for i in range(3):
                min_co[i] = min(min_co[i], world_corner[i])
                max_co[i] = max(max_co[i], world_corner[i])
    return min_co, max_co


def _get_character_center():
    min_co, max_co = _get_scene_bounds()
    return [(min_co[i] + max_co[i]) / 2 for i in range(3)]


def _point_camera_at(cam_obj, target, height_angle_deg, distance, yaw_deg):
    yaw = math.radians(yaw_deg)
    pitch = math.radians(90 - height_angle_deg)
    x = distance * math.sin(pitch) * math.sin(yaw)
    y = distance * math.sin(pitch) * math.cos(yaw)
    z = distance * math.cos(pitch)
    cam_obj.location = (target[0] + x, target[1] + y, target[2] + z)
    direction = mathutils.Vector(target) - cam_obj.location
    rot_quat = direction.to_track_quat("-Z", "Y")
    cam_obj.rotation_euler = rot_quat.to_euler()


def main():
    config = _load_config()

    character_path = config["character_path"]
    animation_path = config["animation_path"]
    output_dir = config["output_dir"]
    sprite_size = int(config["sprite_size"])
    num_directions = int(config["num_directions"])
    camera_height_angle = float(config["camera_height_angle"])
    frame_step = int(config["frame_step"])
    render_engine_override = config.get("render_engine", "") or ""
    camera_follow_mode = config.get("camera_follow_mode", "xy") or "xy"
    if camera_follow_mode not in ("xy", "static", "xyz"):
        camera_follow_mode = "xy"

    os.makedirs(output_dir, exist_ok=True)

    # --- Clear scene ---
    if bpy.data.objects:
        bpy.ops.object.select_all(action="SELECT")
        bpy.ops.object.delete()

    # --- Import the character (mesh + armature) ---
    bpy.ops.import_scene.fbx(filepath=character_path)

    character_armature = None
    for obj in bpy.context.scene.objects:
        if obj.type == "ARMATURE":
            character_armature = obj
            break

    if character_armature is None:
        raise RuntimeError("No armature found in character FBX: " + character_path)

    # --- Bring in the animation, either from a separate file or the character's own ---
    if os.path.normcase(os.path.abspath(animation_path)) == os.path.normcase(os.path.abspath(character_path)):
        # Same file -- just use whatever action Blender already attached on import.
        if not (character_armature.animation_data and character_armature.animation_data.action):
            raise RuntimeError("Character FBX has no embedded animation action.")
        action = character_armature.animation_data.action
        print("ANIM_SOURCE embedded", flush=True)
    else:
        pre_import_objects = set(bpy.context.scene.objects)
        bpy.ops.import_scene.fbx(filepath=animation_path)
        new_objects = [o for o in bpy.context.scene.objects if o not in pre_import_objects]

        anim_armature = None
        for obj in new_objects:
            if obj.type == "ARMATURE":
                anim_armature = obj
                break

        if anim_armature is None or not (anim_armature.animation_data and anim_armature.animation_data.action):
            raise RuntimeError("No animated armature found in animation FBX: " + animation_path)

        action = anim_armature.animation_data.action

        if not character_armature.animation_data:
            character_armature.animation_data_create()
        character_armature.animation_data.action = action

        # Clean up the animation-source armature and any meshes parented to it,
        # so they don't pollute the bounding-box / camera-framing calculations below.
        for obj in list(new_objects):
            if obj.name in bpy.context.scene.objects:
                bpy.data.objects.remove(bpy.data.objects[obj.name], do_unlink=True)

        print("ANIM_SOURCE " + os.path.basename(animation_path), flush=True)

    frame_start = int(action.frame_range[0])
    frame_end = int(action.frame_range[1])

    # Evaluate at a mid-animation frame to get the reference center used for
    # Z-locking ("xy" mode) and for the fixed target in "static" mode.
    mid_frame = (frame_start + frame_end) // 2
    bpy.context.scene.frame_set(mid_frame)
    min_co, max_co = _get_scene_bounds()
    reference_center = [(min_co[i] + max_co[i]) / 2 for i in range(3)]

    # Sample bounds across several frames (not just mid-animation) so the
    # ortho scale is big enough to fit the full range of motion -- e.g. the
    # top of a jump or the bottom of a crouch -- instead of cropping it.
    sample_frames = sorted(set([
        frame_start,
        frame_start + (frame_end - frame_start) // 4,
        mid_frame,
        frame_start + 3 * (frame_end - frame_start) // 4,
        frame_end,
    ]))
    size = 0.0
    for f in sample_frames:
        bpy.context.scene.frame_set(f)
        f_min_co, f_max_co = _get_scene_bounds()
        size = max(size, max(f_max_co[i] - f_min_co[i] for i in range(3)))
    if size <= 0:
        size = 1.0
    cam_distance = size * 3

    # --- Camera setup ---
    bpy.ops.object.camera_add()
    camera = bpy.context.object
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = size * 1.3
    bpy.context.scene.camera = camera

    # --- Lights ---
    bpy.ops.object.light_add(type="SUN")
    sun = bpy.context.object
    sun.data.energy = 3.0
    sun.rotation_euler = (math.radians(50), 0, math.radians(30))

    bpy.ops.object.light_add(type="SUN")
    fill = bpy.context.object
    fill.data.energy = 1.0
    fill.rotation_euler = (math.radians(50), 0, math.radians(210))

    # --- Render settings ---
    scene = bpy.context.scene
    available_engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    if render_engine_override and render_engine_override in available_engines:
        scene.render.engine = render_engine_override
    elif "BLENDER_EEVEE_NEXT" in available_engines:
        scene.render.engine = "BLENDER_EEVEE_NEXT"
    elif "BLENDER_EEVEE" in available_engines:
        scene.render.engine = "BLENDER_EEVEE"
    else:
        scene.render.engine = "CYCLES"

    scene.render.resolution_x = sprite_size
    scene.render.resolution_y = sprite_size
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"

    # Precompute total frame count for progress reporting
    frames_per_direction = len(range(frame_start, frame_end + 1, frame_step))
    total_frames = frames_per_direction * num_directions
    print(f"TOTAL_FRAMES {total_frames}", flush=True)

    rendered_so_far = 0

    for d in range(num_directions):
        yaw_deg = d * (360 / num_directions)
        dir_folder = os.path.join(output_dir, f"dir_{d:02d}")
        os.makedirs(dir_folder, exist_ok=True)

        frame = frame_start
        frame_index = 0
        while frame <= frame_end:
            scene.frame_set(frame)
            current_center = _get_character_center()

            if camera_follow_mode == "static":
                target = reference_center
            elif camera_follow_mode == "xy":
                target = [current_center[0], current_center[1], reference_center[2]]
            else:  # "xyz" -- legacy full re-centering, cancels out vertical motion
                target = current_center

            _point_camera_at(camera, target, camera_height_angle, cam_distance, yaw_deg)

            scene.render.filepath = os.path.join(dir_folder, f"frame_{frame_index:03d}.png")
            bpy.ops.render.render(write_still=True)

            rendered_so_far += 1
            print(f"FRAME_DONE {rendered_so_far} {total_frames}", flush=True)

            frame += frame_step
            frame_index += 1

        print(f"DIRECTION_DONE {d} {yaw_deg:.0f} {frame_index}", flush=True)

    print(f"RENDER_COMPLETE {output_dir}", flush=True)


if __name__ == "__main__":
    main()