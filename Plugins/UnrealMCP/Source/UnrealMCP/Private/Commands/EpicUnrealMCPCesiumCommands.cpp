#include "Commands/EpicUnrealMCPCesiumCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"

#if WITH_CESIUM
// Cesium for Unreal headers 窶・only included when the plugin is detected by Build.cs.
#include "CesiumGeoreference.h"
#include "Cesium3DTileset.h"
#include "CesiumGlobeAnchorComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#endif

FEpicUnrealMCPCesiumCommands::FEpicUnrealMCPCesiumCommands()
{
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPCesiumCommands::HandleCommand(
	const FString& CommandType,
	const TSharedPtr<FJsonObject>& Params)
{
	using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPCesiumCommands::*)(const TSharedPtr<FJsonObject>&);
	static const TMap<FString, Handler> Dispatch = {
		{TEXT("cesium_check_plugin"),                &FEpicUnrealMCPCesiumCommands::HandleCesiumCheckPlugin},
		{TEXT("cesium_setup_georeference"),          &FEpicUnrealMCPCesiumCommands::HandleCesiumSetupGeoreference},
		{TEXT("cesium_add_tileset"),                 &FEpicUnrealMCPCesiumCommands::HandleCesiumAddTileset},
		{TEXT("cesium_place_actor_at_geolocation"),  &FEpicUnrealMCPCesiumCommands::HandleCesiumPlaceActorAtGeolocation},
	};

	if (const Handler* H = Dispatch.Find(CommandType))
	{
		return (this->*(*H))(Params);
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown Cesium command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// cesium_check_plugin (always implemented)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPCesiumCommands::HandleCesiumCheckPlugin(
	const TSharedPtr<FJsonObject>& /*Params*/)
{
	const bool bAvailable = IsCesiumPluginAvailable();
	TSharedPtr<FJsonObject> Diagnostics = CollectCesiumDiagnostics();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("available"), bAvailable);
	Data->SetObjectField(TEXT("plugin"), Diagnostics.ToSharedRef());

#if WITH_CESIUM
	Data->SetBoolField(TEXT("compiled_with_cesium"), true);
#else
	Data->SetBoolField(TEXT("compiled_with_cesium"), false);
#endif

	if (!bAvailable)
	{
		Data->SetStringField(TEXT("hint"),
			TEXT("Install Cesium for Unreal (v2.18+ ships official Unreal Engine 5.7 binaries) from https://cesium.com/learn/unreal/ and enable CesiumForUnreal in this project's .uproject, then rebuild UnrealMCP so WITH_CESIUM=1."));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetObjectField(TEXT("data"), Data.ToSharedRef());
	return Response;
}

// ---------------------------------------------------------------------------
// cesium_setup_georeference
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPCesiumCommands::HandleCesiumSetupGeoreference(
	const TSharedPtr<FJsonObject>& Params)
{
	if (!IsCesiumPluginAvailable())
	{
		return MakeCesiumUnavailableResponse(TEXT("cesium_setup_georeference"));
	}

#if WITH_CESIUM
	double OriginLat = 0.0;
	double OriginLon = 0.0;
	double OriginHeight = 0.0;
	Params->TryGetNumberField(TEXT("origin_latitude"), OriginLat);
	Params->TryGetNumberField(TEXT("origin_longitude"), OriginLon);
	Params->TryGetNumberField(TEXT("origin_height"), OriginHeight);

	FString ActorName = TEXT("CesiumGeoreference");
	Params->TryGetStringField(TEXT("actor_name"), ActorName);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
		Resp->SetBoolField(TEXT("success"), false);
		Resp->SetStringField(TEXT("error"), TEXT("No editor world available"));
		Resp->SetStringField(TEXT("hint"), TEXT("Run from the editor with a level open. PIE and headless builds are not supported."));
		return Resp;
	}

	// Reuse an existing Georeference if present
	ACesiumGeoreference* Geo = nullptr;
	for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
	{
		Geo = *It;
		break;
	}

	bool bCreated = false;
	if (!Geo)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = *ActorName;
		Geo = World->SpawnActor<ACesiumGeoreference>(ACesiumGeoreference::StaticClass(), FTransform::Identity, SpawnParams);
		if (!Geo)
		{
			TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
			Resp->SetBoolField(TEXT("success"), false);
			Resp->SetStringField(TEXT("error"), TEXT("Failed to spawn ACesiumGeoreference"));
			Resp->SetStringField(TEXT("hint"), TEXT("Verify CesiumForUnreal plugin module 'CesiumRuntime' is loaded (cesium_check_plugin)"));
			return Resp;
		}
		Geo->SetActorLabel(*ActorName);
		Geo->Tags.AddUnique(FName(TEXT("managed_by_mcp")));
		bCreated = true;
	}

	Geo->SetOriginLongitudeLatitudeHeight(FVector(OriginLon, OriginLat, OriginHeight));
	Geo->Modify();
	Geo->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor_name"), Geo->GetName());
	Data->SetStringField(TEXT("actor_path"), Geo->GetPathName());
	Data->SetBoolField(TEXT("created"), bCreated);
	Data->SetNumberField(TEXT("origin_latitude"), OriginLat);
	Data->SetNumberField(TEXT("origin_longitude"), OriginLon);
	Data->SetNumberField(TEXT("origin_height"), OriginHeight);

	TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
	Resp->SetBoolField(TEXT("success"), true);
	Resp->SetObjectField(TEXT("data"), Data.ToSharedRef());
	return Resp;
#else
	return MakeCesiumNotImplementedResponse(TEXT("cesium_setup_georeference"));
#endif
}

// ---------------------------------------------------------------------------
// cesium_add_tileset
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPCesiumCommands::HandleCesiumAddTileset(
	const TSharedPtr<FJsonObject>& Params)
{
	if (!IsCesiumPluginAvailable())
	{
		return MakeCesiumUnavailableResponse(TEXT("cesium_add_tileset"));
	}

#if WITH_CESIUM
	FString ActorName = TEXT("Cesium3DTileset");
	Params->TryGetStringField(TEXT("actor_name"), ActorName);

	FString Url;
	Params->TryGetStringField(TEXT("url"), Url);
	int32 IonAssetId = 0;
	Params->TryGetNumberField(TEXT("ion_asset_id"), IonAssetId);
	FString IonAccessToken;
	Params->TryGetStringField(TEXT("ion_access_token"), IonAccessToken);

	if (Url.IsEmpty() && IonAssetId == 0)
	{
		TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
		Resp->SetBoolField(TEXT("success"), false);
		Resp->SetStringField(TEXT("error"), TEXT("Either 'url' or ('ion_asset_id' + 'ion_access_token') must be provided"));
		Resp->SetStringField(TEXT("hint"), TEXT("For Cesium ion assets, set ion_asset_id (e.g. 96188 for Cesium World Terrain) and ion_access_token. Otherwise pass a 3D Tiles tileset.json URL."));
		return Resp;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
		Resp->SetBoolField(TEXT("success"), false);
		Resp->SetStringField(TEXT("error"), TEXT("No editor world available"));
		return Resp;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = *ActorName;
	ACesium3DTileset* Tileset = World->SpawnActor<ACesium3DTileset>(ACesium3DTileset::StaticClass(), FTransform::Identity, SpawnParams);
	if (!Tileset)
	{
		TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
		Resp->SetBoolField(TEXT("success"), false);
		Resp->SetStringField(TEXT("error"), TEXT("Failed to spawn ACesium3DTileset"));
		Resp->SetStringField(TEXT("hint"), TEXT("Cesium plugin may need to register types. Try restarting the editor."));
		return Resp;
	}

	Tileset->SetActorLabel(*ActorName);
	Tileset->Tags.AddUnique(FName(TEXT("managed_by_mcp")));
	if (!Url.IsEmpty())
	{
		Tileset->SetTilesetSource(ETilesetSource::FromUrl);
		Tileset->SetUrl(Url);
	}
	else
	{
		Tileset->SetTilesetSource(ETilesetSource::FromCesiumIon);
		Tileset->SetIonAssetID(IonAssetId);
		Tileset->SetIonAccessToken(IonAccessToken);
	}
	Tileset->Modify();
	Tileset->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor_name"), Tileset->GetName());
	Data->SetStringField(TEXT("actor_path"), Tileset->GetPathName());
	if (!Url.IsEmpty())
	{
		Data->SetStringField(TEXT("source"), TEXT("url"));
		Data->SetStringField(TEXT("url"), Url);
	}
	else
	{
		Data->SetStringField(TEXT("source"), TEXT("cesium_ion"));
		Data->SetNumberField(TEXT("ion_asset_id"), IonAssetId);
	}

	TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
	Resp->SetBoolField(TEXT("success"), true);
	Resp->SetObjectField(TEXT("data"), Data.ToSharedRef());
	return Resp;
#else
	return MakeCesiumNotImplementedResponse(TEXT("cesium_add_tileset"));
#endif
}

// ---------------------------------------------------------------------------
// cesium_place_actor_at_geolocation
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPCesiumCommands::HandleCesiumPlaceActorAtGeolocation(
	const TSharedPtr<FJsonObject>& Params)
{
	if (!IsCesiumPluginAvailable())
	{
		return MakeCesiumUnavailableResponse(TEXT("cesium_place_actor_at_geolocation"));
	}

#if WITH_CESIUM
	// Accept either actor_mcp_id (preferred, looked up by mcp_id:<id> tag) or
	// actor_name (legacy fallback that matches FName / actor label).
	FString McpId;
	FString ActorName;
	const bool bHasMcpId = Params->TryGetStringField(TEXT("actor_mcp_id"), McpId) && !McpId.IsEmpty();
	const bool bHasName  = Params->TryGetStringField(TEXT("actor_name"),  ActorName) && !ActorName.IsEmpty();
	if (!bHasMcpId && !bHasName)
	{
		TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
		Resp->SetBoolField(TEXT("success"), false);
		Resp->SetStringField(TEXT("error"), TEXT("Missing 'actor_mcp_id' (preferred) or 'actor_name' parameter"));
		Resp->SetStringField(TEXT("hint"), TEXT("Spawn the actor first via spawn_actor and pass its mcp_id back here as actor_mcp_id."));
		return Resp;
	}

	double Lat = 0.0, Lon = 0.0, Height = 0.0;
	Params->TryGetNumberField(TEXT("latitude"), Lat);
	Params->TryGetNumberField(TEXT("longitude"), Lon);
	Params->TryGetNumberField(TEXT("height"), Height);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
		Resp->SetBoolField(TEXT("success"), false);
		Resp->SetStringField(TEXT("error"), TEXT("No editor world available"));
		return Resp;
	}

	AActor* Target = nullptr;
	if (bHasMcpId)
	{
		// Primary path: locate by mcp_id:<id> tag (matches the convention used
		// by HandleSpawnActor / FEpicUnrealMCPActorCommands::FindActorByMcpId).
		const FName TargetTag(*FString::Printf(TEXT("mcp_id:%s"), *McpId));
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->Tags.Contains(TargetTag))
			{
				Target = *It;
				break;
			}
		}
	}
	if (!Target && bHasName)
	{
		// Legacy fallback: match by FName or actor label.
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
			{
				Target = *It;
				break;
			}
		}
	}
	if (!Target)
	{
		const FString Lookup = bHasMcpId
			? FString::Printf(TEXT("actor_mcp_id='%s'"), *McpId)
			: FString::Printf(TEXT("actor_name='%s'"), *ActorName);
		TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
		Resp->SetBoolField(TEXT("success"), false);
		Resp->SetStringField(TEXT("error"), FString::Printf(TEXT("Actor not found by %s"), *Lookup));
		Resp->SetStringField(TEXT("hint"), TEXT("Spawn the actor first via spawn_actor (it will get a mcp_id:<id> tag) and pass that mcp_id as actor_mcp_id."));
		return Resp;
	}

	UCesiumGlobeAnchorComponent* Anchor = Target->FindComponentByClass<UCesiumGlobeAnchorComponent>();
	if (!Anchor)
	{
		Anchor = NewObject<UCesiumGlobeAnchorComponent>(Target, TEXT("CesiumGlobeAnchor"));
		if (Anchor)
		{
			Anchor->RegisterComponent();
			Target->AddInstanceComponent(Anchor);
		}
	}
	if (!Anchor)
	{
		TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
		Resp->SetBoolField(TEXT("success"), false);
		Resp->SetStringField(TEXT("error"), TEXT("Failed to attach UCesiumGlobeAnchorComponent"));
		return Resp;
	}

	Anchor->MoveToLongitudeLatitudeHeight(FVector(Lon, Lat, Height));
	Target->Modify();
	Target->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor_name"), Target->GetName());
	Data->SetStringField(TEXT("actor_path"), Target->GetPathName());
	if (bHasMcpId)
	{
		Data->SetStringField(TEXT("actor_mcp_id"), McpId);
		Data->SetStringField(TEXT("matched_by"), TEXT("mcp_id"));
	}
	else
	{
		Data->SetStringField(TEXT("matched_by"), TEXT("actor_name"));
	}
	Data->SetNumberField(TEXT("latitude"), Lat);
	Data->SetNumberField(TEXT("longitude"), Lon);
	Data->SetNumberField(TEXT("height"), Height);

	TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
	Resp->SetBoolField(TEXT("success"), true);
	Resp->SetObjectField(TEXT("data"), Data.ToSharedRef());
	return Resp;
#else
	return MakeCesiumNotImplementedResponse(TEXT("cesium_place_actor_at_geolocation"));
#endif
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool FEpicUnrealMCPCesiumCommands::IsCesiumPluginAvailable()
{
	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CesiumForUnreal")))
	{
		if (Plugin->IsEnabled())
		{
			return true;
		}
	}

	// Fallback: probe runtime modules in case plugin is mounted under a different descriptor name.
	if (FModuleManager::Get().IsModuleLoaded(TEXT("CesiumRuntime")))
	{
		return true;
	}
	if (FModuleManager::Get().IsModuleLoaded(TEXT("CesiumEditor")))
	{
		return true;
	}

	return false;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPCesiumCommands::CollectCesiumDiagnostics()
{
	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetStringField(TEXT("descriptor_name"), TEXT("CesiumForUnreal"));

	bool bDescriptorFound = false;
	bool bDescriptorEnabled = false;
	FString DescriptorVersion = TEXT("unknown");
	FString DescriptorFriendlyName;

	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CesiumForUnreal")))
	{
		bDescriptorFound = true;
		bDescriptorEnabled = Plugin->IsEnabled();
		const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();
		DescriptorVersion = Descriptor.VersionName;
		DescriptorFriendlyName = Descriptor.FriendlyName;
	}

	Out->SetBoolField(TEXT("descriptor_found"), bDescriptorFound);
	Out->SetBoolField(TEXT("enabled"), bDescriptorEnabled);
	Out->SetStringField(TEXT("version"), DescriptorVersion);
	if (!DescriptorFriendlyName.IsEmpty())
	{
		Out->SetStringField(TEXT("friendly_name"), DescriptorFriendlyName);
	}

	TArray<TSharedPtr<FJsonValue>> ModuleStatus;
	const TArray<FString> CandidateModules = { TEXT("CesiumRuntime"), TEXT("CesiumEditor") };
	for (const FString& ModuleName : CandidateModules)
	{
		TSharedPtr<FJsonObject> ModuleInfo = MakeShared<FJsonObject>();
		ModuleInfo->SetStringField(TEXT("name"), ModuleName);
		ModuleInfo->SetBoolField(TEXT("loaded"), FModuleManager::Get().IsModuleLoaded(*ModuleName));
		ModuleStatus.Add(MakeShared<FJsonValueObject>(ModuleInfo));
	}
	Out->SetArrayField(TEXT("modules"), ModuleStatus);

	return Out;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPCesiumCommands::MakeCesiumUnavailableResponse(const FString& CommandName)
{
	TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
	Resp->SetBoolField(TEXT("success"), false);
	Resp->SetStringField(TEXT("error"),
		FString::Printf(TEXT("CesiumForUnreal plugin is not enabled. '%s' cannot run."), *CommandName));
	Resp->SetStringField(TEXT("hint"),
		TEXT("Run 'cesium_check_plugin' to inspect the plugin status, then enable CesiumForUnreal in the .uproject (Cesium for Unreal v2.18+ officially ships UE 5.7 binaries)."));
	return Resp;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPCesiumCommands::MakeCesiumNotImplementedResponse(const FString& CommandName)
{
	TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
	Resp->SetBoolField(TEXT("success"), false);
	Resp->SetStringField(TEXT("error"),
		FString::Printf(TEXT("Cesium command '%s' is reserved for a follow-up implementation."), *CommandName));
	Resp->SetStringField(TEXT("hint"),
		TEXT("Plugin descriptor was detected at runtime but the UnrealMCP module was compiled without WITH_CESIUM. Reinstall the Cesium plugin and rebuild UnrealMCP so Build.cs picks up the .uplugin probe."));
	return Resp;
}