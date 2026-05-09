#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UExponentialHeightFogComponent;
class ULightComponent;
class UPointLightComponent;
class URectLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class USpotLightComponent;

/**
 * Handler class for Lighting and Atmosphere MCP commands.
 * Controls actor-level light properties, sky/atmosphere/fog, and production lighting pipeline.
 */
class FEpicUnrealMCPLightingAtmosphereCommands
{
public:
	FEpicUnrealMCPLightingAtmosphereCommands();
	~FEpicUnrealMCPLightingAtmosphereCommands();

	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// --- Phase 1: Basic Light Property Control ---
	TSharedPtr<FJsonObject> HandleSetLightIntensity(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetLightColor(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetLightTemperature(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetLightMobility(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetLightShadowEnabled(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetLightShadowBias(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetLightContactShadows(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetLightVolumetricScattering(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetLightAttenuationRadius(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetLightConeAngles(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetLightSourceRadius(const TSharedPtr<FJsonObject>& Params);

	// --- Phase 2: Advanced Light Properties ---
	TSharedPtr<FJsonObject> HandleSetLightIesProfile(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetLightChannel(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetRectLightProperties(const TSharedPtr<FJsonObject>& Params);

	// --- Phase 3: Sky & Atmosphere ---
	TSharedPtr<FJsonObject> HandleSetSkyLightProperties(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetSkyAtmosphereProperties(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetHeightFogProperties(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetVolumetricFog(const TSharedPtr<FJsonObject>& Params);

	// --- Phase 4: Sun, Environment, Reflections ---
	TSharedPtr<FJsonObject> HandleSetDirectionalLightAsSun(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetSunPosition(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateHdriBackdrop(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateReflectionCapture(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetReflectionCaptureSettings(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleBuildReflectionCaptures(const TSharedPtr<FJsonObject>& Params);

	// --- Phase 5: Production Pipeline ---
	TSharedPtr<FJsonObject> HandleCreateLightmassImportanceVolume(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleBuildLighting(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetLightingScenario(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetMegaliights(const TSharedPtr<FJsonObject>& Params);

	// Helpers
	AActor* FindActorByName(UWorld* World, const FString& ActorName);
	ULightComponent* GetLightComponent(AActor* Actor, FString& OutError);
	UPointLightComponent* GetPointLightComponent(AActor* Actor, FString& OutError);
	USpotLightComponent* GetSpotLightComponent(AActor* Actor, FString& OutError);
	URectLightComponent* GetRectLightComponent(AActor* Actor, FString& OutError);
	USkyLightComponent* GetSkyLightComponent(AActor* Actor, FString& OutError);
	USkyAtmosphereComponent* GetSkyAtmosphereComponent(AActor* Actor, FString& OutError);
	UExponentialHeightFogComponent* GetHeightFogComponent(AActor* Actor, FString& OutError);
};
