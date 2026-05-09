#include "Commands/EpicUnrealMCPLightingAtmosphereCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "EpicUnrealMCPBridge.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Light.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/RectLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SphereReflectionCapture.h"
#include "Engine/BoxReflectionCapture.h"
#include "Engine/ReflectionCapture.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Components/SphereReflectionCaptureComponent.h"
#include "Components/BoxReflectionCaptureComponent.h"
#include "Components/BrushComponent.h"
#include "Engine/TextureLightProfile.h"
#include "Engine/TextureCube.h"
#include "LightingBuildOptions.h"
#include "Lightmass/LightmassImportanceVolume.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"

FEpicUnrealMCPLightingAtmosphereCommands::FEpicUnrealMCPLightingAtmosphereCommands()
{
}

FEpicUnrealMCPLightingAtmosphereCommands::~FEpicUnrealMCPLightingAtmosphereCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPLightingAtmosphereCommands::*)(const TSharedPtr<FJsonObject>&);
	static const TMap<FString, Handler> Dispatch = {
		// Phase 1: Basic Light Property Control
		{TEXT("set_light_intensity"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightIntensity},
		{TEXT("set_light_color"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightColor},
		{TEXT("set_light_temperature"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightTemperature},
		{TEXT("set_light_mobility"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightMobility},
		{TEXT("set_light_shadow_enabled"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightShadowEnabled},
		{TEXT("set_light_shadow_bias"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightShadowBias},
		{TEXT("set_light_contact_shadows"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightContactShadows},
		{TEXT("set_light_volumetric_scattering"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightVolumetricScattering},
		{TEXT("set_light_attenuation_radius"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightAttenuationRadius},
		{TEXT("set_light_cone_angles"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightConeAngles},
		{TEXT("set_light_source_radius"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightSourceRadius},
		// Phase 2: Advanced Light Properties
		{TEXT("set_light_ies_profile"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightIesProfile},
		{TEXT("set_light_channel"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightChannel},
		{TEXT("set_rect_light_properties"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetRectLightProperties},
		// Phase 3: Sky & Atmosphere
		{TEXT("set_sky_light_properties"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetSkyLightProperties},
		{TEXT("set_sky_atmosphere_properties"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetSkyAtmosphereProperties},
		{TEXT("set_height_fog_properties"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetHeightFogProperties},
		{TEXT("set_volumetric_fog"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetVolumetricFog},
		// Phase 4: Sun, Environment, Reflections
		{TEXT("set_directional_light_as_sun"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetDirectionalLightAsSun},
		{TEXT("set_sun_position"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetSunPosition},
		{TEXT("create_hdri_backdrop"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleCreateHdriBackdrop},
		{TEXT("create_reflection_capture"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleCreateReflectionCapture},
		{TEXT("set_reflection_capture_settings"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetReflectionCaptureSettings},
		{TEXT("build_reflection_captures"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleBuildReflectionCaptures},
		// Phase 5: Production Pipeline
		{TEXT("create_lightmass_importance_volume"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleCreateLightmassImportanceVolume},
		{TEXT("build_lighting"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleBuildLighting},
		{TEXT("set_lighting_scenario"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightingScenario},
		{TEXT("set_megaliights"), &FEpicUnrealMCPLightingAtmosphereCommands::HandleSetMegaliights},
	};

	const Handler* H = Dispatch.Find(CommandType);
	if (H)
	{
		return (this->*(*H))(Params);
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown lighting/atmosphere command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

AActor* FEpicUnrealMCPLightingAtmosphereCommands::FindActorByName(UWorld* World, const FString& ActorName)
{
	if (!World) return nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetName() == ActorName)
		{
			return *It;
		}
	}
	return nullptr;
}

ULightComponent* FEpicUnrealMCPLightingAtmosphereCommands::GetLightComponent(AActor* Actor, FString& OutError)
{
	if (!Actor)
	{
		OutError = TEXT("Actor is null");
		return nullptr;
	}
	ULightComponent* Comp = Cast<ULightComponent>(Actor->GetComponentByClass(ULightComponent::StaticClass()));
	if (!Comp)
	{
		OutError = FString::Printf(TEXT("Actor '%s' does not have a LightComponent"), *Actor->GetName());
	}
	return Comp;
}

UPointLightComponent* FEpicUnrealMCPLightingAtmosphereCommands::GetPointLightComponent(AActor* Actor, FString& OutError)
{
	ULightComponent* LightComp = GetLightComponent(Actor, OutError);
	if (!LightComp) return nullptr;
	UPointLightComponent* Comp = Cast<UPointLightComponent>(LightComp);
	if (!Comp)
	{
		OutError = FString::Printf(TEXT("Actor '%s' does not have a PointLightComponent (or SpotLightComponent)"), *Actor->GetName());
	}
	return Comp;
}

USpotLightComponent* FEpicUnrealMCPLightingAtmosphereCommands::GetSpotLightComponent(AActor* Actor, FString& OutError)
{
	ULightComponent* LightComp = GetLightComponent(Actor, OutError);
	if (!LightComp) return nullptr;
	USpotLightComponent* Comp = Cast<USpotLightComponent>(LightComp);
	if (!Comp)
	{
		OutError = FString::Printf(TEXT("Actor '%s' does not have a SpotLightComponent"), *Actor->GetName());
	}
	return Comp;
}

URectLightComponent* FEpicUnrealMCPLightingAtmosphereCommands::GetRectLightComponent(AActor* Actor, FString& OutError)
{
	ULightComponent* LightComp = GetLightComponent(Actor, OutError);
	if (!LightComp) return nullptr;
	URectLightComponent* Comp = Cast<URectLightComponent>(LightComp);
	if (!Comp)
	{
		OutError = FString::Printf(TEXT("Actor '%s' does not have a RectLightComponent"), *Actor->GetName());
	}
	return Comp;
}

USkyLightComponent* FEpicUnrealMCPLightingAtmosphereCommands::GetSkyLightComponent(AActor* Actor, FString& OutError)
{
	if (!Actor)
	{
		OutError = TEXT("Actor is null");
		return nullptr;
	}
	USkyLightComponent* Comp = Cast<USkyLightComponent>(Actor->GetComponentByClass(USkyLightComponent::StaticClass()));
	if (!Comp)
	{
		OutError = FString::Printf(TEXT("Actor '%s' does not have a SkyLightComponent"), *Actor->GetName());
	}
	return Comp;
}

USkyAtmosphereComponent* FEpicUnrealMCPLightingAtmosphereCommands::GetSkyAtmosphereComponent(AActor* Actor, FString& OutError)
{
	if (!Actor)
	{
		OutError = TEXT("Actor is null");
		return nullptr;
	}
	USkyAtmosphereComponent* Comp = Cast<USkyAtmosphereComponent>(Actor->GetComponentByClass(USkyAtmosphereComponent::StaticClass()));
	if (!Comp)
	{
		OutError = FString::Printf(TEXT("Actor '%s' does not have a SkyAtmosphereComponent"), *Actor->GetName());
	}
	return Comp;
}

UExponentialHeightFogComponent* FEpicUnrealMCPLightingAtmosphereCommands::GetHeightFogComponent(AActor* Actor, FString& OutError)
{
	if (!Actor)
	{
		OutError = TEXT("Actor is null");
		return nullptr;
	}
	UExponentialHeightFogComponent* Comp = Cast<UExponentialHeightFogComponent>(Actor->GetComponentByClass(UExponentialHeightFogComponent::StaticClass()));
	if (!Comp)
	{
		OutError = FString::Printf(TEXT("Actor '%s' does not have an ExponentialHeightFogComponent"), *Actor->GetName());
	}
	return Comp;
}

// ---------------------------------------------------------------------------
// Phase 1: Basic Light Property Control
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightIntensity(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	double Intensity = 0.0;
	if (!Params->TryGetNumberField(TEXT("intensity"), Intensity))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'intensity' parameter"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	ULightComponent* LightComp = GetLightComponent(Actor, Error);
	if (!LightComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Light Intensity")));
	LightComp->Modify();
	LightComp->SetIntensity(static_cast<float>(Intensity));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetNumberField(TEXT("intensity"), Intensity);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightColor(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	const TArray<TSharedPtr<FJsonValue>>* ColorArray = nullptr;
	if (!Params->TryGetArrayField(TEXT("color"), ColorArray) || ColorArray->Num() < 3)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or invalid 'color' parameter. Expected [R, G, B]"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	ULightComponent* LightComp = GetLightComponent(Actor, Error);
	if (!LightComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FLinearColor NewColor(
		static_cast<float>((*ColorArray)[0]->AsNumber()),
		static_cast<float>((*ColorArray)[1]->AsNumber()),
		static_cast<float>((*ColorArray)[2]->AsNumber())
	);

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Light Color")));
	LightComp->Modify();
	LightComp->SetLightColor(NewColor);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	TArray<TSharedPtr<FJsonValue>> OutColor;
	OutColor.Add(MakeShared<FJsonValueNumber>(NewColor.R));
	OutColor.Add(MakeShared<FJsonValueNumber>(NewColor.G));
	OutColor.Add(MakeShared<FJsonValueNumber>(NewColor.B));
	Result->SetArrayField(TEXT("color"), OutColor);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightTemperature(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	double Temperature = 6500.0;
	Params->TryGetNumberField(TEXT("temperature"), Temperature);

	bool bUseTemperature = true;
	Params->TryGetBoolField(TEXT("enabled"), bUseTemperature);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	ULightComponent* LightComp = GetLightComponent(Actor, Error);
	if (!LightComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Light Temperature")));
	LightComp->Modify();
	LightComp->SetUseTemperature(bUseTemperature);
	LightComp->SetTemperature(static_cast<float>(Temperature));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetBoolField(TEXT("use_temperature"), bUseTemperature);
	Result->SetNumberField(TEXT("temperature"), Temperature);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightMobility(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	FString MobilityStr;
	if (!Params->TryGetStringField(TEXT("mobility"), MobilityStr) || MobilityStr.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'mobility' parameter. Use: Static, Stationary, Movable"));
	}

	EComponentMobility::Type Mobility = EComponentMobility::Stationary;
	if (MobilityStr == TEXT("Static")) Mobility = EComponentMobility::Static;
	else if (MobilityStr == TEXT("Stationary")) Mobility = EComponentMobility::Stationary;
	else if (MobilityStr == TEXT("Movable")) Mobility = EComponentMobility::Movable;
	else
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid mobility '%s'. Use: Static, Stationary, Movable"), *MobilityStr));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	ULightComponent* LightComp = GetLightComponent(Actor, Error);
	if (!LightComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Light Mobility")));
	LightComp->Modify();
	LightComp->SetMobility(Mobility);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("mobility"), MobilityStr);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightShadowEnabled(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	bool bEnabled = true;
	Params->TryGetBoolField(TEXT("enabled"), bEnabled);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	ULightComponent* LightComp = GetLightComponent(Actor, Error);
	if (!LightComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Light Shadow")));
	LightComp->Modify();
	LightComp->SetCastShadows(bEnabled);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetBoolField(TEXT("cast_shadows"), bEnabled);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightShadowBias(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	double Bias = 0.0;
	if (!Params->TryGetNumberField(TEXT("bias"), Bias))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'bias' parameter"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	ULightComponent* LightComp = GetLightComponent(Actor, Error);
	if (!LightComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Light Shadow Bias")));
	LightComp->Modify();
	LightComp->SetShadowBias(static_cast<float>(Bias));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetNumberField(TEXT("shadow_bias"), Bias);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightContactShadows(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	bool bEnabled = true;
	Params->TryGetBoolField(TEXT("enabled"), bEnabled);

	double Length = 0.0;
	Params->TryGetNumberField(TEXT("length"), Length);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	ULightComponent* LightComp = GetLightComponent(Actor, Error);
	if (!LightComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Light Contact Shadows")));
	LightComp->Modify();
	if (bEnabled && Length > 0.0)
	{
		LightComp->ContactShadowLength = static_cast<float>(Length);
	}
	else if (!bEnabled)
	{
		LightComp->ContactShadowLength = 0.0f;
	}
	LightComp->MarkRenderStateDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetBoolField(TEXT("contact_shadows_enabled"), bEnabled);
	Result->SetNumberField(TEXT("contact_shadow_length"), LightComp->ContactShadowLength);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightVolumetricScattering(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	bool bEnabled = true;
	Params->TryGetBoolField(TEXT("enabled"), bEnabled);

	double Intensity = 1.0;
	Params->TryGetNumberField(TEXT("intensity"), Intensity);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	ULightComponent* LightComp = GetLightComponent(Actor, Error);
	if (!LightComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Light Volumetric Scattering")));
	LightComp->Modify();
	LightComp->SetVolumetricScatteringIntensity(bEnabled ? static_cast<float>(Intensity) : 0.0f);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetNumberField(TEXT("volumetric_scattering_intensity"), LightComp->VolumetricScatteringIntensity);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightAttenuationRadius(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	double Radius = 0.0;
	if (!Params->TryGetNumberField(TEXT("radius"), Radius))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'radius' parameter"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	UPointLightComponent* PointComp = GetPointLightComponent(Actor, Error);
	if (!PointComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Light Attenuation Radius")));
	PointComp->Modify();
	PointComp->SetAttenuationRadius(static_cast<float>(Radius));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetNumberField(TEXT("attenuation_radius"), Radius);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightConeAngles(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	double InnerAngle = 0.0;
	Params->TryGetNumberField(TEXT("inner"), InnerAngle);

	double OuterAngle = 44.0;
	Params->TryGetNumberField(TEXT("outer"), OuterAngle);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	USpotLightComponent* SpotComp = GetSpotLightComponent(Actor, Error);
	if (!SpotComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Light Cone Angles")));
	SpotComp->Modify();
	SpotComp->SetInnerConeAngle(static_cast<float>(InnerAngle));
	SpotComp->SetOuterConeAngle(static_cast<float>(OuterAngle));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetNumberField(TEXT("inner_cone_angle"), InnerAngle);
	Result->SetNumberField(TEXT("outer_cone_angle"), OuterAngle);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightSourceRadius(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	double Radius = 0.0;
	Params->TryGetNumberField(TEXT("radius"), Radius);

	double SoftRadius = 0.0;
	Params->TryGetNumberField(TEXT("soft_radius"), SoftRadius);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	UPointLightComponent* PointComp = GetPointLightComponent(Actor, Error);
	if (!PointComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Light Source Radius")));
	PointComp->Modify();
	PointComp->SetSourceRadius(static_cast<float>(Radius));
	PointComp->SetSoftSourceRadius(static_cast<float>(SoftRadius));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetNumberField(TEXT("source_radius"), Radius);
	Result->SetNumberField(TEXT("soft_source_radius"), SoftRadius);
	return Result;
}

// ---------------------------------------------------------------------------
// Phase 2: Advanced Light Properties
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightIesProfile(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	FString IesPath;
	if (!Params->TryGetStringField(TEXT("ies_path"), IesPath) || IesPath.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'ies_path' parameter"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	ULightComponent* LightComp = GetLightComponent(Actor, Error);
	if (!LightComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UTextureLightProfile* IESTexture = LoadObject<UTextureLightProfile>(nullptr, *IesPath);
	if (!IESTexture)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load IES profile at path '%s'"), *IesPath));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set IES Profile")));
	LightComp->Modify();
	LightComp->SetIESTexture(IESTexture);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("ies_path"), IesPath);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightChannel(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	int32 Channel = 0;
	if (!Params->TryGetNumberField(TEXT("channel"), Channel) || Channel < 0 || Channel > 3)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid 'channel' parameter. Use 0-3"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	ULightComponent* LightComp = GetLightComponent(Actor, Error);
	if (!LightComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Light Channel")));
	LightComp->Modify();
	FLightingChannels Channels;
	Channels.bChannel0 = (Channel == 0);
	Channels.bChannel1 = (Channel == 1);
	Channels.bChannel2 = (Channel == 2);
	LightComp->LightingChannels = Channels;

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetNumberField(TEXT("channel"), Channel);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetRectLightProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	URectLightComponent* RectComp = GetRectLightComponent(Actor, Error);
	if (!RectComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Rect Light Properties")));
	RectComp->Modify();

	double SourceWidth = 0.0;
	if (Params->TryGetNumberField(TEXT("source_width"), SourceWidth))
	{
		RectComp->SetSourceWidth(static_cast<float>(SourceWidth));
	}

	double SourceHeight = 0.0;
	if (Params->TryGetNumberField(TEXT("source_height"), SourceHeight))
	{
		RectComp->SetSourceHeight(static_cast<float>(SourceHeight));
	}

	double BarnDoorAngle = 0.0;
	if (Params->TryGetNumberField(TEXT("barn_door_angle"), BarnDoorAngle))
	{
		RectComp->SetBarnDoorAngle(static_cast<float>(BarnDoorAngle));
	}

	double BarnDoorLength = 0.0;
	if (Params->TryGetNumberField(TEXT("barn_door_length"), BarnDoorLength))
	{
		RectComp->SetBarnDoorLength(static_cast<float>(BarnDoorLength));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetNumberField(TEXT("source_width"), RectComp->SourceWidth);
	Result->SetNumberField(TEXT("source_height"), RectComp->SourceHeight);
	Result->SetNumberField(TEXT("barn_door_angle"), RectComp->BarnDoorAngle);
	Result->SetNumberField(TEXT("barn_door_length"), RectComp->BarnDoorLength);
	return Result;
}

// ---------------------------------------------------------------------------
// Phase 3: Sky & Atmosphere
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetSkyLightProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	USkyLightComponent* SkyComp = GetSkyLightComponent(Actor, Error);
	if (!SkyComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Sky Light Properties")));
	SkyComp->Modify();

	FString CubemapPath;
	if (Params->TryGetStringField(TEXT("cubemap_path"), CubemapPath) && !CubemapPath.IsEmpty())
	{
		UTextureCube* Cubemap = LoadObject<UTextureCube>(nullptr, *CubemapPath);
		if (Cubemap)
		{
			SkyComp->SetCubemap(Cubemap);
		}
		else
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load cubemap at path '%s'"), *CubemapPath));
		}
	}

	double Intensity = 0.0;
	if (Params->TryGetNumberField(TEXT("intensity"), Intensity))
	{
		SkyComp->SetIntensity(static_cast<float>(Intensity));
	}

	const TArray<TSharedPtr<FJsonValue>>* ColorArray = nullptr;
	if (Params->TryGetArrayField(TEXT("lower_hemisphere_color"), ColorArray) && ColorArray->Num() >= 3)
	{
		FLinearColor NewColor(
			static_cast<float>((*ColorArray)[0]->AsNumber()),
			static_cast<float>((*ColorArray)[1]->AsNumber()),
			static_cast<float>((*ColorArray)[2]->AsNumber())
		);
		SkyComp->SetLowerHemisphereColor(NewColor);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetNumberField(TEXT("intensity"), SkyComp->Intensity);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetSkyAtmosphereProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	USkyAtmosphereComponent* AtmosComp = GetSkyAtmosphereComponent(Actor, Error);
	if (!AtmosComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Sky Atmosphere Properties")));
	AtmosComp->Modify();

	double GroundRadius = 0.0;
	if (Params->TryGetNumberField(TEXT("ground_radius"), GroundRadius))
	{
		AtmosComp->SetBottomRadius(static_cast<float>(GroundRadius));
	}

	double AtmosphereHeight = 0.0;
	if (Params->TryGetNumberField(TEXT("atmosphere_height"), AtmosphereHeight))
	{
		AtmosComp->SetAtmosphereHeight(static_cast<float>(AtmosphereHeight));
	}

	double MieScattering = 0.0;
	if (Params->TryGetNumberField(TEXT("mie_scattering"), MieScattering))
	{
		AtmosComp->MieScatteringScale = static_cast<float>(MieScattering);
	}

	double RayleighScattering = 0.0;
	if (Params->TryGetNumberField(TEXT("rayleigh_scattering"), RayleighScattering))
	{
		AtmosComp->RayleighScatteringScale = static_cast<float>(RayleighScattering);
	}

	double SunIlluminance = 0.0;
	if (Params->TryGetNumberField(TEXT("sun_illuminance_scale"), SunIlluminance))
	{
		AtmosComp->TraceSampleCountScale = static_cast<float>(SunIlluminance);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetHeightFogProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	UExponentialHeightFogComponent* FogComp = GetHeightFogComponent(Actor, Error);
	if (!FogComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Height Fog Properties")));
	FogComp->Modify();

	double FogDensity = 0.0;
	if (Params->TryGetNumberField(TEXT("fog_density"), FogDensity))
	{
		FogComp->SetFogDensity(static_cast<float>(FogDensity));
	}

	double FogHeightFalloff = 0.0;
	if (Params->TryGetNumberField(TEXT("fog_height_falloff"), FogHeightFalloff))
	{
		FogComp->SetFogHeightFalloff(static_cast<float>(FogHeightFalloff));
	}

	double FogMaxOpacity = 0.0;
	if (Params->TryGetNumberField(TEXT("fog_max_opacity"), FogMaxOpacity))
	{
		FogComp->SetFogMaxOpacity(static_cast<float>(FogMaxOpacity));
	}

	double StartDistance = 0.0;
	if (Params->TryGetNumberField(TEXT("start_distance"), StartDistance))
	{
		FogComp->SetStartDistance(static_cast<float>(StartDistance));
	}

	const TArray<TSharedPtr<FJsonValue>>* ColorArray = nullptr;
	if (Params->TryGetArrayField(TEXT("light_inscattering_color"), ColorArray) && ColorArray->Num() >= 3)
	{
		FLinearColor NewColor(
			static_cast<float>((*ColorArray)[0]->AsNumber()),
			static_cast<float>((*ColorArray)[1]->AsNumber()),
			static_cast<float>((*ColorArray)[2]->AsNumber())
		);
		FogComp->SetFogInscatteringColor(NewColor);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetVolumetricFog(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	bool bEnabled = true;
	Params->TryGetBoolField(TEXT("enabled"), bEnabled);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	FString Error;
	UExponentialHeightFogComponent* FogComp = GetHeightFogComponent(Actor, Error);
	if (!FogComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Volumetric Fog")));
	FogComp->Modify();
	FogComp->SetVolumetricFog(bEnabled);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetBoolField(TEXT("volumetric_fog_enabled"), bEnabled);
	return Result;
}

// ---------------------------------------------------------------------------
// Phase 4: Sun, Environment, Reflections
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetDirectionalLightAsSun(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	bool bIsSun = true;
	Params->TryGetBoolField(TEXT("is_sun"), bIsSun);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	UDirectionalLightComponent* DirComp = Cast<UDirectionalLightComponent>(Actor->GetComponentByClass(UDirectionalLightComponent::StaticClass()));
	if (!DirComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' does not have a DirectionalLightComponent"), *ActorName));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Directional Light As Sun")));
	DirComp->Modify();
	DirComp->SetAtmosphereSunLight(bIsSun);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetBoolField(TEXT("is_sun"), bIsSun);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetSunPosition(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	double Azimuth = 0.0;
	Params->TryGetNumberField(TEXT("azimuth"), Azimuth);

	double Zenith = 45.0;
	Params->TryGetNumberField(TEXT("zenith"), Zenith);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	UDirectionalLightComponent* DirComp = Cast<UDirectionalLightComponent>(Actor->GetComponentByClass(UDirectionalLightComponent::StaticClass()));
	if (!DirComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' does not have a DirectionalLightComponent"), *ActorName));
	}

	// Convert azimuth/zenith to rotation
	// Azimuth: angle around the vertical axis (Yaw)
	// Zenith: angle from vertical (Pitch, where 0 = straight up, 90 = horizon)
	float Pitch = -1.0f * static_cast<float>(Zenith) + 90.0f;  // UE pitch: -90 = straight up, 0 = horizon
	float Yaw = static_cast<float>(Azimuth);
	FRotator NewRotation(Pitch, Yaw, 0.0f);

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Sun Position")));
	Actor->Modify();
	Actor->SetActorRotation(NewRotation);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetNumberField(TEXT("azimuth"), Azimuth);
	Result->SetNumberField(TEXT("zenith"), Zenith);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleCreateHdriBackdrop(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	FString HdriPath;
	if (!Params->TryGetStringField(TEXT("hdri_path"), HdriPath) || HdriPath.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'hdri_path' parameter"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
	}

	UTextureCube* Cubemap = LoadObject<UTextureCube>(nullptr, *HdriPath);
	if (!Cubemap)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load HDRI cubemap at path '%s'"), *HdriPath));
	}

	FVector Location(0.0f, 0.0f, 0.0f);
	FVector RotationVec(0.0f, 0.0f, 0.0f);
	FString ParamError;
	if (Params->HasField(TEXT("location")))
	{
		if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("location"), Location, ParamError))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'location': %s"), *ParamError));
		}
	}
	if (Params->HasField(TEXT("rotation")))
	{
		FRotator Rot;
		if (!FEpicUnrealMCPCommonUtils::TryGetRotatorFromJson(Params, TEXT("rotation"), Rot, ParamError))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'rotation': %s"), *ParamError));
		}
		RotationVec = FVector(Rot.Pitch, Rot.Yaw, Rot.Roll);
	}

	// Spawn a SkyLight actor and assign the HDRI cubemap as the primary HDRI mechanism
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = *ActorName;
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FTransform SpawnTransform(FRotator(RotationVec.X, RotationVec.Y, RotationVec.Z), Location);
	ASkyLight* NewSkyLight = World->SpawnActorDeferred<ASkyLight>(ASkyLight::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!NewSkyLight)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to spawn SkyLight actor '%s'"), *ActorName));
	}

	NewSkyLight->GetLightComponent()->SetCubemap(Cubemap);
	NewSkyLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	NewSkyLight->FinishSpawning(SpawnTransform);

	double Intensity = 1.0;
	if (Params->TryGetNumberField(TEXT("intensity"), Intensity))
	{
		NewSkyLight->GetLightComponent()->SetIntensity(static_cast<float>(Intensity));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("hdri_path"), HdriPath);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleCreateReflectionCapture(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	FString TypeStr;
	if (!Params->TryGetStringField(TEXT("type"), TypeStr) || TypeStr.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'type' parameter. Use: Sphere, Box"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
	}

	FVector Location(0.0f, 0.0f, 0.0f);
	FString ParamError;
	if (Params->HasField(TEXT("location")))
	{
		if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("location"), Location, ParamError))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'location': %s"), *ParamError));
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = *ActorName;
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* NewActor = nullptr;
	if (TypeStr == TEXT("Sphere"))
	{
		ASphereReflectionCapture* Capture = World->SpawnActor<ASphereReflectionCapture>(ASphereReflectionCapture::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
		NewActor = Capture;
		if (Capture)
		{
			double Radius = 1000.0;
			Params->TryGetNumberField(TEXT("radius"), Radius);
			if (USphereReflectionCaptureComponent* SphereComponent = Cast<USphereReflectionCaptureComponent>(Capture->GetCaptureComponent()))
			{
				SphereComponent->InfluenceRadius = static_cast<float>(Radius);
				SphereComponent->MarkDirtyForRecapture();
			}
		}
	}
	else if (TypeStr == TEXT("Box"))
	{
		ABoxReflectionCapture* Capture = World->SpawnActor<ABoxReflectionCapture>(ABoxReflectionCapture::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
		NewActor = Capture;
		if (Capture)
		{
			FVector Extent(1000.0f, 1000.0f, 1000.0f);
			if (Params->HasField(TEXT("extent")))
			{
				if (FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("extent"), Extent, ParamError))
				{
					Capture->SetActorScale3D(Extent / 100.0f);
					if (UBoxReflectionCaptureComponent* BoxComponent = Cast<UBoxReflectionCaptureComponent>(Capture->GetCaptureComponent()))
					{
						BoxComponent->BoxTransitionDistance = FMath::Max3(Extent.X, Extent.Y, Extent.Z) * 0.1f;
						BoxComponent->MarkDirtyForRecapture();
					}
				}
			}
		}
	}
	else
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid reflection capture type '%s'. Use: Sphere, Box"), *TypeStr));
	}

	if (!NewActor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to spawn reflection capture actor '%s'"), *ActorName));
	}

	double Brightness = 1.0;
	if (Params->TryGetNumberField(TEXT("brightness"), Brightness))
	{
		AReflectionCapture* ReflectionCapture = Cast<AReflectionCapture>(NewActor);
		if (ReflectionCapture && ReflectionCapture->GetCaptureComponent())
		{
			ReflectionCapture->GetCaptureComponent()->Brightness = static_cast<float>(Brightness);
			ReflectionCapture->GetCaptureComponent()->MarkDirtyForRecapture();
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("type"), TypeStr);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetReflectionCaptureSettings(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	AReflectionCapture* Capture = Cast<AReflectionCapture>(Actor);
	if (!Capture || !Capture->GetCaptureComponent())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' is not a ReflectionCapture"), *ActorName));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Reflection Capture Settings")));
	Capture->GetCaptureComponent()->Modify();

	double Brightness = 0.0;
	if (Params->TryGetNumberField(TEXT("brightness"), Brightness))
	{
		Capture->GetCaptureComponent()->Brightness = static_cast<float>(Brightness);
		Capture->GetCaptureComponent()->MarkDirtyForRecapture();
	}

	FVector Offset(0.0f, 0.0f, 0.0f);
	FString ParamError;
	if (Params->HasField(TEXT("capture_offset")))
	{
		if (FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("capture_offset"), Offset, ParamError))
		{
			Capture->GetCaptureComponent()->CaptureOffset = Offset;
			Capture->GetCaptureComponent()->MarkDirtyForRecapture();
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleBuildReflectionCaptures(const TSharedPtr<FJsonObject>& Params)
{
	if (UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
	{
		UReflectionCaptureComponent::UpdateReflectionCaptureContents(World, TEXT("UnrealMCP"), false, false, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), TEXT("Reflection capture build initiated"));
	return Result;
}

// ---------------------------------------------------------------------------
// Phase 5: Production Pipeline
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleCreateLightmassImportanceVolume(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
	}

	FVector Location(0.0f, 0.0f, 0.0f);
	FString ParamError;
	if (Params->HasField(TEXT("location")))
	{
		if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("location"), Location, ParamError))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'location': %s"), *ParamError));
		}
	}

	FVector Extent(1000.0f, 1000.0f, 1000.0f);
	if (Params->HasField(TEXT("extent")))
	{
		if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("extent"), Extent, ParamError))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'extent': %s"), *ParamError));
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ALightmassImportanceVolume* Volume = World->SpawnActor<ALightmassImportanceVolume>(ALightmassImportanceVolume::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
	if (!Volume)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn LightmassImportanceVolume"));
	}

	if (Volume->GetBrushComponent())
	{
		Volume->GetBrushComponent()->SetRelativeScale3D(Extent / 500.0f);  // Approximate scale mapping
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), Volume->GetName());
	Result->SetStringField(TEXT("class"), TEXT("LightmassImportanceVolume"));
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleBuildLighting(const TSharedPtr<FJsonObject>& Params)
{
	FString QualityStr = TEXT("Preview");
	Params->TryGetStringField(TEXT("quality"), QualityStr);

	FLightingBuildOptions Options;
	if (QualityStr == TEXT("Preview"))
	{
		Options.QualityLevel = Quality_Preview;
	}
	else if (QualityStr == TEXT("Medium"))
	{
		Options.QualityLevel = Quality_Medium;
	}
	else if (QualityStr == TEXT("High"))
	{
		Options.QualityLevel = Quality_High;
	}
	else if (QualityStr == TEXT("Production"))
	{
		Options.QualityLevel = Quality_Production;
	}
	else
	{
		Options.QualityLevel = Quality_Preview;
	}

	GEditor->BuildLighting(Options);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("quality"), QualityStr);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Lighting build initiated with quality: %s"), *QualityStr));
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetLightingScenario(const TSharedPtr<FJsonObject>& Params)
{
	FString ScenarioName;
	if (!Params->TryGetStringField(TEXT("scenario_name"), ScenarioName) || ScenarioName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'scenario_name' parameter"));
	}

	bool bActivate = true;
	Params->TryGetBoolField(TEXT("activate"), bActivate);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
	}

	// Find streaming level by name and toggle lighting scenario
	for (ULevelStreaming* LevelStreaming : World->GetStreamingLevels())
	{
		if (LevelStreaming && LevelStreaming->GetWorldAssetPackageName().Contains(ScenarioName))
		{
			LevelStreaming->SetShouldBeLoaded(bActivate);
			LevelStreaming->SetShouldBeVisible(bActivate);
			LevelStreaming->SetIsRequestingUnloadAndRemoval(!bActivate);
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("scenario_name"), ScenarioName);
	Result->SetBoolField(TEXT("activated"), bActivate);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPLightingAtmosphereCommands::HandleSetMegaliights(const TSharedPtr<FJsonObject>& Params)
{
	bool bEnabled = true;
	Params->TryGetBoolField(TEXT("enabled"), bEnabled);

	int32 Quality = 1;
	Params->TryGetNumberField(TEXT("quality"), Quality);

	// MegaLights is controlled via project settings / console variables
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MegaLights.Enabled"));
	if (CVar)
	{
		CVar->Set(bEnabled ? 1 : 0, ECVF_SetByCode);
	}

	IConsoleVariable* QualityCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MegaLights.Quality"));
	if (QualityCVar)
	{
		QualityCVar->Set(Quality, ECVF_SetByCode);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("enabled"), bEnabled);
	Result->SetNumberField(TEXT("quality"), Quality);
	return Result;
}
