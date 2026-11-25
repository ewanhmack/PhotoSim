"""Imports Julien's real lens model (exported from Cinema4D to FBX) as a static mesh at
/Game/PhotoSim/Meshes/SM_Lens, and logs its bounding box next to SM_Camera's so an absurd
import-scale mismatch (a very common C4D->FBX->UE units problem) is obvious before anyone
bothers nudging rotation/scale in the editor.

Run headless:
  UnrealEditor-Cmd.exe <uproject> -unattended -nosplash -RenderOffscreen
    -ExecutePythonScript="Tools/LevelGen/import_lens_mesh.py"
"""
import unreal

SOURCE_FBX = "C:/Users/ewanm/Downloads/LENTE.fbx"
DEST_PATH = "/Game/PhotoSim/Meshes"
DEST_NAME = "SM_Lens"


def import_lens_mesh():
    # UE 5.6 routes FBX import through the newer Interchange pipeline by default, which is async
    # (the asset isn't available - not in the registry, not even on the AssetImportTask itself -
    # synchronously right after import_asset_tasks returns). Forcing the legacy FbxFactory path
    # for this one import keeps it synchronous, which is all a one-shot headless script needs.
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")

    dest_object_path = "{}/{}".format(DEST_PATH, DEST_NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(dest_object_path):
        unreal.log("{} already exists, re-importing over it".format(dest_object_path))

    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = True
    options.import_textures = True
    options.static_mesh_import_data.combine_meshes = True

    task = unreal.AssetImportTask()
    task.filename = SOURCE_FBX
    task.destination_path = DEST_PATH
    task.destination_name = DEST_NAME
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = options

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    # UE 5.6 routes FBX import through Interchange, which is async - the asset registry hasn't
    # necessarily caught up yet right after import_asset_tasks returns, so load_asset(path) can
    # come back None even on a successful import. task.get_objects() asks the task directly for
    # what it actually created, which is available synchronously.
    imported_objects = task.get_objects()
    if len(imported_objects) > 0:
        return imported_objects[0]

    unreal.log_warning("task.get_objects() was empty - falling back to an asset registry lookup")
    return unreal.EditorAssetLibrary.load_asset(dest_object_path)


def log_bounds(label, mesh):
    if mesh is None:
        unreal.log_warning("{}: could not load mesh".format(label))
        return
    bounds = mesh.get_bounds()
    extent = bounds.box_extent
    unreal.log("{}: box_extent=({:.2f}, {:.2f}, {:.2f})  diagonal={:.2f}".format(
        label, extent.x, extent.y, extent.z,
        (extent.x ** 2 + extent.y ** 2 + extent.z ** 2) ** 0.5))


def main():
    lens_mesh = import_lens_mesh()
    if lens_mesh is None:
        unreal.log_error("===FAILED: SM_Lens did not import===")
        unreal.SystemLibrary.quit_editor()
        return

    camera_mesh = unreal.EditorAssetLibrary.load_asset("/Game/PhotoSim/Meshes/SM_Camera")

    log_bounds("SM_Lens", lens_mesh)
    log_bounds("SM_Camera", camera_mesh)

    unreal.log("===LENS_MESH_IMPORT_DONE===")
    unreal.SystemLibrary.quit_editor()


main()
