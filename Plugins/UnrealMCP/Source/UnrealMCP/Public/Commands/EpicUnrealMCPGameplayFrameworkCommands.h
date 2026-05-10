#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Gameplay Framework MCP commands
 * Implements UE5 Gameplay Framework: GameMode, GameState, PlayerController, Pawn, Character, etc.
 */
class FEpicUnrealMCPGameplayFrameworkCommands
{
public:
    FEpicUnrealMCPGameplayFrameworkCommands();

    // Handle gameplay framework commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // --- GameMode ---
    TSharedPtr<FJsonObject> HandleCreateGameModeBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateGameModeCPPClass(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetDefaultGameMode(const TSharedPtr<FJsonObject>& Params);
    
    // --- GameState ---
    TSharedPtr<FJsonObject> HandleCreateGameState(const TSharedPtr<FJsonObject>& Params);
    
    // --- PlayerState ---
    TSharedPtr<FJsonObject> HandleCreatePlayerState(const TSharedPtr<FJsonObject>& Params);
    
    // --- Controllers ---
    TSharedPtr<FJsonObject> HandleCreatePlayerController(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateAIController(const TSharedPtr<FJsonObject>& Params);
    
    // --- Pawn / Character ---
    TSharedPtr<FJsonObject> HandleCreatePawn(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateCharacter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetDefaultPawn(const TSharedPtr<FJsonObject>& Params);
    
    // --- HUD / Spectator ---
    TSharedPtr<FJsonObject> HandleSetHUDClass(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetSpectatorPawn(const TSharedPtr<FJsonObject>& Params);
    
    // --- Player Start / Spawn ---
    TSharedPtr<FJsonObject> HandlePlacePlayerStart(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetSpawnRules(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetPossessRules(const TSharedPtr<FJsonObject>& Params);
    
    // --- Camera ---
    TSharedPtr<FJsonObject> HandleSetCameraManager(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetupCameraComponent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetupSpringArm(const TSharedPtr<FJsonObject>& Params);
    
    // --- SaveGame ---
    TSharedPtr<FJsonObject> HandleCreateSaveGameClass(const TSharedPtr<FJsonObject>& Params);

    // --- Save / Load ---
    TSharedPtr<FJsonObject> HandleSaveGameToSlot(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleLoadGameFromSlot(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteSaveSlot(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleHasSaveGame(const TSharedPtr<FJsonObject>& Params);
    
    // --- GameInstance ---
    TSharedPtr<FJsonObject> HandleCreateGameInstance(const TSharedPtr<FJsonObject>& Params);
    
    // --- Subsystems ---
    TSharedPtr<FJsonObject> HandleCreateGameInstanceSubsystem(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateWorldSubsystem(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateLocalPlayerSubsystem(const TSharedPtr<FJsonObject>& Params);
    
    // --- Gameplay Tags ---
    TSharedPtr<FJsonObject> HandleSetupGameplayTags(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddGameplayTag(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateGameplayTagQuery(const TSharedPtr<FJsonObject>& Params);
    
    // Helper functions
    UBlueprint* CreateBlueprintWithParent(const FString& BlueprintName, const FString& PackagePath, UClass* ParentClass);
    bool SetProjectSetting(const FString& SectionName, const FString& PropertyName, const FString& Value);
    FString GetProjectSetting(const FString& SectionName, const FString& PropertyName);
    bool UpdateDefaultEngineIni(const FString& SectionName, const FString& PropertyName, const FString& Value);
    bool UpdateDefaultGameIni(const FString& SectionName, const FString& PropertyName, const FString& Value);
};