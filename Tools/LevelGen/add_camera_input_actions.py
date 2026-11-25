"""Creates IA_RollCameraModifier (Alt) and IA_ExposureModeModifier (M) as boolean Input Action
assets under /Game/PhotoSim/Input/, matching the existing modifier actions (IA_ApertureModifier
etc.), and maps both into IMC_Camera. Read-only otherwise - does not touch any level.

Run headless:
  UnrealEditor-Cmd.exe <uproject> -unattended -nosplash -RenderOffscreen
    -ExecutePythonScript="Tools/LevelGen/add_camera_input_actions.py"
"""
import unreal

INPUT_FOLDER = "/Game/PhotoSim/Input"
IMC_PATH = "/Game/PhotoSim/Input/IMC_Camera"


def create_boolean_action(name):
    dest_path = "{}/{}".format(INPUT_FOLDER, name)
    if unreal.EditorAssetLibrary.does_asset_exist(dest_path):
        unreal.log("Already exists, skipping create: {}".format(dest_path))
        return unreal.EditorAssetLibrary.load_asset(dest_path)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    action = asset_tools.create_asset(name, INPUT_FOLDER, unreal.InputAction, None)
    if action is None:
        unreal.log_error("Failed to create {}".format(name))
        return None

    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    unreal.EditorAssetLibrary.save_asset(dest_path)
    unreal.log("Created {}".format(dest_path))
    return action


def main():
    roll_action = create_boolean_action("IA_RollCameraModifier")
    exposure_mode_action = create_boolean_action("IA_ExposureModeModifier")

    if roll_action is None or exposure_mode_action is None:
        unreal.log_error("===FAILED: one or both Input Actions did not get created===")
        unreal.SystemLibrary.quit_editor()
        return

    imc = unreal.EditorAssetLibrary.load_asset(IMC_PATH)
    if imc is None:
        unreal.log_error("Could not load {}".format(IMC_PATH))
        unreal.SystemLibrary.quit_editor()
        return

    existing_mappings = imc.get_editor_property("mappings")
    existing_action_names = set()
    for m in existing_mappings:
        action = m.get_editor_property("action")
        if action is not None:
            existing_action_names.add(action.get_name())

    new_mappings = list(existing_mappings)

    def make_key(key_name):
        key = unreal.Key()
        key.set_editor_property("key_name", key_name)
        return key

    if "IA_RollCameraModifier" not in existing_action_names:
        roll_mapping = unreal.EnhancedActionKeyMapping()
        roll_mapping.set_editor_property("action", roll_action)
        roll_mapping.set_editor_property("key", make_key("LeftAlt"))
        new_mappings.append(roll_mapping)
        unreal.log("Mapped IA_RollCameraModifier -> LeftAlt")
    else:
        unreal.log("IA_RollCameraModifier already mapped in IMC_Camera, skipping")

    if "IA_ExposureModeModifier" not in existing_action_names:
        exposure_mapping = unreal.EnhancedActionKeyMapping()
        exposure_mapping.set_editor_property("action", exposure_mode_action)
        exposure_mapping.set_editor_property("key", make_key("M"))
        new_mappings.append(exposure_mapping)
        unreal.log("Mapped IA_ExposureModeModifier -> M")
    else:
        unreal.log("IA_ExposureModeModifier already mapped in IMC_Camera, skipping")

    imc.set_editor_property("mappings", new_mappings)
    unreal.EditorAssetLibrary.save_asset(IMC_PATH)
    unreal.log("===INPUT_ACTIONS_DONE===")
    unreal.SystemLibrary.quit_editor()


main()
