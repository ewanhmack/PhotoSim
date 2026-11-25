"""
Creates one FoliageType_InstancedStaticMesh asset per tree mesh already used
in build_photo_village.py, saved under /Game/PhotoSim/Foliage/, so Julien can
paint them by hand with the Foliage Tool's brush instead of (or alongside)
the scripted scatter.

Mesh, random yaw, and "stay upright" (no align-to-normal) are set to match
what the scripted placement already does. Density, scale range, and ground
slope filtering are left at engine defaults for hand-tuning in the Foliage
Mode panel, since that's the whole point of handing this over to a brush.

Run headless:
  UnrealEditor-Cmd.exe <uproject> -unattended -nosplash -RenderOffscreen
    -ExecutePythonScript="Tools/LevelGen/create_tree_foliage_types.py"
"""
import unreal

SPRUCE_MESHES = [
    "/Game/PN_interactiveSpruceForest/Meshes/full/high/spruce_full_01",
    "/Game/PN_interactiveSpruceForest/Meshes/full/high/spruce_full_02",
    "/Game/PN_interactiveSpruceForest/Meshes/full/high/spruce_full_03",
    "/Game/PN_interactiveSpruceForest/Meshes/half/high/spruce_half_01",
    "/Game/PN_interactiveSpruceForest/Meshes/half/high/spruce_half_02",
    "/Game/PN_interactiveSpruceForest/Meshes/half/high/spruce_half_03",
    "/Game/PN_interactiveSpruceForest/Meshes/half/high/spruce_half_04",
    "/Game/PN_interactiveSpruceForest/Meshes/small/spruce_small_01",
    "/Game/PN_interactiveSpruceForest/Meshes/small/spruce_small_02",
    "/Game/PN_interactiveSpruceForest/Meshes/small/spruce_small_03",
]
PINE_MESHES = [
    "/Game/Modular_Rural_Cabin/Meshes/Foliage/Pine_Tree",
    "/Game/Modular_Rural_Cabin/Meshes/Foliage/SM_Pine_Tree_01",
    "/Game/Modular_Rural_Cabin/Meshes/Foliage/SM_Pine_Tree_02",
    "/Game/Modular_Rural_Cabin/Meshes/Foliage/SM_Pine_Tree_03",
    "/Game/Modular_Rural_Cabin/Meshes/Foliage/SM_Pine_Tree_04",
    "/Game/Modular_Rural_Cabin/Meshes/Foliage/SM_Pine_Tree_05",
]

DEST_FOLDER = "/Game/PhotoSim/Foliage"


def create_foliage_type(mesh_path):
    mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
    if mesh is None:
        unreal.log_warning("Missing mesh, skipping: {}".format(mesh_path))
        return None

    mesh_name = mesh_path.rsplit("/", 1)[-1]
    asset_name = "FT_{}".format(mesh_name)
    dest_path = "{}/{}".format(DEST_FOLDER, asset_name)

    if unreal.EditorAssetLibrary.does_asset_exist(dest_path):
        unreal.EditorAssetLibrary.delete_asset(dest_path)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    foliage_type = asset_tools.create_asset(asset_name, DEST_FOLDER, unreal.FoliageType_InstancedStaticMesh, None)
    if foliage_type is None:
        unreal.log_error("Failed to create foliage type for {}".format(mesh_path))
        return None

    foliage_type.set_editor_property("mesh", mesh)
    foliage_type.set_editor_property("random_yaw", True)
    foliage_type.set_editor_property("align_to_normal", False)  # trees stay vertical regardless of ground slope

    unreal.EditorAssetLibrary.save_asset(dest_path)
    unreal.log("Created {}".format(dest_path))
    return foliage_type


def main():
    if not unreal.EditorAssetLibrary.does_directory_exist(DEST_FOLDER):
        unreal.EditorAssetLibrary.make_directory(DEST_FOLDER)

    created = []
    for mesh_path in SPRUCE_MESHES + PINE_MESHES:
        ft = create_foliage_type(mesh_path)
        if ft is not None:
            created.append(mesh_path)

    unreal.log("===CREATED {}/{} tree foliage types in {}===".format(
        len(created), len(SPRUCE_MESHES) + len(PINE_MESHES), DEST_FOLDER))


main()
unreal.SystemLibrary.quit_editor()
