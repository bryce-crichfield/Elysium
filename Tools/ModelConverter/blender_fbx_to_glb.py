"""
Runs INSIDE Blender (headless). Not meant to be run directly.

    blender -b --factory-startup --python blender_fbx_to_glb.py -- jobs.json

jobs.json is a list of [input_fbx, output_glb] pairs. All files are converted in a
single Blender session (Blender's startup time is the slow part), and one status
line per file is printed for the caller to parse:

    BAKE_OK<TAB><output path>
    BAKE_FAIL<TAB><input path><TAB><reason>
"""
import json
import os
import sys
import traceback

import bpy


def log(*a):
    print(*a, flush=True)


def supported(op, **kw):
    """Drop keyword args the installed Blender version doesn't know about.
    Operator property names drift between Blender releases; passing an unknown one raises."""
    try:
        props = set(op.get_rna_type().properties.keys())
    except Exception:
        return kw
    return {k: v for k, v in kw.items() if k in props}


def enable_addons():
    try:
        import addon_utils
    except Exception:
        return
    for mod in ("io_scene_fbx", "io_scene_gltf2"):
        try:
            addon_utils.enable(mod, default_set=False, persistent=False)
        except Exception:
            pass


def clean_scene():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for coll in (bpy.data.actions, bpy.data.meshes, bpy.data.armatures,
                 bpy.data.materials, bpy.data.images, bpy.data.textures):
        for block in list(coll):
            coll.remove(block)


def import_fbx(path):
    if hasattr(bpy.types, "IMPORT_SCENE_OT_fbx"):
        op = bpy.ops.import_scene.fbx
    elif hasattr(bpy.types, "WM_OT_fbx_import"):  # newer native importer, if present
        op = bpy.ops.wm.fbx_import
    else:
        raise RuntimeError("No FBX importer available in this Blender build")
    op(**supported(op, filepath=path))


def export_glb(path):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    op = bpy.ops.export_scene.gltf
    op(**supported(
        op,
        filepath=path,
        export_format="GLB",
        export_yup=True,
        export_apply=False,                   # modifiers + armatures don't mix
        export_animations=True,
        export_force_sampling=True,           # bake every frame, no curve interpolation surprises
        export_optimize_animation_size=False, # keep constant channels
        export_skins=True,
        export_def_bones=False,               # keep ALL bones, not just deforming ones
        export_image_format="AUTO",
        export_cameras=False,
        export_lights=False,
        export_extras=False,
    ))


def main():
    argv = sys.argv
    job_file = argv[argv.index("--") + 1]
    with open(job_file, encoding="utf-8") as f:
        jobs = json.load(f)

    enable_addons()
    for src, dst in jobs:
        try:
            clean_scene()
            import_fbx(src)
            export_glb(dst)
            log(f"BAKE_OK\t{dst}")
        except Exception as e:  # keep going so one bad file doesn't sink the batch
            traceback.print_exc()
            log(f"BAKE_FAIL\t{src}\t{e}")


main()
