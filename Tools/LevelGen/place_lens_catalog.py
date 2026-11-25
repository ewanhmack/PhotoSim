"""Places the full 15-lens JSON catalog as ALensPickup actors in a row beside the existing
camera pickup in L_PhotoVillage, for visual detail. Each lens gets the real FLensDefinition
parsed from the catalog (via ULensJSONImporter, the same importer the game loads at runtime)
and an instance-time mesh scale baked in so the row reads correctly in the editor viewport too,
not just at PIE runtime (ALensPickup::ApplyVisuals re-applies the same scale in BeginPlay, so
this is a no-op duplicate at runtime rather than a second source of truth).

Lenses are laid out ascending by focal length along the camera pickup's right vector. Spacing is
read from each spawned lens's OWN mesh bounds (SM_Lens's real size) rather than a hardcoded
guess, so this keeps working if the mesh is ever re-imported at a different native scale.

Idempotent: re-running this deletes any ALensPickup actors already in the level first, so it
never accumulates duplicates.

Run headless (after rebuilding PhotoSimEditor so the editor's Python bindings pick up the new
FLensDefinition.OverrideMesh property and ALensPickup::ComputeLengthScaleForFocalLength):
  UnrealEditor-Cmd.exe <uproject> -unattended -nosplash -RenderOffscreen
    -ExecutePythonScript="Tools/LevelGen/place_lens_catalog.py"
"""
import unreal

LEVEL_PATH = "/Game/PhotoSim/Maps/L_PhotoVillage"
CATALOG_RELATIVE_PATH = "PhotoSim/Data/LensCatalog.json"

START_OFFSET_FROM_CAMERA = 200.0
GAP_BETWEEN_LENSES = 40.0


def find_camera_pickup(actor_subsystem):
    for actor in actor_subsystem.get_all_level_actors():
        if isinstance(actor, unreal.CameraPickup):
            return actor
    return None


def remove_existing_lens_pickups(actor_subsystem):
    existing = [actor for actor in actor_subsystem.get_all_level_actors() if isinstance(actor, unreal.LensPickup)]
    for actor in existing:
        actor_subsystem.destroy_actor(actor)
    if existing:
        unreal.log("Removed {} existing LensPickup actors before re-placing".format(len(existing)))


def load_lens_catalog():
    content_dir = unreal.Paths.project_content_dir()
    relative_path = content_dir + CATALOG_RELATIVE_PATH
    abs_path = unreal.Paths.convert_relative_path_to_full(relative_path)

    # Python bindings for a UFUNCTION with non-const reference out-params don't accept them as
    # arguments - they come back as an (out_lenses, out_warnings) tuple, or None if the bool
    # return value was false.
    result = unreal.LensJSONImporter.import_lenses_from_file(abs_path)
    if result is None:
        unreal.log_error("Failed to import lens catalog from {}".format(abs_path))
        return None

    out_lenses, out_warnings = result
    for warning in out_warnings:
        unreal.log_warning("[LensCatalog] {}".format(warning))

    if len(out_lenses) == 0:
        unreal.log_error("Lens catalog at {} imported no rows".format(abs_path))
        return None

    return list(out_lenses)


def main():
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_subsystem.load_level(LEVEL_PATH):
        unreal.log_error("Could not load level {}".format(LEVEL_PATH))
        unreal.SystemLibrary.quit_editor()
        return

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    remove_existing_lens_pickups(actor_subsystem)

    camera_pickup = find_camera_pickup(actor_subsystem)
    if camera_pickup is None:
        unreal.log_error("===FAILED: no ACameraPickup found in {} to place lenses beside===".format(LEVEL_PATH))
        unreal.SystemLibrary.quit_editor()
        return

    lenses = load_lens_catalog()
    if lenses is None:
        unreal.log_error("===FAILED: lens catalog did not import===")
        unreal.SystemLibrary.quit_editor()
        return

    lenses.sort(key=lambda lens: lens.get_editor_property("max_focal_length_mm"))

    camera_location = camera_pickup.get_actor_location()
    camera_rotation = camera_pickup.get_actor_rotation()
    right_vector = unreal.MathLibrary.get_right_vector(camera_rotation)
    lens_rotation = unreal.Rotator(0.0, camera_rotation.yaw, 0.0)

    offset_along_row = START_OFFSET_FROM_CAMERA
    placed_count = 0

    for lens in lenses:
        # Spawned at the camera's own location first, then moved once the real scale/footprint
        # (which depends on the actor's own NativeMeshScaleCorrection and its mesh's real bounds)
        # is known - avoids hardcoding either as a guess in this script.
        lens_actor = actor_subsystem.spawn_actor_from_class(unreal.LensPickup, camera_location, lens_rotation)
        if lens_actor is None:
            unreal.log_error("Failed to spawn lens pickup for {}".format(lens.get_editor_property("name")))
            continue

        max_focal_length = lens.get_editor_property("max_focal_length_mm")
        base_scale = unreal.LensPickup.compute_length_scale_for_focal_length(max_focal_length)
        native_correction = lens_actor.get_editor_property("native_mesh_scale_correction")
        scale = base_scale * native_correction

        lens_mesh = lens_actor.get_editor_property("lens_mesh")
        lens_mesh.set_editor_property("relative_scale3d", unreal.Vector(scale, scale, scale))

        static_mesh = lens_mesh.get_editor_property("static_mesh")
        native_radius = static_mesh.get_bounds().sphere_radius if static_mesh is not None else 50.0
        half_footprint = native_radius * scale

        offset_along_row += half_footprint
        spawn_location = camera_location + right_vector * offset_along_row
        spawn_location.z = camera_location.z + half_footprint
        lens_actor.set_actor_location(spawn_location, False, False)

        lens_actor.set_editor_property("lens_definition", lens)

        lens_name = str(lens.get_editor_property("name"))
        lens_actor.set_actor_label("LensPickup_{}".format(lens_name.replace(" ", "_").replace("/", "-")))
        lens_actor.set_folder_path("Pickups/Lenses")

        offset_along_row += half_footprint + GAP_BETWEEN_LENSES
        placed_count += 1

    if placed_count != len(lenses):
        unreal.log_warning("Placed {} of {} lenses - see errors above".format(placed_count, len(lenses)))

    level_subsystem.save_current_level()
    unreal.log("===LENS_CATALOG_PLACEMENT_DONE: placed {} lenses beside {}===".format(placed_count, camera_pickup.get_actor_label()))
    unreal.SystemLibrary.quit_editor()


main()
