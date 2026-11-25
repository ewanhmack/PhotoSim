"""
Creates BP_FilmCameraPickup next to BP_CameraPickup at /Game, parented to ACameraPickup, using
the imported Fab vintage SLR camera mesh and configured as a Film body. A placeable Blueprint
asset, not a level instance - drag it into the level wherever you want, as many times as you want.

Safe to run from the editor's own Python console the same way as the other scripts in this folder;
already verified headlessly against this project before being handed over.
"""

import unreal

BLUEPRINT_NAME = "BP_FilmCameraPickup"
BLUEPRINT_PACKAGE_PATH = "/Game"
MESH_PATH = "/Game/Fab/Vintage_SLR_Camera/vintage_slr_camera/StaticMeshes/vintage_slr_camera.vintage_slr_camera"


def main():
    asset_path = BLUEPRINT_PACKAGE_PATH + "/" + BLUEPRINT_NAME

    mesh = unreal.load_asset(MESH_PATH)
    if mesh is None:
        unreal.log_error("Could not load the vintage camera mesh at {}".format(MESH_PATH))
        return

    existing = unreal.EditorAssetLibrary.find_asset_data(asset_path)
    if existing.is_valid():
        # Re-running after an earlier failed attempt left this half-configured - load and finish
        # configuring it instead of requiring it to be deleted first.
        unreal.log("{} already exists - configuring it rather than recreating it.".format(asset_path))
        new_blueprint = unreal.load_asset(asset_path)
    else:
        parent_class = unreal.load_class(None, "/Script/PhotoSim.CameraPickup")
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)

        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        new_blueprint = asset_tools.create_asset(BLUEPRINT_NAME, BLUEPRINT_PACKAGE_PATH, None, factory)

    if new_blueprint is None:
        unreal.log_error("Blueprint was not created/loaded.")
        return

    # new_blueprint.get_editor_property("generated_class") doesn't resolve on this engine build -
    # load the generated class directly by its well-known _C suffix instead.
    generated_class = unreal.load_class(None, "{}/{}.{}_C".format(BLUEPRINT_PACKAGE_PATH, BLUEPRINT_NAME, BLUEPRINT_NAME))
    if generated_class is None:
        unreal.log_error("Could not load the generated class for the new Blueprint.")
        return

    cdo = unreal.get_default_object(generated_class)

    body_definition = cdo.get_editor_property("body_definition")
    body_definition.set_editor_property("name", "Vintage Film Camera")
    body_definition.set_editor_property("medium_type", unreal.CameraMediumType.FILM)
    body_definition.set_editor_property("override_mesh", mesh)
    cdo.set_editor_property("body_definition", body_definition)

    unreal.BlueprintEditorLibrary.compile_blueprint(new_blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(new_blueprint)

    unreal.log("Created {}/{} - parent ACameraPickup, BodyDefinition.MediumType=Film, OverrideMesh={}. "
               "Drag it into the level from the Content Browser wherever you want.".format(
                   BLUEPRINT_PACKAGE_PATH, BLUEPRINT_NAME, MESH_PATH))


main()
