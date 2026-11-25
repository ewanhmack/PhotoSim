"""
Read-only inspection of the already-built L_PhotoVillage. Opens the level as-is
(no edits, no save) and prints real measurements -- terrain height under every
placed actor, GoodSky's actual exposed properties, tree spacing stats -- so
fixes are made against evidence instead of guesses.

Run headless:
  UnrealEditor-Cmd.exe <uproject> -unattended -nosplash -RenderOffscreen
    -ExecutePythonScript="Tools/LevelGen/diagnose_photo_village.py"
"""
import math
import unreal

LEVEL = "/Game/PhotoSim/Maps/L_PhotoVillage"

CANDIDATE_GOODSKY_PROPS = [
    "Time", "TimeOfDay", "Time Of Day", "DayTime", "CurrentTime", "TimeSpeed",
    "Preset", "CurrentPreset", "WeatherPreset", "Weather Preset", "SkyPreset",
    "AutoTimeOfDay", "bAutoTimeOfDay", "AutoPlay", "bAutoPlay", "bAutoUpdateTime",
    "bDayNightCycle", "DayNightCycle", "bEnableDayNightCycle",
    "SunIntensity", "SunBrightness", "CloudCoverage", "Clouds Coverage",
]


def ground_z(world, x, y):
    start = unreal.Vector(x, y, 50000.0)
    end = unreal.Vector(x, y, -50000.0)
    hit = unreal.SystemLibrary.line_trace_single(
        world, start, end,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        False, [], unreal.DrawDebugTrace.NONE,
        ignore_self=True,
    )
    if hit is None:
        return None
    return hit.to_tuple()[4].z


def main():
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_subsystem.load_level(LEVEL):
        unreal.log_error("Could not load {}".format(LEVEL))
        return
    world = unreal.EditorLevelLibrary.get_editor_world()
    actors = unreal.EditorLevelLibrary.get_all_level_actors()

    unreal.log("===DIAG_START===")

    # --- Per-actor grounding -------------------------------------------
    unreal.log("--- Actor grounding (folder, class, loc, rot, bottom_z, ground_z, delta) ---")
    for actor in actors:
        folder = str(actor.get_folder_path())
        if not folder.startswith("PhotoVillage"):
            continue
        loc = actor.get_actor_location()
        rot = actor.get_actor_rotation()
        try:
            origin, extent = actor.get_actor_bounds(only_colliding_components=False)
            bottom_z = origin.z - extent.z
        except Exception:
            bottom_z = None
        gz = ground_z(world, loc.x, loc.y)
        delta = (bottom_z - gz) if (bottom_z is not None and gz is not None) else None
        unreal.log("ACTORROW|{}|{}|{:.1f},{:.1f},{:.1f}|{:.1f},{:.1f},{:.1f}|{}|{}|{}".format(
            folder, actor.get_class().get_name(),
            loc.x, loc.y, loc.z, rot.pitch, rot.yaw, rot.roll,
            "{:.1f}".format(bottom_z) if bottom_z is not None else "NA",
            "{:.1f}".format(gz) if gz is not None else "NA",
            "{:.1f}".format(delta) if delta is not None else "NA",
        ))

    # --- Village footprint flatness (sample across each building's full extent) ---
    unreal.log("--- Village building footprint flatness (4 corners + center) ---")
    for actor in actors:
        folder = str(actor.get_folder_path())
        if folder != "PhotoVillage/Village":
            continue
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        loc = actor.get_actor_location()
        origin, extent = actor.get_actor_bounds(only_colliding_components=False)
        corners = [
            (loc.x - extent.x, loc.y - extent.y), (loc.x + extent.x, loc.y - extent.y),
            (loc.x - extent.x, loc.y + extent.y), (loc.x + extent.x, loc.y + extent.y),
            (loc.x, loc.y),
        ]
        heights = [ground_z(world, cx, cy) for cx, cy in corners]
        heights_valid = [h for h in heights if h is not None]
        spread = (max(heights_valid) - min(heights_valid)) if heights_valid else -1
        unreal.log("FOOTPRINT|{}|center=({:.0f},{:.0f})|extent=({:.0f},{:.0f})|heights={}|spread={:.1f}".format(
            actor.get_name(), loc.x, loc.y, extent.x, extent.y,
            ["{:.1f}".format(h) if h is not None else "NA" for h in heights], spread))

    # --- River line flatness (sample along its length) ------------------
    unreal.log("--- River line flatness (sampled along length) ---")
    for actor in actors:
        folder = str(actor.get_folder_path())
        if folder != "PhotoVillage/Water":
            continue
        if "Water" not in actor.get_class().get_name() and "SM_Water" not in str(actor.get_actor_label()):
            pass
        loc = actor.get_actor_location()
        origin, extent = actor.get_actor_bounds(only_colliding_components=False)
        samples_y = [loc.y - extent.y, loc.y - extent.y / 2, loc.y, loc.y + extent.y / 2, loc.y + extent.y]
        heights = [ground_z(world, loc.x, sy) for sy in samples_y]
        heights_valid = [h for h in heights if h is not None]
        spread = (max(heights_valid) - min(heights_valid)) if heights_valid else -1
        unreal.log("RIVERLINE|{}|x={:.0f}|y_range=({:.0f},{:.0f})|heights={}|spread={:.1f}".format(
            actor.get_name(), loc.x, loc.y - extent.y, loc.y + extent.y,
            ["{:.1f}".format(h) if h is not None else "NA" for h in heights], spread))

    # --- Tree spacing / formation ---------------------------------------
    unreal.log("--- Tree formation ---")
    tree_positions = []
    for actor in actors:
        if str(actor.get_folder_path()) == "PhotoVillage/Trees":
            loc = actor.get_actor_location()
            tree_positions.append((loc.x, loc.y))
    unreal.log("TREECOUNT|{}".format(len(tree_positions)))
    if len(tree_positions) > 1:
        min_dists = []
        for i, (x1, y1) in enumerate(tree_positions):
            best = None
            for j, (x2, y2) in enumerate(tree_positions):
                if i == j:
                    continue
                d = math.hypot(x1 - x2, y1 - y2)
                if best is None or d < best:
                    best = d
            min_dists.append(best)
        unreal.log("TREE_NEAREST_DIST|min={:.1f}|max={:.1f}|avg={:.1f}".format(
            min(min_dists), max(min_dists), sum(min_dists) / len(min_dists)))
        dists_from_origin_center = None  # filled below once we know village center

    # --- GoodSky property discovery -------------------------------------
    unreal.log("--- GoodSky property discovery ---")
    good_sky = None
    for actor in actors:
        if "GoodSky" in actor.get_class().get_name():
            good_sky = actor
            break
    if good_sky is None:
        unreal.log("GOODSKY|NOT FOUND")
    else:
        unreal.log("GOODSKY|class={}|name={}".format(good_sky.get_class().get_name(), good_sky.get_name()))
        all_attrs = dir(good_sky)
        unreal.log("GOODSKY_DIR_COUNT|{}".format(len(all_attrs)))
        interesting = [a for a in all_attrs if any(
            k.lower() in a.lower() for k in ("time", "sun", "preset", "day", "night", "cycle", "weather", "cloud", "speed", "auto"))]
        unreal.log("GOODSKY_INTERESTING_ATTRS|{}".format(interesting))
        for name in CANDIDATE_GOODSKY_PROPS:
            try:
                val = good_sky.get_editor_property(name)
                unreal.log("GOODSKY_PROP|{}|{}|{}".format(name, type(val).__name__, val))
            except Exception as e:
                pass

    unreal.log("===DIAG_END===")


main()
unreal.SystemLibrary.quit_editor()
