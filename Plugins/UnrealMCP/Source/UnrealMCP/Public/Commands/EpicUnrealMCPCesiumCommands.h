#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Cesium for Unreal MCP commands.
 *
 * UE 5.7 Notes:
 * - Cesium for Unreal currently tracks UE 5.0–5.5 officially. UE 5.7 typically
 *   requires building from source or using a community-supported pre-release.
 * - All editor operations run on GameThread via AsyncTask from EpicUnrealMCPBridge.
 *
 * Detection Strategy:
 * - Check IPluginManager for CesiumForUnreal plugin descriptor.
 * - Check FModuleManager for CesiumRuntime module load state.
 * - When unavailable, every command returns success=false with an actionable
 *   `error` and `hint` that tells the AI exactly what to do next.
 *
 * The Build.cs DOES NOT depend on CesiumForUnreal so this plugin builds even
 * when Cesium is not installed. All Cesium-specific access is reflection-free
 * runtime probing for the PoC; deeper integration (Tileset spawn, georeference
 * conversion) requires adding Cesium as a public dependency in a follow-up.
 */
class UNREALMCP_API FEpicUnrealMCPCesiumCommands
{
public:
	FEpicUnrealMCPCesiumCommands();

	/** Dispatch a single Cesium command. */
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// ----- Implemented PoC handlers ---------------------------------------
	TSharedPtr<FJsonObject> HandleCesiumCheckPlugin(const TSharedPtr<FJsonObject>& Params);

	// ----- Stub handlers (return actionable "not_implemented" envelope) ---
	TSharedPtr<FJsonObject> HandleCesiumSetupGeoreference(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCesiumAddTileset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCesiumPlaceActorAtGeolocation(const TSharedPtr<FJsonObject>& Params);

	// ----- Helpers --------------------------------------------------------

	/** True when the CesiumForUnreal plugin descriptor is found and enabled. */
	static bool IsCesiumPluginAvailable();

	/** Resolve plugin display name + version + enabled state for diagnostics. */
	static TSharedPtr<FJsonObject> CollectCesiumDiagnostics();

	/** Build a standard "Cesium plugin missing" failure response with actionable hint. */
	static TSharedPtr<FJsonObject> MakeCesiumUnavailableResponse(const FString& CommandName);

	/** Build a standard "feature not yet wired" envelope (plugin present but PoC depth limited). */
	static TSharedPtr<FJsonObject> MakeCesiumNotImplementedResponse(const FString& CommandName);
};
