"""
Read-only diagnostic - run inside the editor's Python console (Window > Developer Tools >
Output Log, then `py "Content/Python/diagnose_landscape_hole.py"`) with L_PhotoVillage open.
Does not spawn, edit or save anything.

Part 1: lists the Landscape actor(s) and anything with "lake"/"water"/"boat"/"diorama" in its
name, with locations and bounds, so there's a concrete coordinate anchor for where the lake
actually is.

Part 2: traces straight down through a grid of points centred on whatever lake/water actor it
found (or the landscape's own centre if none), and reports every point where the trace found no
ground - that's the hole's actual extent, in world-space coordinates, rather than guessing from
a screenshot.
"""

import re
import unreal

GRID_HALF_EXTENT = 2500.0   # scan +/- this many units around the centre point, on X and Y
GRID_STEP = 100.0           # spacing between sample points - smaller is slower but finer

# Read directly off the character's Transform in the World Outliner while mid-fall through the
# actual hole (Shift+F1 during PIE, no pause) - ground truth, not inferred from a camera angle.
OVERRIDE_CENTRE = unreal.Vector(-26414.968236, -14548.358098, 0.0)

_VECTOR_FIELD_RE = re.compile(r"X=(-?[\d.]+),Y=(-?[\d.]+),Z=(-?[\d.]+)")

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def describe_actor(actor):
    origin, extent = actor.get_actor_bounds(only_colliding_components=False)
    return "{} ({})  location={}  bounds_origin={} bounds_extent={}".format(
        actor.get_actor_label(), actor.get_class().get_name(),
        actor.get_actor_location(), origin, extent)


def find_landscape_actors():
    return [a for a in actor_subsystem.get_all_level_actors() if isinstance(a, unreal.Landscape)]


def get_viewport_camera_transform():
    """(location, rotation) of the editor's active viewport camera. (None, None) if it can't be read."""
    try:
        subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        result = subsystem.get_level_viewport_camera_info()
    except Exception as exc:
        unreal.log_warning("Could not call get_level_viewport_camera_info() ({}).".format(exc))
        return None, None

    location = None
    rotation = None
    items = result if isinstance(result, tuple) else (result,)
    for item in items:
        if isinstance(item, unreal.Vector):
            location = item
        elif isinstance(item, unreal.Rotator):
            rotation = item

    if location is None or rotation is None:
        unreal.log_warning("get_level_viewport_camera_info() returned something unexpected ({}): {}".format(type(result), result))
    return location, rotation


def find_lake_actors():
    keywords = ("lake", "water", "boat", "diorama")
    found = []
    for a in actor_subsystem.get_all_level_actors():
        label = a.get_actor_label().lower()
        if any(k in label for k in keywords):
            found.append(a)
    return found


def do_trace(world, x, y, trace_top_z, trace_bottom_z):
    start = unreal.Vector(x, y, trace_top_z)
    end = unreal.Vector(x, y, trace_bottom_z)
    return unreal.SystemLibrary.line_trace_single(
        world, start, end,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,  # Visibility channel
        False, [], unreal.DrawDebugTrace.NONE, True)


def is_blocking_hit(hit):
    # Neither direct attribute access nor get_editor_property("blocking_hit"/"bBlockingHit")
    # resolves on this struct in this engine build - export_text()'s exact format is confirmed
    # by direct observation (see conversation), so read it from there instead of guessing again.
    # Some trace variants return None outright on a miss rather than a HitResult with
    # bBlockingHit=False - treat that as a miss too.
    if hit is None:
        return False
    return "bBlockingHit=True" in hit.export_text()


def hit_impact_point(hit):
    """ImpactPoint pulled straight out of export_text() - same reasoning as is_blocking_hit."""
    text = hit.export_text()
    match = re.search(r"ImpactPoint=\(" + _VECTOR_FIELD_RE.pattern + r"\)", text)
    if not match:
        return None
    x, y, z = (float(v) for v in match.groups())
    return unreal.Vector(x, y, z)


def scan_for_holes(world, centre, trace_top_z, trace_bottom_z):
    misses = []
    steps = int(GRID_HALF_EXTENT * 2 / GRID_STEP) + 1
    for ix in range(steps):
        x = centre.x - GRID_HALF_EXTENT + ix * GRID_STEP
        for iy in range(steps):
            y = centre.y - GRID_HALF_EXTENT + iy * GRID_STEP
            hit = do_trace(world, x, y, trace_top_z, trace_bottom_z)
            if not is_blocking_hit(hit):
                misses.append(unreal.Vector(x, y, 0))
    return misses


def main():
    unreal.log("=== Landscape actors ===")
    landscapes = find_landscape_actors()
    if not landscapes:
        unreal.log_warning("No Landscape actor found in the current level.")
    for a in landscapes:
        unreal.log(describe_actor(a))

    unreal.log("=== Lake / water related actors ===")
    lake_actors = find_lake_actors()
    for a in lake_actors:
        unreal.log(describe_actor(a))
    if not lake_actors:
        unreal.log_warning("Nothing matched lake/water/boat/diorama by name.")

    if not landscapes:
        unreal.log_error("Can't scan for holes without a Landscape actor - stopping here.")
        return

    landscape = landscapes[0]
    ls_origin, ls_extent = landscape.get_actor_bounds(only_colliding_components=False)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

    centre = OVERRIDE_CENTRE
    if centre is not None:
        unreal.log("Using OVERRIDE_CENTRE (the character's real fall-through position): {}".format(centre))

    camera_location, camera_rotation = get_viewport_camera_transform()
    if centre is None and camera_location is not None and camera_rotation is not None:
        forward = unreal.MathLibrary.get_forward_vector(camera_rotation)
        look_end = camera_location + forward * 100000.0
        unreal.log("=== Tracing along the camera's look direction ===")
        look_hit = unreal.SystemLibrary.line_trace_single(
            world, camera_location, look_end,
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
        if is_blocking_hit(look_hit):
            impact = hit_impact_point(look_hit)
            unreal.log("Camera's look direction hits something at {} - full detail: {}".format(impact, look_hit.export_text()))
            centre = impact
        else:
            unreal.log_warning("Nothing in the camera's look direction for 100000 units - you're looking through empty "
                                "space. This is itself the hole.")
            # No surface to anchor on, so estimate where the look ray crosses roughly ground level
            # (Z=0) instead - still far better than the raw camera position for an angled view.
            if forward.z < -0.001:
                t = (0.0 - camera_location.z) / forward.z
                if t > 0:
                    centre = camera_location + forward * t
                    unreal.log("Centring the scan where the look direction crosses Z=0: {}".format(centre))

    if centre is None and lake_actors:
        centre = lake_actors[0].get_actor_location()
        unreal.log("No usable camera look direction - centring on {} instead.".format(lake_actors[0].get_actor_label()))
    elif centre is None:
        centre = ls_origin

    trace_top = ls_origin.z + ls_extent.z + 2000.0
    trace_bottom = ls_origin.z - ls_extent.z - 2000.0

    unreal.log("=== Checking the hit component's own collision response (no trace involved) ===")
    vis_hit = do_trace(world, centre.x, centre.y, trace_top, trace_bottom)
    unreal.log("Visibility channel: {}".format("HIT" if is_blocking_hit(vis_hit) else "no hit"))

    hit_text = vis_hit.export_text() if is_blocking_hit(vis_hit) else ""
    component_match = re.search(r"Component=\"[^']*'([^']+)'\"", hit_text)
    if component_match:
        component_path = component_match.group(1)
        unreal.log("Hit component path: {}".format(component_path))
        component = unreal.load_object(None, component_path)
        if component is not None:
            profile_name = component.get_collision_profile_name()
            pawn_response = component.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)
            visibility_response = component.get_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY)
            unreal.log("Collision profile: {}".format(profile_name))
            unreal.log("Response to ECC_Pawn:      {}".format(pawn_response))
            unreal.log("Response to ECC_Visibility: {}".format(visibility_response))
            if str(pawn_response) != str(visibility_response):
                unreal.log_warning("CONFIRMED: this component responds differently to Pawn than to Visibility - "
                                    "that's a collision-response gap on the landscape/water layer, not a geometric "
                                    "hole. Fix: check this component's (or the Landscape's) Collision Preset / "
                                    "response to the Pawn channel in its Details panel.")
        else:
            unreal.log_warning("Could not load the hit component via its path: {}".format(component_path))
    else:
        unreal.log_warning("Could not find a hit at the centre point to inspect - trying the grid scan anyway.")

    unreal.log("=== Scanning a {}x{} unit grid around {} (step {}) ===".format(
        int(GRID_HALF_EXTENT * 2), int(GRID_HALF_EXTENT * 2), centre, GRID_STEP))

    misses = scan_for_holes(world, centre, trace_top, trace_bottom)

    if not misses:
        unreal.log("No gaps found in the scanned area - the hole (if any) is outside this {}x{} window, "
                    "or narrower than the {} unit step. Widen GRID_HALF_EXTENT or shrink GRID_STEP and re-run.".format(
                        int(GRID_HALF_EXTENT * 2), int(GRID_HALF_EXTENT * 2), GRID_STEP))
    else:
        unreal.log("=== {} point(s) with no ground underneath ===".format(len(misses)))
        for p in misses:
            unreal.log("  hole at X={:.0f} Y={:.0f}".format(p.x, p.y))

        xs = [p.x for p in misses]
        ys = [p.y for p in misses]
        unreal.log("Hole bounding box: X [{:.0f}, {:.0f}]  Y [{:.0f}, {:.0f}]".format(min(xs), max(xs), min(ys), max(ys)))


main()
