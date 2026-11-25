"""
Builds L_PhotoVillage: a small river-village area for the photography game,
built on top of the MWLandscapeAutoMaterial MountainRange landscape (real
sculpted terrain, not a flat platform), dressed with pieces hand-placed from
the LakeTown, PN_interactiveSpruceForest, Modular_Rural_Cabin, FogArea and
GoodSky Fab packs, plus the project's own camera pickup.

Every placement is measured against the real terrain height at that exact
spot (a line trace), not guessed -- so objects sit correctly on the mountain
range's actual slopes rather than assuming flat ground. The landscape itself
is huge; this script only builds within a small, chosen patch of it, found by
sampling the terrain for a reasonably flat site rather than assuming world
origin happens to be buildable.

Run headless:
  UnrealEditor-Cmd.exe <uproject> -unattended -nosplash -RenderOffscreen
    -ExecutePythonScript="Tools/LevelGen/build_photo_village.py"

Deterministic: fixed random seed, same input packs -> same layout every run.
"""
import math
import random
import unreal

SEED = 42
NEW_LEVEL = "/Game/PhotoSim/Maps/L_PhotoVillage"
SOURCE_LEVEL = "/Game/MWLandscapeAutoMaterial/Maps/LandscapeAutoMaterial_MountainRange_Example"

GOOD_SKY_BP = "/Game/GoodSky/Blueprint/BP_GoodSky"
GOOD_SKY_PRESET_PROPERTY = "SkyPreset"
# The template level brings its own lights; GoodSky is meant to be the only
# light source, so anything in these classes gets removed after loading it.
LIGHT_CLASSES_TO_CLEAR = (
    unreal.DirectionalLight, unreal.SkyLight, unreal.SkyAtmosphere, unreal.ExponentialHeightFog,
)

WATER_MESH = "/Game/LakeTown/Meshes/SM_WaterPlane"
WATER_TARGET_LENGTH = 5000.0
WATER_TARGET_WIDTH = 700.0
SAFETY_FLOOR_MESH = "/Engine/BasicShapes/Plane"  # guaranteed to have collision, unlike the water shader plane
SAFETY_FLOOR_DROP = 100.0
SAFETY_FLOOR_MARGIN = 1.25

BRIDGE_MESH = "/Game/LakeTown/Meshes/SM_BridgeA"

VILLAGE_BUILDING_MESHES = [
    "/Game/LakeTown/Meshes/SM_HouseA",
    "/Game/LakeTown/Meshes/SM_Market",
    "/Game/LakeTown/Meshes/SM_HouseB",
    "/Game/LakeTown/Meshes/SM_HouseC",
]
LAMP_BP = "/Game/LakeTown/Blueprints/BP_Lamp"
VILLAGE_ROW_MARGIN = 250.0
RIVER_CLEARANCE = 600.0

CAMERA_PICKUP_BP = "/Game/BP_CameraPickup"
CAMERA_PICKUP_OFFSET_Y = 150.0

FOG_AREA_BP = "/Game/FogArea/Blueprints/BP_FogArea"

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
# Spruce weighted 2x so the forest reads as spruce with pine mixed in, not the reverse.
TREE_POOL = SPRUCE_MESHES * 2 + PINE_MESHES

GROUND_FOLIAGE = [
    "/Game/Modular_Rural_Cabin/Meshes/Foliage/Bush_1",
    "/Game/Modular_Rural_Cabin/Meshes/Foliage/Bush_Tree",
    "/Game/Modular_Rural_Cabin/Meshes/Foliage/Grass_Patch_1",
    "/Game/Modular_Rural_Cabin/Meshes/Foliage/Grass_Patch_2",
    "/Game/Modular_Rural_Cabin/Meshes/Foliage/Grass_Patch_Long",
    "/Game/Modular_Rural_Cabin/Meshes/Foliage/Flower_Patch_1",
    "/Game/Modular_Rural_Cabin/Meshes/Foliage/Shrubs_1",
    "/Game/Modular_Rural_Cabin/Meshes/Foliage/Shrubs_1_Single",
]

NUM_TREES = 450             # broad scatter across the whole build area
NUM_FOLIAGE = 200
BUILD_RADIUS = 15000.0      # how far out the treeline/site extends from the village center --
                            # "entire area" coverage, not just a clearing around the village
RING_INNER_FRACTION = 0.12  # forest starts just past the river/village clearance, not a distant ring
RING_OUTER_FRACTION = 1.0
MIN_TREE_SPACING = 220.0    # below this, canopies visibly overlap/clip

# A dedicated denser forest block, not just uniformly-scattered trees -- a
# wedge of the build area (by angle, relative to the village) packed much
# tighter so it reads as an actual forest instead of scattered individual
# trees. Placed after the broad scatter and spacing-checked against it, so
# it can't clip anything the broad pass already put down.
DENSE_ZONE_ANGLE_MIN = 60.0
DENSE_ZONE_ANGLE_MAX = 150.0
DENSE_ZONE_INNER_FRACTION = 0.18
DENSE_ZONE_OUTER_FRACTION = 0.65
DENSE_ZONE_MIN_SPACING = 150.0
NUM_DENSE_ZONE_TREES = 350

# --- Flat-site search -------------------------------------------------------
SITE_SEARCH_HALF_EXTENT = 10000.0
SITE_SEARCH_STEP = 2500.0
SITE_SAMPLE_OFFSET = 1200.0
TRACE_START_Z = 50000.0
TRACE_END_Z = -50000.0


def load_mesh(path):
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if mesh is None:
        unreal.log_warning("Missing mesh: {}".format(path))
    return mesh


def load_class(path):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if cls is None:
        unreal.log_warning("Missing blueprint class: {}".format(path))
    return cls


def actor_bounds(actor):
    """(origin, extent) of the actor's world-space bounding box. origin is the
    box CENTER, so top = origin.z + extent.z and bottom = origin.z - extent.z
    regardless of the mesh's own pivot convention."""
    return actor.get_actor_bounds(only_colliding_components=False)


def ground_z(world, x, y, ignore_actors=None):
    """Real terrain height at (x, y), via line trace. None if nothing hit.

    ignore_actors must list any actor already standing at (x, y) -- the
    `ignore_self` kwarg below ignores the world-context object, never the
    actor being grounded, so re-querying after an actor is spawned without
    this would hit the actor's own collision instead of the terrain under
    it."""
    start = unreal.Vector(x, y, TRACE_START_Z)
    end = unreal.Vector(x, y, TRACE_END_Z)
    hit_result = unreal.SystemLibrary.line_trace_single(
        world, start, end,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        False, ignore_actors or [], unreal.DrawDebugTrace.NONE,
        ignore_self=True,
    )
    if hit_result is None:
        return None
    # HitResult.Location is reflection-protected in this engine build;
    # to_tuple()[4] is the same field exposed unrestricted.
    return hit_result.to_tuple()[4].z


def find_flat_site(world):
    """Scan a grid around world origin for the flattest buildable patch,
    rather than assuming (0, 0) happens to be usable on a mountain range."""
    best = None
    steps = int(SITE_SEARCH_HALF_EXTENT / SITE_SEARCH_STEP)
    offsets = [(dx, dy) for dx in (-SITE_SAMPLE_OFFSET, 0.0, SITE_SAMPLE_OFFSET)
               for dy in (-SITE_SAMPLE_OFFSET, 0.0, SITE_SAMPLE_OFFSET)]
    for gx in range(-steps, steps + 1):
        for gy in range(-steps, steps + 1):
            cx, cy = gx * SITE_SEARCH_STEP, gy * SITE_SEARCH_STEP
            heights = []
            for dx, dy in offsets:
                z = ground_z(world, cx + dx, cy + dy)
                if z is None:
                    heights = None
                    break
                heights.append(z)
            if not heights:
                continue
            spread = max(heights) - min(heights)
            if best is None or spread < best[0]:
                best = (spread, cx, cy, heights[len(heights) // 2])
    if best is None:
        unreal.log_warning("No flat site found in search area; falling back to world origin")
        return 0.0, 0.0, ground_z(world, 0.0, 0.0) or 0.0
    spread, cx, cy, cz = best
    unreal.log("Flat site chosen at ({:.0f}, {:.0f}), local height spread {:.0f}".format(cx, cy, spread))
    return cx, cy, cz


def spawn_mesh(mesh_path, x, y, z, yaw_deg=0.0, scale=1.0, folder=""):
    mesh = load_mesh(mesh_path)
    if mesh is None:
        return None
    actor = unreal.EditorLevelLibrary.spawn_actor_from_object(
        mesh, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw_deg)
    )
    if actor is None:
        return None
    if scale != 1.0:
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    if folder:
        actor.set_folder_path(folder)
    return actor


def spawn_class_actor(class_path, x, y, z, yaw_deg=0.0, folder=""):
    cls = load_class(class_path)
    if cls is None:
        return None
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        cls, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw_deg)
    )
    if actor is not None and folder:
        actor.set_folder_path(folder)
    return actor


def ground_to(actor, top_z):
    """Shift actor vertically so the bottom of its bounding box sits exactly
    on top_z. Works for any mesh regardless of pivot height."""
    origin, extent = actor_bounds(actor)
    bottom = origin.z - extent.z
    delta = top_z - bottom
    loc = actor.get_actor_location()
    actor.set_actor_location(unreal.Vector(loc.x, loc.y, loc.z + delta), False, False)
    return actor_bounds(actor)


def snap_to_terrain(world, actor):
    """ground_to, but looks up the real terrain height under the actor's own
    (x, y) instead of taking a caller-supplied constant -- needed once the
    ground is a sculpted landscape instead of a flat plane."""
    loc = actor.get_actor_location()
    z = ground_z(world, loc.x, loc.y, ignore_actors=[actor])
    if z is None:
        return actor_bounds(actor)
    return ground_to(actor, z)


def setup_lighting_and_terrain_template(level_subsystem):
    if unreal.EditorAssetLibrary.does_asset_exist(NEW_LEVEL):
        unreal.EditorAssetLibrary.delete_asset(NEW_LEVEL)
    if not level_subsystem.new_level_from_template(NEW_LEVEL, SOURCE_LEVEL):
        return False

    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if isinstance(actor, LIGHT_CLASSES_TO_CLEAR):
            unreal.EditorLevelLibrary.destroy_actor(actor)

    good_sky = spawn_class_actor(GOOD_SKY_BP, 0, 0, 0, folder="PhotoVillage/Lighting")
    if good_sky is not None:
        # GoodSky defaults to CUSTOM_MODE_FROM_TIME_OF_DAY with no time set,
        # which renders as night. NOON_CLEAR_SKY is a fixed bright-daytime
        # preset, not time-driven, so it doesn't need a companion time value.
        preset_enum = type(good_sky.get_editor_property(GOOD_SKY_PRESET_PROPERTY))
        good_sky.set_editor_property(GOOD_SKY_PRESET_PROPERTY, preset_enum.NOON_CLEAR_SKY)

    # GoodSky is a decorative sky dome -- its 15 components are all meshes
    # (sun/moon/star billboards, the sky ring, an arrow gizmo, a text
    # readout), with no DirectionalLightComponent or SkyLightComponent
    # anywhere on it (checked via get_components_by_class against an actual
    # built level). It was never going to illuminate the scene; the dome
    # just renders a blue unlit material regardless of what's lighting the
    # ground under it. Deleting the template's real lights on the assumption
    # GoodSky replaced them left every piece of placed geometry pitch black
    # even with a "bright" sky overhead -- confirmed by disabling
    # auto-exposure and seeing the scene stay solid black while only the sky
    # rendered. A real light rig always needs to exist; GoodSky layers a
    # nicer-looking sky on top of it, it doesn't substitute for it.
    sun = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(roll=0.0, pitch=-60.0, yaw=45.0))
    if sun is not None:
        comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
        if comp is not None:
            comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
            comp.set_editor_property("intensity", 10.0)
        sun.set_folder_path("PhotoVillage/Lighting")
    sky_light = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1000))
    if sky_light is not None:
        comp = sky_light.get_component_by_class(unreal.SkyLightComponent)
        if comp is not None:
            comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
            comp.set_editor_property("real_time_capture", True)
        sky_light.set_folder_path("PhotoVillage/Lighting")

    if good_sky is None:
        # No dome visuals available either -- add SkyAtmosphere so there's at
        # least a proper sky instead of the engine's flat default background.
        # Skipped when GoodSky is present: its dome mesh already encloses the
        # scene, and layering SkyAtmosphere behind an unverified third-party
        # mesh risks fighting it for no visible benefit.
        unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    return True


def main():
    random.seed(SEED)

    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not setup_lighting_and_terrain_template(level_subsystem):
        unreal.log_error("Failed to create {} from template {}".format(NEW_LEVEL, SOURCE_LEVEL))
        return

    world = unreal.EditorLevelLibrary.get_editor_world()
    vx, vy, vz = find_flat_site(world)

    # Every prop placed so far, so later ground traces for trees/foliage
    # can't hit a building/lamp/bridge/water actor that happens to share
    # their column and get grounded to its roof instead of the terrain.
    placed_actors = []

    # --- River + safety floor ------------------------------------------
    river_x = vx - BUILD_RADIUS * 0.3
    river_z = ground_z(world, river_x, vy)
    if river_z is None:
        river_z = vz
    water = spawn_mesh(WATER_MESH, river_x, vy, river_z, folder="PhotoVillage/Water")
    river_half_width = 350.0
    river_half_length = WATER_TARGET_LENGTH / 2.0
    if water is not None:
        _, water_extent_natural = actor_bounds(water)
        natural_length = max(water_extent_natural.y * 2.0, 1.0)
        natural_width = max(water_extent_natural.x * 2.0, 1.0)
        scale_y = WATER_TARGET_LENGTH / natural_length
        scale_x = WATER_TARGET_WIDTH / natural_width
        # Non-uniform on purpose: the water tile is roughly square, a river isn't.
        water.set_actor_scale3d(unreal.Vector(scale_x, scale_y, 1.0))
        water_origin, water_extent = actor_bounds(water)
        river_half_width = water_extent.x
        river_half_length = water_extent.y
        water.set_actor_location(unreal.Vector(river_x, vy, river_z - 20.0), False, False)
        water_origin, water_extent = actor_bounds(water)

        floor = spawn_mesh(SAFETY_FLOOR_MESH, river_x, vy, 0, folder="PhotoVillage/Water")
        if floor is not None:
            # /Engine/BasicShapes/Plane is 100x100 units unscaled; scale to cover
            # the water footprint with margin, then drop it below the water bed
            # so players who swim/fall never fall through into the void.
            floor.set_actor_scale3d(unreal.Vector(
                (river_half_width * 2.0 * SAFETY_FLOOR_MARGIN) / 100.0,
                (river_half_length * 2.0 * SAFETY_FLOOR_MARGIN) / 100.0,
                1.0,
            ))
            floor_bottom_target = water_origin.z - water_extent.z - SAFETY_FLOOR_DROP
            ground_to(floor, floor_bottom_target)
            unreal.log("Safety floor placed under river at Z {:.0f}".format(floor_bottom_target))
            placed_actors.append(floor)
        placed_actors.append(water)
    else:
        unreal.log_warning("No water placed; skipping safety floor")

    # --- Bridge -----------------------------------------------------------
    bridge = spawn_mesh(BRIDGE_MESH, river_x, vy, river_z, folder="PhotoVillage/Village")
    if bridge is not None:
        _, bridge_extent = actor_bounds(bridge)
        bridge_yaw = 0.0 if bridge_extent.x >= bridge_extent.y else 90.0
        if bridge_yaw != 0.0:
            bridge.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=bridge_yaw), False)
        snap_to_terrain(world, bridge)
        placed_actors.append(bridge)

    # --- Village row --------------------------------------------------
    # LakeTown's stilt-house meshes have wildly different footprints (196 to
    # 656 half-extent) and off-center pivots -- a porch or dock can jut out
    # 500+ units from the mesh's own origin, in a direction that differs per
    # mesh. Spacing everything off one global max-extent margin, at a
    # uniform pivot x, left some buildings crowding their neighbors while
    # wasting space around others: it read as a jumbled cluster, not a row.
    # Fixed by measuring each building's own extent AND its bounding-box
    # center offset from its pivot, then placing every building so its real
    # bbox center -- not its pivot -- sits on the row's front line, with the
    # gap between each pair sized from their own actual half-extents.
    footprints = []  # (extent, offset_x, offset_y) per mesh, or None
    for mesh_path in VILLAGE_BUILDING_MESHES:
        probe = spawn_mesh(mesh_path, vx, vy, vz)
        if probe is None:
            footprints.append(None)
            continue
        origin, extent = actor_bounds(probe)
        footprints.append((extent, origin.x - vx, origin.y - vy))
        unreal.EditorLevelLibrary.destroy_actor(probe)

    max_extent_x = max((f[0].x for f in footprints if f is not None), default=300.0)
    village_x = river_x + river_half_width + RIVER_CLEARANCE + max_extent_x

    total_span = sum(f[0].y * 2.0 for f in footprints if f is not None)
    total_span += VILLAGE_ROW_MARGIN * max(len(VILLAGE_BUILDING_MESHES) - 1, 0)
    cursor_y = vy - total_span / 2.0
    village_min_y = cursor_y
    village_max_y = cursor_y + total_span

    first_building_spawn_y = None
    for i, mesh_path in enumerate(VILLAGE_BUILDING_MESHES):
        if footprints[i] is None:
            continue
        extent, offset_x, offset_y = footprints[i]
        bbox_center_y = cursor_y + extent.y
        spawn_y = bbox_center_y - offset_y
        spawn_x = village_x - offset_x
        if first_building_spawn_y is None:
            first_building_spawn_y = spawn_y
        z = ground_z(world, spawn_x, spawn_y)
        if z is None:
            z = vz
        building = spawn_mesh(mesh_path, spawn_x, spawn_y, z, folder="PhotoVillage/Village")
        if building is not None:
            snap_to_terrain(world, building)
            placed_actors.append(building)
        cursor_y += extent.y * 2.0 + VILLAGE_ROW_MARGIN
    if first_building_spawn_y is None:
        first_building_spawn_y = vy

    # Flank the row in front of its own face (same x depth as the buildings'
    # leading edge) and past its actual y-extent with margin -- using the
    # row's own computed min/max, not a re-derived offset, so a lamp can
    # never land inside a building's footprint regardless of that building's
    # individual extent.
    LAMP_MARGIN = 300.0
    lamp_x = village_x - max_extent_x - LAMP_MARGIN * 0.5
    for ly in (village_min_y - LAMP_MARGIN, village_max_y + LAMP_MARGIN):
        lz = ground_z(world, lamp_x, ly)
        lamp = spawn_class_actor(LAMP_BP, lamp_x, ly,
                                  lz if lz is not None else vz, folder="PhotoVillage/Village")
        if lamp is not None:
            snap_to_terrain(world, lamp)
            placed_actors.append(lamp)

    unreal.log("Village row: x={:.0f}, y=[{:.0f}, {:.0f}], river edge at x={:.0f}".format(
        village_x, village_min_y, village_max_y, river_x + river_half_width))

    # --- Player start + camera pickup, outside the first house -----------
    arrival_house_y = first_building_spawn_y
    player_x = village_x - max_extent_x - 300.0
    player_z = ground_z(world, player_x, arrival_house_y)
    if player_z is None:
        player_z = vz
    # Face toward the village, not toward wherever the river happens to be --
    # comparing against river_x here was wrong and had the player spawning
    # facing away from the house (caught by actually looking at a render,
    # not by any property check: facing direction doesn't show up as a
    # grounding/rotation-magnitude bug, only as "the camera points at a
    # wall").
    facing_yaw = 0.0 if village_x >= player_x else 180.0
    player_start = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(player_x, arrival_house_y, player_z + 90.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=facing_yaw)
    )
    if player_start is not None:
        player_start.set_folder_path("PhotoVillage/Gameplay")

    pickup_y = arrival_house_y + CAMERA_PICKUP_OFFSET_Y
    pickup_z = ground_z(world, player_x, pickup_y)
    pickup = spawn_class_actor(
        CAMERA_PICKUP_BP, player_x, pickup_y, pickup_z if pickup_z is not None else vz,
        folder="PhotoVillage/Gameplay",
    )
    if pickup is not None:
        snap_to_terrain(world, pickup)

    unreal.log("PlayerStart at ({:.0f}, {:.0f}), camera pickup at ({:.0f}, {:.0f})".format(
        player_x, arrival_house_y, player_x, pickup_y))

    # --- Trees + foliage ring ------------------------------------------------
    def in_river_band(x, y):
        return (abs(x - river_x) < river_half_width + 200.0) and (abs(y - vy) < river_half_length + 200.0)

    def in_village_band(x, y):
        return (village_x - max_extent_x - 200.0 < x < village_x + max_extent_x + 200.0
                and village_min_y - 200.0 < y < village_max_y + 200.0)

    def too_close(x, y, placed, min_spacing):
        for px, py in placed:
            if (x - px) ** 2 + (y - py) ** 2 < min_spacing ** 2:
                return True
        return False

    def area_uniform_frac(inner, outer):
        # Sampling the radius fraction itself uniformly packs points much
        # denser near the inner edge, since the area of an annular ring
        # shrinks as the radius shrinks while point *count* per unit radius
        # stays constant -- that's why the forest was crowding right against
        # the village and thinning out to nothing further away even though
        # its bounding extent measured wide. Uniform-by-area needs the
        # radius to scale with the square root of a uniform value instead.
        inner = max(inner, 0.0)
        return math.sqrt(random.uniform(inner * inner, outer * outer))

    tree_positions = []

    def scatter_trees(angle_min, angle_max, frac_inner, frac_outer, min_spacing, target_count, folder="PhotoVillage/Trees"):
        placed_count = 0
        attempts = 0
        max_attempts = target_count * 15
        while placed_count < target_count and attempts < max_attempts:
            attempts += 1
            angle = random.uniform(angle_min, angle_max)
            rad = unreal.MathLibrary.degrees_to_radians(angle)
            frac = area_uniform_frac(frac_inner, frac_outer)
            x = vx + frac * BUILD_RADIUS * unreal.MathLibrary.cos(rad)
            y = vy + frac * BUILD_RADIUS * unreal.MathLibrary.sin(rad)
            if in_river_band(x, y) or in_village_band(x, y):
                continue
            if too_close(x, y, tree_positions, min_spacing):
                continue
            # Ignore every prop placed so far, trees included: a candidate
            # spacing of min_spacing from every trunk center still isn't
            # guaranteed to clear a neighboring canopy's actual collision
            # radius, and without this the trace can hit that canopy (or a
            # building/lamp/bridge it happens to share a column with)
            # instead of the ground under it.
            z = ground_z(world, x, y, ignore_actors=placed_actors)
            if z is None:
                continue
            mesh_path = random.choice(TREE_POOL)
            yaw = random.uniform(0.0, 360.0)
            scale = random.uniform(0.7, 1.05)
            tree = spawn_mesh(mesh_path, x, y, z, yaw, scale, folder=folder)
            if tree is not None:
                ground_to(tree, z)
                placed_count += 1
                tree_positions.append((x, y))
                placed_actors.append(tree)
        return placed_count

    tree_count = scatter_trees(0.0, 360.0, RING_INNER_FRACTION, RING_OUTER_FRACTION,
                               MIN_TREE_SPACING, NUM_TREES)
    dense_zone_count = scatter_trees(
        DENSE_ZONE_ANGLE_MIN, DENSE_ZONE_ANGLE_MAX,
        DENSE_ZONE_INNER_FRACTION, DENSE_ZONE_OUTER_FRACTION,
        DENSE_ZONE_MIN_SPACING, NUM_DENSE_ZONE_TREES, folder="PhotoVillage/Trees/DenseZone")
    tree_count += dense_zone_count

    foliage_count = 0
    attempts = 0
    while foliage_count < NUM_FOLIAGE and attempts < NUM_FOLIAGE * 6:
        attempts += 1
        angle = random.uniform(0.0, 360.0)
        rad = unreal.MathLibrary.degrees_to_radians(angle)
        frac = area_uniform_frac(RING_INNER_FRACTION - 0.08, RING_OUTER_FRACTION)
        x = vx + frac * BUILD_RADIUS * unreal.MathLibrary.cos(rad)
        y = vy + frac * BUILD_RADIUS * unreal.MathLibrary.sin(rad)
        if in_river_band(x, y) or in_village_band(x, y):
            continue
        # Same reasoning as the tree loop: a foliage clump can otherwise
        # land under a tree's canopy, inside another foliage clump, or
        # against any other already-placed prop, and get grounded to that
        # instead of the terrain under it.
        z = ground_z(world, x, y, ignore_actors=placed_actors)
        if z is None:
            continue
        mesh_path = random.choice(GROUND_FOLIAGE)
        yaw = random.uniform(0.0, 360.0)
        scale = random.uniform(0.7, 1.2)
        clump = spawn_mesh(mesh_path, x, y, z, yaw, scale, folder="PhotoVillage/Foliage")
        if clump is not None:
            ground_to(clump, z)
            foliage_count += 1
            placed_actors.append(clump)

    # --- Atmosphere -------------------------------------------------------
    fog_count = 0
    fog_actor = spawn_class_actor(FOG_AREA_BP, river_x, vy, river_z + 150.0,
                                   folder="PhotoVillage/Atmosphere")
    if fog_actor is not None:
        fog_count = 1

    # Park the editor viewport camera behind the player start, looking the
    # same direction the player will, so opening this level shows the scene.
    camera_behind = 800.0
    camera_yaw_rad = unreal.MathLibrary.degrees_to_radians(facing_yaw)
    camera_x = player_x - camera_behind * unreal.MathLibrary.cos(camera_yaw_rad)
    camera_y = arrival_house_y - camera_behind * unreal.MathLibrary.sin(camera_yaw_rad)
    camera_z = (ground_z(world, camera_x, camera_y) or player_z) + 500.0
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(
        unreal.Vector(camera_x, camera_y, camera_z), unreal.Rotator(roll=0.0, pitch=-20.0, yaw=facing_yaw)
    )

    level_subsystem.save_current_level()

    unreal.log(
        "L_PhotoVillage built on MountainRange at site ({:.0f},{:.0f}): {} trees ({} in dense zone), "
        "{} foliage clumps, {} fog volume, river half-width {:.0f}".format(
            vx, vy, tree_count, dense_zone_count, foliage_count, fog_count, river_half_width))


main()
unreal.SystemLibrary.quit_editor()
