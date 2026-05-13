#include "Commands/EpicUnrealMCPSequencerCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Sections/MovieSceneEventSection.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Channels/MovieSceneChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneFloatChannel.h"

FEpicUnrealMCPSequencerCommands::FEpicUnrealMCPSequencerCommands()
{
}

FEpicUnrealMCPSequencerCommands::~FEpicUnrealMCPSequencerCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPSequencerCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPSequencerCommands::*)(const TSharedPtr<FJsonObject>&);
    static const TMap<FString, Handler> Dispatch = {
        {TEXT("create_level_sequence"), &FEpicUnrealMCPSequencerCommands::HandleCreateLevelSequence},
        {TEXT("add_actor_binding"), &FEpicUnrealMCPSequencerCommands::HandleAddActorBinding},
        {TEXT("add_transform_track"), &FEpicUnrealMCPSequencerCommands::HandleAddTransformTrack},
        {TEXT("add_camera_cut_track"), &FEpicUnrealMCPSequencerCommands::HandleAddCameraCutTrack},
        {TEXT("add_event_track"), &FEpicUnrealMCPSequencerCommands::HandleAddEventTrack},
        {TEXT("add_keyframe"), &FEpicUnrealMCPSequencerCommands::HandleAddKeyframe},
        {TEXT("set_playback_range"), &FEpicUnrealMCPSequencerCommands::HandleSetPlaybackRange},
        {TEXT("set_frame_rate"), &FEpicUnrealMCPSequencerCommands::HandleSetFrameRate},
    };

    const Handler* H = Dispatch.Find(CommandType);
    if (H)
    {
        return (this->*(*H))(Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown sequencer command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPSequencerCommands::HandleCreateLevelSequence(const TSharedPtr<FJsonObject>& Params)
{
    FString SequencePath;
    if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath) || SequencePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'sequence_path' parameter"));
    }

    int32 DurationFrames = 150;
    Params->TryGetNumberField(TEXT("duration_frames"), DurationFrames);

    int32 FrameRateNum = 30;
    Params->TryGetNumberField(TEXT("frame_rate_numerator"), FrameRateNum);
    int32 FrameRateDen = 1;
    Params->TryGetNumberField(TEXT("frame_rate_denominator"), FrameRateDen);

    FString SequenceName = FPaths::GetBaseFilename(SequencePath);
    UPackage* Package = CreatePackage(*SequencePath);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for level sequence"));
    }

    ULevelSequence* Sequence = NewObject<ULevelSequence>(Package, FName(*SequenceName), RF_Public | RF_Standalone);
    if (!Sequence)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create LevelSequence object"));
    }

    Sequence->Initialize();
    UMovieScene* MovieScene = Sequence->GetMovieScene();
    if (MovieScene)
    {
        MovieScene->SetDisplayRate(FFrameRate(FrameRateNum, FrameRateDen));
        MovieScene->SetPlaybackRange(0, DurationFrames);
        MovieScene->SetWorkingRange(0.0f, static_cast<float>(DurationFrames) / static_cast<float>(FrameRateNum));
    }

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Sequence);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("sequence_path"), SequencePath);
    Result->SetStringField(TEXT("sequence_name"), SequenceName);
    Result->SetNumberField(TEXT("duration_frames"), DurationFrames);
    Result->SetNumberField(TEXT("frame_rate"), static_cast<double>(FrameRateNum) / static_cast<double>(FrameRateDen));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPSequencerCommands::HandleAddActorBinding(const TSharedPtr<FJsonObject>& Params)
{
    FString SequencePath;
    if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath) || SequencePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'sequence_path' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
    }

    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (!Sequence)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("LevelSequence not found: %s"), *SequencePath));
    }

    UMovieScene* MovieScene = Sequence->GetMovieScene();
    if (!MovieScene)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelSequence has no MovieScene"));
    }

    // Find actor in current world
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active editor world"));
    }

    AActor* Actor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorNameOrLabel().Equals(ActorName, ESearchCase::IgnoreCase))
        {
            Actor = *It;
            break;
        }
    }

    if (!Actor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found in level"), *ActorName));
    }

    FGuid Binding = MovieScene->AddPossessable(Actor->GetActorNameOrLabel(), Actor->GetClass());
    Sequence->BindPossessableObject(Binding, *Actor, World);
    Sequence->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("sequence_path"), SequencePath);
    Result->SetStringField(TEXT("actor_name"), ActorName);
    Result->SetStringField(TEXT("binding_guid"), Binding.ToString());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPSequencerCommands::HandleAddTransformTrack(const TSharedPtr<FJsonObject>& Params)
{
    FString SequencePath;
    if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath) || SequencePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'sequence_path' parameter"));
    }

    FString BindingGuidStr;
    if (!Params->TryGetStringField(TEXT("binding_guid"), BindingGuidStr) || BindingGuidStr.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'binding_guid' parameter"));
    }

    FGuid BindingGuid;
    if (!FGuid::Parse(BindingGuidStr, BindingGuid))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid 'binding_guid' format"));
    }

    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (!Sequence)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("LevelSequence not found: %s"), *SequencePath));
    }

    UMovieScene* MovieScene = Sequence->GetMovieScene();
    if (!MovieScene)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelSequence has no MovieScene"));
    }

    UMovieScene3DTransformTrack* TransformTrack = MovieScene->AddTrack<UMovieScene3DTransformTrack>(BindingGuid);
    if (!TransformTrack)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add transform track"));
    }

    UMovieSceneSection* Section = TransformTrack->CreateNewSection();
    if (Section)
    {
        TransformTrack->AddSection(*Section);
    }

    Sequence->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("sequence_path"), SequencePath);
    Result->SetStringField(TEXT("binding_guid"), BindingGuidStr);
    Result->SetStringField(TEXT("track_type"), TEXT("transform"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPSequencerCommands::HandleAddCameraCutTrack(const TSharedPtr<FJsonObject>& Params)
{
    FString SequencePath;
    if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath) || SequencePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'sequence_path' parameter"));
    }

    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (!Sequence)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("LevelSequence not found: %s"), *SequencePath));
    }

    UMovieScene* MovieScene = Sequence->GetMovieScene();
    if (!MovieScene)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelSequence has no MovieScene"));
    }

    UMovieSceneCameraCutTrack* CameraCutTrack = Cast<UMovieSceneCameraCutTrack>(MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass()));
    if (!CameraCutTrack)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add camera cut track"));
    }

    // Optionally add a section with a camera binding
    FString CameraBindingGuidStr;
    if (Params->TryGetStringField(TEXT("camera_binding_guid"), CameraBindingGuidStr) && !CameraBindingGuidStr.IsEmpty())
    {
        FGuid CameraGuid;
        if (FGuid::Parse(CameraBindingGuidStr, CameraGuid))
        {
            UMovieSceneCameraCutSection* CutSection = Cast<UMovieSceneCameraCutSection>(CameraCutTrack->CreateNewSection());
            if (CutSection)
            {
                CutSection->SetCameraGuid(CameraGuid);
                CameraCutTrack->AddSection(*CutSection);
            }
        }
    }

    Sequence->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("sequence_path"), SequencePath);
    Result->SetStringField(TEXT("track_type"), TEXT("camera_cut"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPSequencerCommands::HandleAddEventTrack(const TSharedPtr<FJsonObject>& Params)
{
    FString SequencePath;
    if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath) || SequencePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'sequence_path' parameter"));
    }

    FString BindingGuidStr;
    if (!Params->TryGetStringField(TEXT("binding_guid"), BindingGuidStr) || BindingGuidStr.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'binding_guid' parameter"));
    }

    FGuid BindingGuid;
    if (!FGuid::Parse(BindingGuidStr, BindingGuid))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid 'binding_guid' format"));
    }

    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (!Sequence)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("LevelSequence not found: %s"), *SequencePath));
    }

    UMovieScene* MovieScene = Sequence->GetMovieScene();
    if (!MovieScene)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelSequence has no MovieScene"));
    }

    UMovieSceneEventTrack* EventTrack = MovieScene->AddTrack<UMovieSceneEventTrack>(BindingGuid);
    if (!EventTrack)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add event track"));
    }

    UMovieSceneSection* Section = EventTrack->CreateNewSection();
    if (Section)
    {
        EventTrack->AddSection(*Section);
    }

    Sequence->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("sequence_path"), SequencePath);
    Result->SetStringField(TEXT("binding_guid"), BindingGuidStr);
    Result->SetStringField(TEXT("track_type"), TEXT("event"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPSequencerCommands::HandleAddKeyframe(const TSharedPtr<FJsonObject>& Params)
{
    FString SequencePath;
    if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath) || SequencePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'sequence_path' parameter"));
    }

    FString BindingGuidStr;
    if (!Params->TryGetStringField(TEXT("binding_guid"), BindingGuidStr) || BindingGuidStr.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'binding_guid' parameter"));
    }

    FGuid BindingGuid;
    if (!FGuid::Parse(BindingGuidStr, BindingGuid))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid 'binding_guid' format"));
    }

    int32 Frame = 0;
    Params->TryGetNumberField(TEXT("frame"), Frame);

    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (!Sequence)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("LevelSequence not found: %s"), *SequencePath));
    }

    UMovieScene* MovieScene = Sequence->GetMovieScene();
    if (!MovieScene)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelSequence has no MovieScene"));
    }

    // Find transform track for this binding
    UMovieScene3DTransformTrack* TransformTrack = nullptr;
    for (UMovieSceneTrack* Track : MovieScene->GetTracks())
    {
        if (Track->GetAllSections().Num() > 0 && Track->SupportsType(UMovieScene3DTransformSection::StaticClass()))
        {
            // Check if track belongs to this binding via the object binding ID
            if (Track->FindObjectBindingGuid() == BindingGuid)
            {
                TransformTrack = Cast<UMovieScene3DTransformTrack>(Track);
                break;
            }
        }
    }

    if (!TransformTrack)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No transform track found for binding"));
    }

    TArray<UMovieSceneSection*> Sections = TransformTrack->GetAllSections();
    if (Sections.Num() == 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Transform track has no sections"));
    }

    UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(Sections[0]);
    if (!TransformSection)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to cast section to transform section"));
    }

    // Parse optional location / rotation / scale values
    float LocX = 0.0f, LocY = 0.0f, LocZ = 0.0f;
    float RotX = 0.0f, RotY = 0.0f, RotZ = 0.0f;
    float ScaleX = 1.0f, ScaleY = 1.0f, ScaleZ = 1.0f;

    const TSharedPtr<FJsonObject>* LocationObj = nullptr;
    if (Params->TryGetObjectField(TEXT("location"), LocationObj) && LocationObj->IsValid())
    {
        (*LocationObj)->TryGetNumberField(TEXT("x"), LocX);
        (*LocationObj)->TryGetNumberField(TEXT("y"), LocY);
        (*LocationObj)->TryGetNumberField(TEXT("z"), LocZ);
    }

    const TSharedPtr<FJsonObject>* RotationObj = nullptr;
    if (Params->TryGetObjectField(TEXT("rotation"), RotationObj) && RotationObj->IsValid())
    {
        (*RotationObj)->TryGetNumberField(TEXT("x"), RotX);
        (*RotationObj)->TryGetNumberField(TEXT("y"), RotY);
        (*RotationObj)->TryGetNumberField(TEXT("z"), RotZ);
    }

    const TSharedPtr<FJsonObject>* ScaleObj = nullptr;
    if (Params->TryGetObjectField(TEXT("scale"), ScaleObj) && ScaleObj->IsValid())
    {
        (*ScaleObj)->TryGetNumberField(TEXT("x"), ScaleX);
        (*ScaleObj)->TryGetNumberField(TEXT("y"), ScaleY);
        (*ScaleObj)->TryGetNumberField(TEXT("z"), ScaleZ);
    }

    FFrameNumber KeyTime(Frame);

#if ENGINE_MAJOR_VERSION >= 5
    // UE5 uses double channels accessed via ChannelProxy.
    // Channel order on UMovieScene3DTransformSection: [Tx,Ty,Tz, Rx,Ry,Rz, Sx,Sy,Sz, ManualWeight].
    TArrayView<FMovieSceneDoubleChannel*> AllDoubleChannels =
        TransformSection->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
    if (AllDoubleChannels.Num() >= 9)
    {
        AllDoubleChannels[0]->AddCubicKey(KeyTime, static_cast<double>(LocX));
        AllDoubleChannels[1]->AddCubicKey(KeyTime, static_cast<double>(LocY));
        AllDoubleChannels[2]->AddCubicKey(KeyTime, static_cast<double>(LocZ));
        AllDoubleChannels[3]->AddCubicKey(KeyTime, static_cast<double>(RotX));
        AllDoubleChannels[4]->AddCubicKey(KeyTime, static_cast<double>(RotY));
        AllDoubleChannels[5]->AddCubicKey(KeyTime, static_cast<double>(RotZ));
        AllDoubleChannels[6]->AddCubicKey(KeyTime, static_cast<double>(ScaleX));
        AllDoubleChannels[7]->AddCubicKey(KeyTime, static_cast<double>(ScaleY));
        AllDoubleChannels[8]->AddCubicKey(KeyTime, static_cast<double>(ScaleZ));
    }
#else
    // UE4 uses float channels
    TArrayView<FMovieSceneFloatChannel*> TransChannels = TransformSection->GetTranslationChannel();
    if (TransChannels.Num() >= 3)
    {
        TransChannels[0]->AddKey(KeyTime, LocX);
        TransChannels[1]->AddKey(KeyTime, LocY);
        TransChannels[2]->AddKey(KeyTime, LocZ);
    }

    TArrayView<FMovieSceneFloatChannel*> RotChannels = TransformSection->GetRotationChannel();
    if (RotChannels.Num() >= 3)
    {
        RotChannels[0]->AddKey(KeyTime, RotX);
        RotChannels[1]->AddKey(KeyTime, RotY);
        RotChannels[2]->AddKey(KeyTime, RotZ);
    }

    TArrayView<FMovieSceneFloatChannel*> ScaleChannels = TransformSection->GetScaleChannel();
    if (ScaleChannels.Num() >= 3)
    {
        ScaleChannels[0]->AddKey(KeyTime, ScaleX);
        ScaleChannels[1]->AddKey(KeyTime, ScaleY);
        ScaleChannels[2]->AddKey(KeyTime, ScaleZ);
    }
#endif

    Sequence->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("sequence_path"), SequencePath);
    Result->SetStringField(TEXT("binding_guid"), BindingGuidStr);
    Result->SetNumberField(TEXT("frame"), Frame);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPSequencerCommands::HandleSetPlaybackRange(const TSharedPtr<FJsonObject>& Params)
{
    FString SequencePath;
    if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath) || SequencePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'sequence_path' parameter"));
    }

    int32 StartFrame = 0;
    Params->TryGetNumberField(TEXT("start_frame"), StartFrame);
    int32 EndFrame = 150;
    Params->TryGetNumberField(TEXT("end_frame"), EndFrame);

    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (!Sequence)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("LevelSequence not found: %s"), *SequencePath));
    }

    UMovieScene* MovieScene = Sequence->GetMovieScene();
    if (!MovieScene)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelSequence has no MovieScene"));
    }

    MovieScene->SetPlaybackRange(StartFrame, EndFrame - StartFrame);
    Sequence->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("sequence_path"), SequencePath);
    Result->SetNumberField(TEXT("start_frame"), StartFrame);
    Result->SetNumberField(TEXT("end_frame"), EndFrame);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPSequencerCommands::HandleSetFrameRate(const TSharedPtr<FJsonObject>& Params)
{
    FString SequencePath;
    if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath) || SequencePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'sequence_path' parameter"));
    }

    int32 Numerator = 30;
    Params->TryGetNumberField(TEXT("numerator"), Numerator);
    int32 Denominator = 1;
    Params->TryGetNumberField(TEXT("denominator"), Denominator);

    if (Denominator <= 0)
    {
        Denominator = 1;
    }

    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (!Sequence)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("LevelSequence not found: %s"), *SequencePath));
    }

    UMovieScene* MovieScene = Sequence->GetMovieScene();
    if (!MovieScene)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelSequence has no MovieScene"));
    }

    MovieScene->SetDisplayRate(FFrameRate(Numerator, Denominator));
    Sequence->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("sequence_path"), SequencePath);
    Result->SetNumberField(TEXT("numerator"), Numerator);
    Result->SetNumberField(TEXT("denominator"), Denominator);
    return Result;
}
