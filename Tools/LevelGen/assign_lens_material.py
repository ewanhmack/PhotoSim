"""Lens.obj's companion Lens.mtl (and whatever textures it referenced) wasn't exported alongside
it, so the OBJ importer left all 3 of SM_Lens's material slots on Unreal's default checkerboard
placeholder (WorldGridMaterial) - that's the "empty" look in the Content Browser thumbnail, not
missing geometry (1488 verts / 2470 tris import fine). Until Julien re-exports with materials,
this reuses whatever material SM_Camera's own body already uses, so the lens at least reads as
"dark camera equipment" instead of a checker pattern.

Run headless:
  UnrealEditor-Cmd.exe <uproject> -unattended -nosplash -RenderOffscreen
    -ExecutePythonScript="Tools/LevelGen/assign_lens_material.py"
"""
import unreal

LENS_MESH_PATH = "/Game/PhotoSim/Meshes/Lens"
CAMERA_MESH_PATH = "/Game/PhotoSim/Meshes/SM_Camera"


def get_first_material(mesh):
    for slot in mesh.get_editor_property("static_materials"):
        material = slot.get_editor_property("material_interface")
        if material is not None:
            return material
    return None


def main():
    lens_mesh = unreal.EditorAssetLibrary.load_asset(LENS_MESH_PATH)
    camera_mesh = unreal.EditorAssetLibrary.load_asset(CAMERA_MESH_PATH)

    if lens_mesh is None or camera_mesh is None:
        unreal.log_error("===FAILED: could not load Lens and/or SM_Camera===")
        unreal.SystemLibrary.quit_editor()
        return

    camera_material = get_first_material(camera_mesh)
    if camera_material is None:
        unreal.log_error("===FAILED: SM_Camera has no material to borrow===")
        unreal.SystemLibrary.quit_editor()
        return

    unreal.log("Borrowing material {} from SM_Camera".format(camera_material.get_name()))

    slots = lens_mesh.get_editor_property("static_materials")
    for i, slot in enumerate(slots):
        slot.set_editor_property("material_interface", camera_material)
        unreal.log("Set Lens material slot {} to {}".format(i, camera_material.get_name()))
    lens_mesh.set_editor_property("static_materials", slots)

    save_result = unreal.EditorAssetLibrary.save_loaded_asset(lens_mesh)
    unreal.log("save_loaded_asset returned: {}".format(save_result))

    # Re-load from the in-memory package and re-check the slots actually stuck, independent of
    # whether the file on disk was written.
    reloaded_slots = lens_mesh.get_editor_property("static_materials")
    for i, slot in enumerate(reloaded_slots):
        mat = slot.get_editor_property("material_interface")
        unreal.log("Post-save slot {}: {}".format(i, mat.get_name() if mat is not None else "None"))

    unreal.log("===LENS_MATERIAL_ASSIGN_DONE===")
    # Quitting immediately (quit_editor() right here, in the same call stack as the save above)
    # crashed the process on exit twice in this session - letting Quit arrive as its own later
    # exec command instead gives the engine a few ticks to settle after the save first.


main()
