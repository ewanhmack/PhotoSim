"""
Regression test for L_PhotoVillage. Loads the already-built level and asserts
the specific failures build_photo_village.py used to have are actually gone:
  - grounding: snap_to_terrain() used to re-trace through the actor's own
    just-spawned collision instead of the terrain under it, launching houses,
    the bridge and lamps hundreds of units into the air.
  - rotation: unreal.Rotator(0.0, yaw_deg, 0.0) positional args land as
    (roll, pitch, yaw) in this engine's Python bindings, not (pitch, yaw,
    roll), so every "pure yaw" rotation was secretly a random pitch/roll.
  - lighting: GoodSky defaulted to a time-of-day-driven mode with no time
    set, rendering as night -- and separately, GoodSky turned out to be a
    decorative sky dome with no DirectionalLightComponent or
    SkyLightComponent anywhere on it, so even with the preset fixed the
    actual scene geometry stayed pitch black; only the sky itself rendered.
    A real light rig is spawned independently of GoodSky now.
  - tree formation: a thin distant ring with overlapping canopies instead of
    a forest, with trees close enough to clip into each other. Later widened
    further: coverage now spans the whole build area (BUILD_RADIUS 15000, up
    from 6000) plus a dedicated dense forest wedge, not just scattered
    individual trees around the village.
  - gameplay: PlayerStart must sit outside one specific house with the
    camera pickup immediately next to it, and the river needed a collision
    floor so the player couldn't fall through into the void -- later made
    moot entirely: the scripted river was replaced by water from the
    MountainRange heightmap itself, which has continuous collision
    everywhere, so that check now just skips instead of failing.

This file tracks a level that moved from fully scripted to hand-edited
(trees hand-painted with the Foliage Tool, layout hand-tuned in editor), so
tree/foliage counts are informational only, not pass/fail gates -- but
grounding, rotation, lighting, and spacing/overlap checks stay meaningful
regardless of who placed something.

Run headless:
  UnrealEditor-Cmd.exe <uproject> -unattended -nosplash -RenderOffscreen
    -ExecutePythonScript="Tools/LevelGen/verify_photo_village.py"

Prints PASS/FAIL per check and raises on the first failure (visible in the
log as a Python traceback) so a failing run is unambiguous, not just quieter.
"""
import math
import unreal

LEVEL = "/Game/PhotoSim/Maps/L_PhotoVillage"

GROUND_TOLERANCE = 30.0         # single-point-trace noise on sloped ground for thin
                                 # foliage meshes runs up to ~17 units; the bug this
                                 # check exists to catch was 300-1600 units, so this
                                 # still has zero slack against the real failure mode
ROTATION_TOLERANCE = 0.5        # degrees
MIN_TREE_SPACING_FLOOR = 130.0  # below DENSE_ZONE_MIN_SPACING (150) in the build script, with slack
MIN_DIRECTIONAL_LIGHT_INTENSITY = 1.0
MIN_BUILDING_GAP = 150.0        # below VILLAGE_ROW_MARGIN in the build script, with slack
PLAYERSTART_PICKUP_MAX_DIST = 300.0
PLAYERSTART_HOUSE_ALIGN_TOLERANCE = 50.0

failures = []
checks_run = 0


def check(label, condition, detail=""):
    global checks_run
    checks_run += 1
    status = "PASS" if condition else "FAIL"
    unreal.log("CHECK|{}|{}|{}".format(status, label, detail))
    if not condition:
        failures.append("{}: {}".format(label, detail))


def ground_z(world, x, y, ignore_actors=None):
    start = unreal.Vector(x, y, 50000.0)
    end = unreal.Vector(x, y, -50000.0)
    hit = unreal.SystemLibrary.line_trace_single(
        world, start, end,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        False, ignore_actors or [], unreal.DrawDebugTrace.NONE,
        ignore_self=True,
    )
    if hit is None:
        return None
    return hit.to_tuple()[4].z


def main():
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_subsystem.load_level(LEVEL):
        raise RuntimeError("Could not load {}".format(LEVEL))
    world = unreal.EditorLevelLibrary.get_editor_world()
    actors = unreal.EditorLevelLibrary.get_all_level_actors()

    buildings, lamps, trees, foliage = [], [], [], []
    bridge = None
    good_sky = None
    player_start = None
    camera_pickup = None
    water = None
    safety_floor = None

    for actor in actors:
        folder = str(actor.get_folder_path())
        cls_name = actor.get_class().get_name()
        if "GoodSky" in cls_name:
            good_sky = actor
        elif isinstance(actor, unreal.PlayerStart):
            player_start = actor
        elif "BP_CameraPickup" in cls_name:
            camera_pickup = actor
        elif folder == "PhotoVillage/Village" and cls_name == "BP_Lamp_C":
            lamps.append(actor)
        elif folder == "PhotoVillage/Village" and isinstance(actor, unreal.StaticMeshActor):
            buildings.append(actor)
        elif folder.startswith("PhotoVillage/Trees"):
            trees.append(actor)
        elif folder == "PhotoVillage/Foliage":
            foliage.append(actor)
        elif folder == "PhotoVillage/Water":
            loc = actor.get_actor_location()
            if water is None or loc.z > water.get_actor_location().z:
                water = actor if water is None else water
            if safety_floor is None:
                safety_floor = actor

    # The bridge is a StaticMeshActor in the Village folder too; separate it
    # from houses by its distinctive extent (it's long and thin, houses aren't).
    # Water vs safety floor: two StaticMeshActors in PhotoVillage/Water, the
    # floor is the lower one (it sits below the riverbed by design).
    water_actors = [a for a in actors if str(a.get_folder_path()) == "PhotoVillage/Water"]
    water_actors.sort(key=lambda a: a.get_actor_location().z, reverse=True)
    water = water_actors[0] if water_actors else None
    safety_floor = water_actors[1] if len(water_actors) > 1 else None

    # The bridge is the Village StaticMeshActor spawned at the river's own
    # (x, y) -- everything else in that folder is a house, offset from it.
    bridge = None
    real_buildings = []
    if water is not None:
        w_loc = water.get_actor_location()
        for b in buildings:
            b_loc = b.get_actor_location()
            if math.hypot(b_loc.x - w_loc.x, b_loc.y - w_loc.y) < 50.0:
                bridge = b
            else:
                real_buildings.append(b)
        buildings = real_buildings

    # --- 1. Grounding: nothing floats or sinks beyond tolerance ------------
    terrain_conforming = buildings + lamps + ([bridge] if bridge else []) + [camera_pickup] + trees + foliage
    terrain_conforming = [a for a in terrain_conforming if a is not None]
    max_abs_delta = 0.0
    worst = None
    for actor in terrain_conforming:
        loc = actor.get_actor_location()
        origin, extent = actor.get_actor_bounds(only_colliding_components=False)
        bottom = origin.z - extent.z
        # Exclude every other terrain-conforming actor too, not just this
        # one: with hundreds of trees/foliage packed into the scene, a
        # single-actor exclusion can clip a neighboring canopy at a grazing
        # angle and report a false float that was never actually there --
        # the build script already excludes everything placed so far for
        # exactly this reason, so the check needs to match it to mean
        # anything.
        gz = ground_z(world, loc.x, loc.y, ignore_actors=terrain_conforming)
        if gz is None:
            continue
        delta = abs(bottom - gz)
        if delta > max_abs_delta:
            max_abs_delta = delta
            worst = actor.get_name()
    check("grounding_no_float_or_sink", max_abs_delta <= GROUND_TOLERANCE,
          "max |bottom-ground| = {:.1f} units (worst actor: {})".format(max_abs_delta, worst))

    # --- 1b. Buildings don't crowd or overlap each other -------------------
    # The row used to space every building off one global max-extent margin
    # at a uniform pivot x, which ignored that these meshes have very
    # different footprints and off-center pivots (a porch/dock can jut out
    # 500+ units from the mesh's own origin). It never produced a true AABB
    # overlap, but it put bounding boxes close enough, and misaligned enough
    # on x, to read as a jumbled cluster instead of a village. Assert a real
    # minimum gap between every pair, not just "doesn't overlap".
    worst_building_gap = None
    worst_pair = None
    for i in range(len(buildings)):
        for j in range(i + 1, len(buildings)):
            o1, e1 = buildings[i].get_actor_bounds(only_colliding_components=False)
            o2, e2 = buildings[j].get_actor_bounds(only_colliding_components=False)
            # The row separates buildings along y by construction; that's
            # the axis a spacing regression would actually show up on.
            gap_y = abs(o1.y - o2.y) - e1.y - e2.y
            if worst_building_gap is None or gap_y < worst_building_gap:
                worst_building_gap = gap_y
                worst_pair = (buildings[i].get_name(), buildings[j].get_name())
    check("buildings_have_real_spacing",
          worst_building_gap is not None and worst_building_gap >= MIN_BUILDING_GAP,
          "smallest building-to-building gap = {} (pair: {})".format(
              "{:.1f}".format(worst_building_gap) if worst_building_gap is not None else "N/A", worst_pair))

    # --- 2. Rotation: no accidental pitch/roll from the Rotator arg-order bug ---
    bad_rotation = []
    for actor in buildings + lamps + trees + foliage + ([bridge] if bridge else []) + [camera_pickup]:
        if actor is None:
            continue
        rot = actor.get_actor_rotation()
        if abs(rot.pitch) > ROTATION_TOLERANCE or abs(rot.roll) > ROTATION_TOLERANCE:
            bad_rotation.append((actor.get_name(), rot.pitch, rot.roll))
    check("no_unintended_pitch_or_roll", len(bad_rotation) == 0,
          "{} actors with nonzero pitch/roll: {}".format(len(bad_rotation), bad_rotation[:5]))

    if bridge is not None:
        bridge_rot = bridge.get_actor_rotation()
        check("bridge_rotation_is_pure_yaw",
              abs(bridge_rot.pitch) < ROTATION_TOLERANCE and abs(bridge_rot.roll) < ROTATION_TOLERANCE,
              "bridge rotation pitch={:.1f} yaw={:.1f} roll={:.1f}".format(
                  bridge_rot.pitch, bridge_rot.yaw, bridge_rot.roll))

    # --- 3. Lighting: a real light actually illuminates the scene ----------
    # GoodSky looking right (its sky dome preset) is necessary but not
    # sufficient: GoodSky is a decorative mesh system with no
    # DirectionalLightComponent or SkyLightComponent anywhere on it, so its
    # preset alone leaves every piece of placed geometry pitch black. Only
    # caught by rendering an actual frame and seeing the scene stay solid
    # black with the sky rendering fine -- assert the real light directly so
    # this can't silently regress back to "sky looks right, scene is dark".
    if good_sky is not None:
        preset = good_sky.get_editor_property("SkyPreset")
        check("goodsky_is_bright_daytime", preset.name == "NOON_CLEAR_SKY", str(preset))
    else:
        check("goodsky_is_bright_daytime", False, "GoodSky actor not found in level")

    brightest_directional = 0.0
    for actor in actors:
        if not isinstance(actor, unreal.DirectionalLight):
            continue
        comp = actor.get_component_by_class(unreal.DirectionalLightComponent)
        if comp is None:
            continue
        try:
            intensity = comp.get_editor_property("intensity")
        except Exception:
            continue
        brightest_directional = max(brightest_directional, intensity)
    check("scene_has_real_directional_light", brightest_directional >= MIN_DIRECTIONAL_LIGHT_INTENSITY,
          "brightest DirectionalLightComponent intensity = {:.1f} (need >= {:.1f})".format(
              brightest_directional, MIN_DIRECTIONAL_LIGHT_INTENSITY))

    # --- 4. Tree formation: coverage and no clipping ------------------------
    # Trees are now hand-painted with the Foliage Tool (Content/PhotoSim/
    # Foliage/) rather than always matching the scripted scatter's exact
    # count, so the count itself is informational, not a pass/fail gate --
    # only report it. Canopy-overlap spacing is still a real quality bar
    # regardless of who placed them, but needs at least 2 trees to mean
    # anything, which won't hold while painting is still in progress.
    unreal.log("INFO|tree_count|{} trees|foliage_count|{} clumps".format(len(trees), len(foliage)))

    tree_positions = [(a.get_actor_location().x, a.get_actor_location().y) for a in trees]
    if len(tree_positions) > 1:
        min_nearest = min(
            min(math.hypot(x1 - x2, y1 - y2) for j, (x2, y2) in enumerate(tree_positions) if i != j)
            for i, (x1, y1) in enumerate(tree_positions)
        )
        check("no_tree_canopy_overlap", min_nearest >= MIN_TREE_SPACING_FLOOR,
              "nearest tree-to-tree distance = {:.1f}".format(min_nearest))
    else:
        unreal.log("SKIP|no_tree_canopy_overlap|only {} tree(s) placed, nothing to compare".format(len(tree_positions)))

    # --- 5. Gameplay: PlayerStart outside one specific house, pickup beside it ---
    if player_start is not None and camera_pickup is not None:
        ps_loc = player_start.get_actor_location()
        pickup_loc = camera_pickup.get_actor_location()
        dist = math.hypot(ps_loc.x - pickup_loc.x, ps_loc.y - pickup_loc.y)
        check("pickup_next_to_playerstart", dist <= PLAYERSTART_PICKUP_MAX_DIST,
              "distance = {:.1f}".format(dist))

        aligned_house = any(
            abs(b.get_actor_location().y - ps_loc.y) <= PLAYERSTART_HOUSE_ALIGN_TOLERANCE
            for b in buildings
        )
        check("playerstart_outside_one_specific_house", aligned_house,
              "PlayerStart y={:.1f}, building y's={}".format(
                  ps_loc.y, [round(b.get_actor_location().y, 1) for b in buildings]))
    else:
        check("pickup_next_to_playerstart", False, "PlayerStart or camera pickup missing")
        check("playerstart_outside_one_specific_house", False, "PlayerStart or camera pickup missing")

    # --- 6. River safety floor: only applies if a scripted water setup exists ---
    # The river was intentionally replaced with water from the MountainRange
    # heightmap itself -- there's no separate water mesh floating over a void
    # anymore, so no safety floor is needed: the landscape's own collision
    # (verified continuous everywhere, see diagnose_photo_village.py's hole
    # scan) already guarantees solid ground under it. Only check this if a
    # PhotoVillage/Water setup happens to exist again in the future.
    if water is not None and safety_floor is not None:
        w_origin, w_extent = water.get_actor_bounds(only_colliding_components=False)
        f_origin, f_extent = safety_floor.get_actor_bounds(only_colliding_components=False)
        covers_x = (f_origin.x - f_extent.x) <= (w_origin.x - w_extent.x) and \
                   (f_origin.x + f_extent.x) >= (w_origin.x + w_extent.x)
        covers_y = (f_origin.y - f_extent.y) <= (w_origin.y - w_extent.y) and \
                   (f_origin.y + f_extent.y) >= (w_origin.y + w_extent.y)
        below_water = (f_origin.z + f_extent.z) < (w_origin.z - w_extent.z)
        check("safety_floor_covers_river_footprint", covers_x and covers_y,
              "water extent=({:.0f},{:.0f}) floor extent=({:.0f},{:.0f})".format(
                  w_extent.x, w_extent.y, f_extent.x, f_extent.y))
        check("safety_floor_sits_below_riverbed", below_water,
              "floor top z={:.1f}, water bottom z={:.1f}".format(
                  f_origin.z + f_extent.z, w_origin.z - w_extent.z))
    else:
        unreal.log("SKIP|river_safety_floor|no PhotoVillage/Water setup -- water comes from the heightmap now")

    unreal.log("===VERIFY_SUMMARY|{}/{} checks passed===".format(
        checks_run - len(failures), checks_run))
    if failures:
        raise AssertionError("{} check(s) failed:\n  ".format(len(failures)) + "\n  ".join(failures))
    unreal.log("===VERIFY_ALL_PASSED===")


main()
unreal.SystemLibrary.quit_editor()
