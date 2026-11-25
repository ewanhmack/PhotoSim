"""
Run inside the editor's own Python console (Window > Developer Tools > Output Log, then
`py "Content/Python/place_patrol_and_objectives.py"`), with L_PhotoVillage already open - this
spawns into the currently open level, it does not load one.

Drops a handful of PatrolNPC and PhotoObjectiveArea actors in a loose cluster around the level's
PlayerStart. Nothing is positioned for real - move them to wherever they should actually live, then
Ctrl+S when happy. Re-running this adds another batch rather than replacing the last one, so don't
run it twice without cleaning up the first set first.
"""

import unreal

NUM_PATROL_NPCS = 3
NUM_OBJECTIVES = 3

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def find_player_start():
    for actor in actor_subsystem.get_all_level_actors():
        if isinstance(actor, unreal.PlayerStart):
            return actor
    return None


def main():
    player_start = find_player_start()
    if player_start is None:
        unreal.log_error("No PlayerStart found in the current level - open L_PhotoVillage first.")
        return

    origin = player_start.get_actor_location()

    patrol_npc_class = unreal.load_class(None, "/Script/PhotoSim.PatrolNPC")
    objective_area_class = unreal.load_class(None, "/Script/PhotoSim.PhotoObjectiveArea")

    npc_offsets = [
        unreal.Vector(300, 0, 0),
        unreal.Vector(0, 300, 0),
        unreal.Vector(-300, 200, 0),
    ]
    for i in range(NUM_PATROL_NPCS):
        offset = npc_offsets[i % len(npc_offsets)]
        location = origin + offset
        npc = actor_subsystem.spawn_actor_from_class(patrol_npc_class, location)
        npc.set_actor_label("PatrolNPC_{}".format(i + 1))
        # A small default square route (4 checkpoints, not just 2) so it's obviously patrolling
        # between distinct points rather than pacing a single line - reposition freely, PatrolPoints
        # accepts any number of waypoints.
        npc.set_editor_property("patrol_points", [
            unreal.Vector(0, 0, 0),
            unreal.Vector(300, 0, 0),
            unreal.Vector(300, 300, 0),
            unreal.Vector(0, 300, 0),
        ])

    objective_offsets = [
        unreal.Vector(200, -300, 0),
        unreal.Vector(-200, -300, 0),
        unreal.Vector(0, -500, 0),
    ]
    for i in range(NUM_OBJECTIVES):
        offset = objective_offsets[i % len(objective_offsets)]
        location = origin + offset
        area = actor_subsystem.spawn_actor_from_class(objective_area_class, location)
        area.set_actor_label("PhotoObjective_{}".format(i + 1))
        area.photo_subject.set_editor_property("display_name", "Objective {}".format(i + 1))

    unreal.log("Placed {} PatrolNPC and {} PhotoObjectiveArea actor(s) near the PlayerStart. Reposition and save when ready.".format(
        NUM_PATROL_NPCS, NUM_OBJECTIVES))


main()
