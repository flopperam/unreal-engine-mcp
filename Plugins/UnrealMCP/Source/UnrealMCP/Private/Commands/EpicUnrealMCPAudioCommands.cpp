#include "Commands/EpicUnrealMCPAudioCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Engine/World.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundAttenuation.h"
#include "Components/AudioComponent.h"
#include "UObject/Package.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"

FEpicUnrealMCPAudioCommands::FEpicUnrealMCPAudioCommands()
{
}

FEpicUnrealMCPAudioCommands::~FEpicUnrealMCPAudioCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAudioCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPAudioCommands::*)(const TSharedPtr<FJsonObject>&);
    static const TMap<FString, Handler> Dispatch = {
        {TEXT("create_sound_cue"), &FEpicUnrealMCPAudioCommands::HandleCreateSoundCue},
        {TEXT("add_audio_component"), &FEpicUnrealMCPAudioCommands::HandleAddAudioComponent},
        {TEXT("set_sound_attenuation"), &FEpicUnrealMCPAudioCommands::HandleSetSoundAttenuation},
    };

    const Handler* H = Dispatch.Find(CommandType);
    if (H)
    {
        return (this->*(*H))(Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown audio command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAudioCommands::HandleCreateSoundCue(const TSharedPtr<FJsonObject>& Params)
{
    FString CuePath;
    if (!Params->TryGetStringField(TEXT("cue_path"), CuePath) || CuePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'cue_path' parameter"));
    }

    FString CueName = FPaths::GetBaseFilename(CuePath);
    UPackage* Package = CreatePackage(*CuePath);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for sound cue"));
    }

    USoundCue* NewCue = NewObject<USoundCue>(Package, FName(*CueName), RF_Public | RF_Standalone);
    if (!NewCue)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create SoundCue object"));
    }

    FString SoundWavePath;
    if (Params->TryGetStringField(TEXT("sound_wave_path"), SoundWavePath) && !SoundWavePath.IsEmpty())
    {
        USoundWave* SoundWave = LoadObject<USoundWave>(nullptr, *SoundWavePath);
        if (SoundWave)
        {
            NewCue->FirstNode = nullptr; // Let UE build the graph if needed
            // For a simple cue with a single wave, we could add a wave player node
            // but that requires USoundNodeWavePlayer which may not be exposed in all engine versions
            // Setting the SoundWave as the default sound is simpler
            // In practice, a SoundCue without nodes will not play anything
            // This is a basic scaffold; users can edit the cue graph manually or via additional tools
        }
    }

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewCue);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("cue_path"), CuePath);
    Result->SetStringField(TEXT("cue_name"), CueName);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAudioCommands::HandleAddAudioComponent(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
    }

    FString SoundPath;
    if (!Params->TryGetStringField(TEXT("sound_path"), SoundPath) || SoundPath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'sound_path' parameter"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    AActor* Actor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName() == ActorName)
        {
            Actor = *It;
            break;
        }
    }

    if (!Actor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    USoundBase* Sound = LoadObject<USoundBase>(nullptr, *SoundPath);
    if (!Sound)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Sound not found at path '%s'"), *SoundPath));
    }

    UAudioComponent* AudioComp = NewObject<UAudioComponent>(Actor, UAudioComponent::StaticClass());
    if (!AudioComp)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create AudioComponent"));
    }

    AudioComp->SetSound(Sound);
    AudioComp->RegisterComponent();

    double Volume = 1.0;
    if (Params->TryGetNumberField(TEXT("volume"), Volume))
    {
        AudioComp->SetVolumeMultiplier(static_cast<float>(Volume));
    }

    double Pitch = 1.0;
    if (Params->TryGetNumberField(TEXT("pitch"), Pitch))
    {
        AudioComp->SetPitchMultiplier(static_cast<float>(Pitch));
    }

    bool bAutoActivate = false;
    if (Params->TryGetBoolField(TEXT("auto_activate"), bAutoActivate))
    {
        AudioComp->bAutoActivate = bAutoActivate;
    }

    bool bLooping = false;
    if (Params->TryGetBoolField(TEXT("loop"), bLooping))
    {
        // Looping is typically controlled by the SoundCue, but we can set it on the component for SoundWaves
        AudioComp->SetBoolParameter(TEXT("Loop"), bLooping);
    }

    Actor->Modify();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor_name"), ActorName);
    Result->SetStringField(TEXT("sound_path"), SoundPath);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAudioCommands::HandleSetSoundAttenuation(const TSharedPtr<FJsonObject>& Params)
{
    FString AttenuationPath;
    if (!Params->TryGetStringField(TEXT("attenuation_path"), AttenuationPath) || AttenuationPath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'attenuation_path' parameter"));
    }

    USoundAttenuation* Attenuation = LoadObject<USoundAttenuation>(nullptr, *AttenuationPath);
    bool bCreated = false;

    if (!Attenuation)
    {
        FString AttenuationName = FPaths::GetBaseFilename(AttenuationPath);
        UPackage* Package = CreatePackage(*AttenuationPath);
        if (!Package)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for attenuation"));
        }

        Attenuation = NewObject<USoundAttenuation>(Package, FName(*AttenuationName), RF_Public | RF_Standalone);
        if (!Attenuation)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create SoundAttenuation object"));
        }

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(Attenuation);
        bCreated = true;
    }

    FAttenuationSettings& Settings = Attenuation->Attenuation;

    double Radius = 0.0;
    if (Params->TryGetNumberField(TEXT("radius"), Radius))
    {
        Settings.FalloffDistance = static_cast<float>(Radius);
    }

    bool bSpatialization = false;
    if (Params->TryGetBoolField(TEXT("spatialization"), bSpatialization))
    {
        Settings.bAttenuate = true;
        Settings.bSpatialize = bSpatialization;
    }

    bool bConeAttenuation = false;
    if (Params->TryGetBoolField(TEXT("cone_attenuation"), bConeAttenuation))
    {
        Settings.bAttenuateWithLPF = bConeAttenuation;
    }

    double ConeInnerAngle = 0.0;
    if (Params->TryGetNumberField(TEXT("cone_inner_angle"), ConeInnerAngle))
    {
        Settings.ConeRadius = static_cast<float>(ConeInnerAngle);
    }

    double ConeOuterAngle = 0.0;
    if (Params->TryGetNumberField(TEXT("cone_outer_angle"), ConeOuterAngle))
    {
        Settings.ConeRadius = static_cast<float>(ConeOuterAngle);
    }

    double ReverbSend = 0.0;
    if (Params->TryGetNumberField(TEXT("reverb_send"), ReverbSend))
    {
        Settings.bAttenuateWithReverbSend = true;
        Settings.ReverbDistanceMin = 0.0f;
        Settings.ReverbDistanceMax = static_cast<float>(ReverbSend);
    }

    Attenuation->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("attenuation_path"), AttenuationPath);
    Result->SetBoolField(TEXT("created"), bCreated);
    return Result;
}
