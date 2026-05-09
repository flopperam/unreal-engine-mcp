import json
import os
import time
import traceback

import unreal


SAVED_DIR = os.path.join(unreal.Paths.project_saved_dir(), "VerticalTest")
REPORT_PATH = os.path.join(SAVED_DIR, "pie_validation_report.json")


report = {
    "success": False,
    "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S"),
    "errors": [],
    "data": {},
}


def add_error(message):
    unreal.log_error("[VT PIE] " + str(message))
    report["errors"].append(str(message))


def write_report():
    os.makedirs(SAVED_DIR, exist_ok=True)
    report["success"] = len(report["errors"]) == 0 and bool(report["data"].get("cleared"))
    with open(REPORT_PATH, "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=2, ensure_ascii=False)


def get_game_world():
    try:
        subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = subsystem.get_game_world()
        if world:
            return world
    except Exception:
        pass
    try:
        return unreal.EditorLevelLibrary.get_game_world()
    except Exception:
        return None


def get_prop(obj, names, default=None):
    for name in names:
        try:
            return obj.get_editor_property(name)
        except Exception:
            pass
    return default


def call_method(obj, names, *args):
    for name in names:
        try:
            method = getattr(obj, name)
            return method(*args)
        except Exception:
            pass
    raise RuntimeError(f"None of the methods exist or succeeded: {names}")


def main():
    try:
        world = get_game_world()
        if not world:
            add_error("No PIE game world available")
            write_report()
            return

        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not pawn:
            add_error("No player pawn was spawned in PIE")
            write_report()
            return

        game_state = None
        try:
            game_state = unreal.GameplayStatics.get_game_state(world)
        except Exception:
            pass
        if not game_state:
            vt_game_state_class = unreal.load_class(None, "/Script/FlopperamUnrealMCP.VTGameState")
            if vt_game_state_class:
                states = list(unreal.GameplayStatics.get_all_actors_of_class(world, vt_game_state_class))
                if states:
                    game_state = states[0]
        if not game_state:
            add_error("No GameState in PIE world")
            write_report()
            return

        core_class = unreal.load_class(None, "/Script/FlopperamUnrealMCP.VTCorePickup")
        gate_class = unreal.load_class(None, "/Script/FlopperamUnrealMCP.VTExitGate")
        if not core_class or not gate_class:
            add_error("Vertical test native actor classes are unavailable")
            write_report()
            return

        cores = list(unreal.GameplayStatics.get_all_actors_of_class(world, core_class))
        gates = list(unreal.GameplayStatics.get_all_actors_of_class(world, gate_class))
        if len(cores) != 3:
            add_error(f"Expected 3 core actors, found {len(cores)}")
        if len(gates) != 1:
            add_error(f"Expected 1 exit gate actor, found {len(gates)}")

        collected_results = []
        for core in cores:
            try:
                pawn.set_actor_location(core.get_actor_location(), False, True)
            except Exception:
                pass
            result = call_method(core, ["try_collect", "TryCollect"], pawn)
            collected_results.append(bool(result))

        gate_unlocked = bool(get_prop(game_state, ["bGateUnlocked", "b_gate_unlocked"], False))
        if not gate_unlocked:
            add_error("Gate did not unlock after collecting cores")

        clear_result = bool(call_method(game_state, ["complete_run", "CompleteRun"]))
        cleared = bool(get_prop(game_state, ["bCleared", "b_cleared"], False))
        collected_count = int(get_prop(game_state, ["CollectedCores", "collected_cores"], 0) or 0)
        total_count = int(get_prop(game_state, ["TotalCores", "total_cores"], 0) or 0)
        clear_time = float(get_prop(game_state, ["ClearTimeSeconds", "clear_time_seconds"], 0.0) or 0.0)

        if not clear_result and not cleared:
            add_error("CompleteRun did not enter the cleared state")
        if collected_count != 3 or total_count != 3:
            add_error(f"Unexpected core count after PIE validation: {collected_count}/{total_count}")

        report["data"] = {
            "pawn_class": pawn.get_class().get_name(),
            "cores_found": len(cores),
            "collected_results": collected_results,
            "gate_unlocked": gate_unlocked,
            "complete_run_returned": clear_result,
            "cleared": cleared,
            "collected_cores": collected_count,
            "total_cores": total_count,
            "clear_time_seconds": clear_time,
        }
        write_report()
    except Exception:
        add_error(traceback.format_exc())
        write_report()


main()
