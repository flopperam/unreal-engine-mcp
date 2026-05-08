#!/usr/bin/env python3
"""
Test script for Gameplay Framework commands.
Tests all 26 gameplay framework MCP commands.
"""

import json
import sys
from pathlib import Path

# Add the project root to the path
sys.path.insert(0, "c:/development/unreal-engine-mcp/Python")

from unreal_mcp_server_advanced import get_unreal_connection


PROJECT_DIR = Path(__file__).resolve().parents[1] / "FlopperamUnrealMCP"


def send_command(command_type, params=None):
    """Send a command to Unreal Engine via MCP."""
    if params is None:
        params = {}
    
    conn = get_unreal_connection()
    if not conn:
        print("No connection to Unreal Engine for command: " + command_type)
        return None
    
    try:
        response = conn.send_command(command_type, params)
        return response
    except Exception as e:
        print("Error sending " + command_type + ": " + str(e))
        return None


def test_command(name, command_type, params=None):
    """Test a single command and print results."""
    if params is None:
        params = {}
    
    print("\n" + "="*60)
    print("Testing: " + name)
    print("Command: " + command_type)
    print("Params: " + json.dumps(params, indent=2))
    print("-"*60)
    
    result = send_command(command_type, params)
    
    if result is None:
        print("FAILED - No response")
        return False
    
    status = result.get("status")
    if status == "success" or result.get("success") is True:
        print("SUCCESS")
        result_data = result.get("result", result)
        print("Result: " + json.dumps(result_data, indent=2))
        return True
    else:
        print("FAILED - Status: " + str(status or "error"))
        print("Error: " + result.get("error", "Unknown error"))
        return False


def read_ini_value(path, section, key):
    current_section = None
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith(";"):
            continue
        if stripped.startswith("[") and stripped.endswith("]"):
            current_section = stripped[1:-1]
            continue
        if current_section == section and stripped.startswith(key + "="):
            return stripped.split("=", 1)[1].strip()
    return None


def test_ini_value(name, path, section, key, expected):
    print("\n" + "="*60)
    print("Testing: " + name)
    print("File: " + str(path))
    print("-"*60)

    if not path.exists():
        print("FAILED - Config file does not exist")
        return False

    actual = read_ini_value(path, section, key)
    if actual == expected:
        print("SUCCESS")
        print("Value: " + actual)
        return True

    print("FAILED - Expected: " + expected)
    print("Actual: " + str(actual))
    return False


def main():
    """Run all Gameplay Framework command tests."""
    print("=" * 70)
    print(" GAMEPLAY FRAMEWORK COMMAND TEST SUITE")
    print("=" * 70)
    print("\nMake sure:")
    print("1. Unreal Editor is running with the UnrealMCP plugin enabled")
    print("2. The MCP server is connected on port 55557")
    print("3. You have a project open")
    print("\n" + "=" * 70)
    
    results = []
    
    # --- GameMode Commands ---
    print("\n\n" + "=" * 70)
    print(" GAME MODE COMMANDS")
    print("=" * 70)
    
    results.append(test_command(
        "Create GameMode Blueprint",
        "create_gamemode_blueprint",
        {"name": "BP_TestGameMode"}
    ))
    
    results.append(test_command(
        "Create GameMode C++ Class",
        "create_gamemode_cpp_class",
        {"name": "TestGameModeCPP"}
    ))
    
    results.append(test_command(
        "Set Default GameMode",
        "set_default_gamemode",
        {"gamemode_path": "/Game/GameModes/BP_TestGameMode"}
    ))

    results.append(test_ini_value(
        "DefaultEngine.ini GlobalDefaultGameMode",
        PROJECT_DIR / "Config" / "DefaultEngine.ini",
        "/Script/EngineSettings.GameMapsSettings",
        "GlobalDefaultGameMode",
        "/Game/GameModes/BP_TestGameMode.BP_TestGameMode_C"
    ))
    
    # --- GameState Commands ---
    print("\n\n" + "=" * 70)
    print(" GAME STATE COMMANDS")
    print("=" * 70)
    
    results.append(test_command(
        "Create GameState",
        "create_gamestate",
        {"name": "BP_TestGameState"}
    ))
    
    # --- PlayerState Commands ---
    print("\n\n" + "=" * 70)
    print(" PLAYER STATE COMMANDS")
    print("=" * 70)
    
    results.append(test_command(
        "Create PlayerState",
        "create_playerstate",
        {"name": "BP_TestPlayerState"}
    ))
    
    # --- Controller Commands ---
    print("\n\n" + "=" * 70)
    print(" CONTROLLER COMMANDS")
    print("=" * 70)
    
    results.append(test_command(
        "Create PlayerController",
        "create_playercontroller",
        {"name": "BP_TestPlayerController"}
    ))
    
    results.append(test_command(
        "Create AIController",
        "create_aicontroller",
        {"name": "BP_TestAIController"}
    ))
    
    # --- Pawn / Character Commands ---
    print("\n\n" + "=" * 70)
    print(" PAWN / CHARACTER COMMANDS")
    print("=" * 70)
    
    results.append(test_command(
        "Create Pawn",
        "create_pawn",
        {"name": "BP_TestPawn"}
    ))
    
    results.append(test_command(
        "Create Character",
        "create_character",
        {
            "name": "BP_TestCharacter",
            "add_movement_component": True,
            "max_walk_speed": 800.0
        }
    ))
    
    results.append(test_command(
        "Set Default Pawn",
        "set_default_pawn",
        {
            "pawn_path": "/Game/Characters/BP_TestCharacter",
            "gamemode_path": "/Game/GameModes/BP_TestGameMode"
        }
    ))
    
    # --- HUD / Spectator Commands ---
    print("\n\n" + "=" * 70)
    print(" HUD / SPECTATOR COMMANDS")
    print("=" * 70)
    
    results.append(test_command(
        "Set HUD Class",
        "set_hud_class",
        {
            "name": "BP_TestHUD",
            "gamemode_path": "/Game/GameModes/BP_TestGameMode"
        }
    ))
    
    results.append(test_command(
        "Set Spectator Pawn",
        "set_spectator_pawn",
        {
            "name": "BP_TestSpectator",
            "gamemode_path": "/Game/GameModes/BP_TestGameMode"
        }
    ))
    
    # --- Player Start / Spawn Commands ---
    print("\n\n" + "=" * 70)
    print(" PLAYER START / SPAWN COMMANDS")
    print("=" * 70)
    
    results.append(test_command(
        "Place Player Start",
        "place_player_start",
        {
            "location": {"value": [0.0, 0.0, 100.0]},
            "rotation": {"value": [0.0, 0.0, 0.0]},
            "tag": "PlayerSpawn",
            "name": "PlayerStart_Main"
        }
    ))
    
    results.append(test_command(
        "Set Spawn Rules",
        "set_spawn_rules",
        {"spawn_method": "PlayerStart"}
    ))
    
    results.append(test_command(
        "Set Possess Rules",
        "set_possess_rules",
        {
            "possess_method": "Auto",
            "auto_possess": True
        }
    ))
    
    # --- Camera Commands ---
    print("\n\n" + "=" * 70)
    print(" CAMERA COMMANDS")
    print("=" * 70)
    
    results.append(test_command(
        "Set Camera Manager",
        "set_camera_manager",
        {
            "name": "BP_TestCameraManager",
            "playercontroller_path": "/Game/Controllers/BP_TestPlayerController"
        }
    ))
    
    results.append(test_command(
        "Setup Camera Component",
        "setup_camera_component",
        {
            "blueprint_name": "BP_TestCharacter",
            "field_of_view": 100.0,
            "use_pawn_control_rotation": True,
            "offset": {"value": [0.0, 0.0, 160.0]}
        }
    ))
    
    results.append(test_command(
        "Setup Spring Arm",
        "setup_spring_arm",
        {
            "blueprint_name": "BP_TestCharacter",
            "target_arm_length": 400.0,
            "socket_offset": {"value": [0.0, 50.0, 80.0]},
            "use_pawn_control_rotation": True,
            "inherit_pitch": True,
            "inherit_yaw": True,
            "inherit_roll": False,
            "do_collision_test": True
        }
    ))
    
    # --- SaveGame Commands ---
    print("\n\n" + "=" * 70)
    print(" SAVE GAME COMMANDS")
    print("=" * 70)
    
    results.append(test_command(
        "Create SaveGame Class",
        "create_savegame_class",
        {"name": "BP_TestSaveGame"}
    ))
    
    # --- GameInstance Commands ---
    print("\n\n" + "=" * 70)
    print(" GAME INSTANCE COMMANDS")
    print("=" * 70)
    
    results.append(test_command(
        "Create GameInstance",
        "create_gameinstance",
        {"name": "BP_TestGameInstance"}
    ))
    
    # --- Subsystem Commands ---
    print("\n\n" + "=" * 70)
    print(" SUBSYSTEM COMMANDS")
    print("=" * 70)
    
    results.append(test_command(
        "Create GameInstance Subsystem",
        "create_gameinstance_subsystem",
        {"name": "MyGameInstanceSubsystem"}
    ))
    
    results.append(test_command(
        "Create World Subsystem",
        "create_world_subsystem",
        {"name": "MyWorldSubsystem"}
    ))
    
    results.append(test_command(
        "Create LocalPlayer Subsystem",
        "create_localplayer_subsystem",
        {"name": "MyLocalPlayerSubsystem"}
    ))
    
    # --- Gameplay Tags Commands ---
    print("\n\n" + "=" * 70)
    print(" GAMEPLAY TAGS COMMANDS")
    print("=" * 70)
    
    results.append(test_command(
        "Setup Gameplay Tags",
        "setup_gameplay_tags",
        {
            "tags": [
                "State.Alive",
                "State.Dead",
                "Role.Player",
                "Role.Enemy",
                "Ability.Jump",
                "Ability.Sprint"
            ]
        }
    ))
    
    results.append(test_command(
        "Add Gameplay Tag",
        "add_gameplay_tag",
        {
            "tag": "State.Stunned",
            "comment": "Character is stunned"
        }
    ))
    
    results.append(test_command(
        "Create Gameplay Tag Query",
        "create_gameplay_tag_query",
        {
            "name": "IsAliveQuery",
            "query_type": "Any",
            "tags": ["State.Alive", "State.Invulnerable"]
        }
    ))
    
    # --- Summary ---
    print("\n\n" + "=" * 70)
    print(" TEST SUMMARY")
    print("=" * 70)
    
    total = len(results)
    passed = sum(1 for r in results if r)
    failed = total - passed
    
    print("\nTotal Tests: " + str(total))
    print("Passed: " + str(passed))
    print("Failed: " + str(failed))
    if total > 0:
        print("Success Rate: " + str(passed/total*100) + "%")
    
    if failed == 0:
        print("\nALL TESTS PASSED!")
    else:
        print("\n" + str(failed) + " test(s) failed. Check the output above for details.")
    
    return failed == 0


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
