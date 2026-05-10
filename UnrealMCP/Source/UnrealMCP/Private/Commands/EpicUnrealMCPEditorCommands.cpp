#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Commands/EpicUnrealMCPErrorCodes.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EditorAssetLibrary.h"
#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "UObject/UnrealType.h"
// Phase 1: Editor lifecycle
#include "Misc/EngineVersion.h"
#include "FileHelpers.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
// Phase 1: Level management
#include "EditorLevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "EditorLoadingAndSavingUtils.h"
// Phase 1: Viewport
#include "LevelEditor.h"
#include "Slate/SceneViewport.h"

// ============================================================================
// Anonymous namespace: reflection helper functions (merged from standalone)
// ============================================================================
namespace
{
    static bool JsonValueToVector(const TSharedPtr<FJsonValue>& JsonValue, FVector& OutVector)
    {
        if (!JsonValue.IsValid())
        {
            return false;
        }

        if (JsonValue->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& Values = JsonValue->AsArray();
            if (Values.Num() < 3)
            {
                return false;
            }

            OutVector = FVector(
                static_cast<float>(Values[0]->AsNumber()),
                static_cast<float>(Values[1]->AsNumber()),
                static_cast<float>(Values[2]->AsNumber())
            );
            return true;
        }

        if (JsonValue->Type == EJson::Object)
        {
            TSharedPtr<FJsonObject> Obj = JsonValue->AsObject();
            if (!Obj.IsValid())
            {
                return false;
            }

            double X = 0.0;
            double Y = 0.0;
            double Z = 0.0;
            bool bHasXYZ = Obj->TryGetNumberField(TEXT("x"), X) &&
                           Obj->TryGetNumberField(TEXT("y"), Y) &&
                           Obj->TryGetNumberField(TEXT("z"), Z);
            if (!bHasXYZ)
            {
                bHasXYZ = Obj->TryGetNumberField(TEXT("X"), X) &&
                          Obj->TryGetNumberField(TEXT("Y"), Y) &&
                          Obj->TryGetNumberField(TEXT("Z"), Z);
            }

            if (bHasXYZ)
            {
                OutVector = FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
                return true;
            }
        }

        return false;
    }

    static bool JsonValueToRotator(const TSharedPtr<FJsonValue>& JsonValue, FRotator& OutRotator)
    {
        if (!JsonValue.IsValid())
        {
            return false;
        }

        if (JsonValue->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& Values = JsonValue->AsArray();
            if (Values.Num() < 3)
            {
                return false;
            }

            OutRotator = FRotator(
                static_cast<float>(Values[0]->AsNumber()),
                static_cast<float>(Values[1]->AsNumber()),
                static_cast<float>(Values[2]->AsNumber())
            );
            return true;
        }

        if (JsonValue->Type == EJson::Object)
        {
            TSharedPtr<FJsonObject> Obj = JsonValue->AsObject();
            if (!Obj.IsValid())
            {
                return false;
            }

            double Pitch = 0.0;
            double Yaw = 0.0;
            double Roll = 0.0;
            bool bHasPYR = Obj->TryGetNumberField(TEXT("pitch"), Pitch) &&
                           Obj->TryGetNumberField(TEXT("yaw"), Yaw) &&
                           Obj->TryGetNumberField(TEXT("roll"), Roll);
            if (!bHasPYR)
            {
                bHasPYR = Obj->TryGetNumberField(TEXT("Pitch"), Pitch) &&
                          Obj->TryGetNumberField(TEXT("Yaw"), Yaw) &&
                          Obj->TryGetNumberField(TEXT("Roll"), Roll);
            }

            if (bHasPYR)
            {
                OutRotator = FRotator(static_cast<float>(Pitch), static_cast<float>(Yaw), static_cast<float>(Roll));
                return true;
            }
        }

        return false;
    }

    static UObject* ResolveTargetObject(const TSharedPtr<FJsonObject>& Params, FString& OutError)
    {
        if (!Params.IsValid())
        {
            OutError = TEXT("Missing params object");
            return nullptr;
        }

        FString ObjectPath;
        if (Params->TryGetStringField(TEXT("object_path"), ObjectPath))
        {
            UObject* TargetObject = StaticFindObject(UObject::StaticClass(), nullptr, *ObjectPath);
            if (!TargetObject)
            {
                TargetObject = LoadObject<UObject>(nullptr, *ObjectPath);
            }
            if (!TargetObject && ObjectPath.StartsWith(TEXT("/")) && !ObjectPath.Contains(TEXT(".")))
            {
                FString AssetName = FPaths::GetBaseFilename(ObjectPath);
                FString ExpandedPath = FString::Printf(TEXT("%s.%s"), *ObjectPath, *AssetName);
                TargetObject = StaticFindObject(UObject::StaticClass(), nullptr, *ExpandedPath);
                if (!TargetObject)
                {
                    TargetObject = LoadObject<UObject>(nullptr, *ExpandedPath);
                }
            }

            if (TargetObject)
            {
                return TargetObject;
            }

            OutError = FString::Printf(TEXT("Failed to resolve object_path: %s"), *ObjectPath);
            return nullptr;
        }

        FString ActorName;
        if (Params->TryGetStringField(TEXT("actor_name"), ActorName))
        {
            UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
            if (!World)
            {
                OutError = TEXT("Failed to resolve editor world for actor lookup");
                return nullptr;
            }

            TArray<AActor*> AllActors;
            UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
            for (AActor* Actor : AllActors)
            {
                if (Actor && Actor->GetName() == ActorName)
                {
                    return Actor;
                }
            }

            OutError = FString::Printf(TEXT("Actor not found: %s"), *ActorName);
            return nullptr;
        }

        OutError = TEXT("Missing target object. Provide 'object_path' or 'actor_name'");
        return nullptr;
    }

    static bool SetPropertyFromJsonValue(FProperty* Property, void* ValuePtr, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError)
    {
        if (!Property || !ValuePtr || !JsonValue.IsValid())
        {
            OutError = TEXT("Invalid property/value input");
            return false;
        }

        if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
        {
            if (JsonValue->Type == EJson::Boolean)
            {
                BoolProp->SetPropertyValue(ValuePtr, JsonValue->AsBool());
                return true;
            }
            if (JsonValue->Type == EJson::Number)
            {
                BoolProp->SetPropertyValue(ValuePtr, JsonValue->AsNumber() != 0.0);
                return true;
            }
            OutError = TEXT("Expected bool or number");
            return false;
        }

        if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
        {
            IntProp->SetPropertyValue(ValuePtr, static_cast<int32>(JsonValue->AsNumber()));
            return true;
        }

        if (FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
        {
            Int64Prop->SetPropertyValue(ValuePtr, static_cast<int64>(JsonValue->AsNumber()));
            return true;
        }

        if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
        {
            FloatProp->SetPropertyValue(ValuePtr, static_cast<float>(JsonValue->AsNumber()));
            return true;
        }

        if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
        {
            DoubleProp->SetPropertyValue(ValuePtr, JsonValue->AsNumber());
            return true;
        }

        if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
        {
            StrProp->SetPropertyValue(ValuePtr, JsonValue->AsString());
            return true;
        }

        if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
        {
            NameProp->SetPropertyValue(ValuePtr, FName(*JsonValue->AsString()));
            return true;
        }

        if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
        {
            TextProp->SetPropertyValue(ValuePtr, FText::FromString(JsonValue->AsString()));
            return true;
        }

        if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
        {
            UEnum* EnumDef = ByteProp->GetIntPropertyEnum();
            if (EnumDef && JsonValue->Type == EJson::String)
            {
                FString EnumValueName = JsonValue->AsString();
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    OutError = FString::Printf(TEXT("Enum value not found: %s"), *EnumValueName);
                    return false;
                }
                ByteProp->SetPropertyValue(ValuePtr, static_cast<uint8>(EnumValue));
                return true;
            }
            ByteProp->SetPropertyValue(ValuePtr, static_cast<uint8>(JsonValue->AsNumber()));
            return true;
        }

        if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
        {
            UEnum* EnumDef = EnumProp->GetEnum();
            FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
            if (!EnumDef || !Underlying)
            {
                OutError = TEXT("Invalid enum property");
                return false;
            }
            if (JsonValue->Type == EJson::String)
            {
                FString EnumValueName = JsonValue->AsString();
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    OutError = FString::Printf(TEXT("Enum value not found: %s"), *EnumValueName);
                    return false;
                }
                Underlying->SetIntPropertyValue(ValuePtr, EnumValue);
                return true;
            }
            Underlying->SetIntPropertyValue(ValuePtr, static_cast<int64>(JsonValue->AsNumber()));
            return true;
        }

        if (FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property))
        {
            if (JsonValue->Type == EJson::Null)
            {
                ObjectProp->SetObjectPropertyValue(ValuePtr, nullptr);
                return true;
            }
            FString ObjectPath = JsonValue->AsString();
            UClass* ExpectedClass = ObjectProp->PropertyClass ? ObjectProp->PropertyClass : UObject::StaticClass();
            UObject* ReferencedObject = StaticFindObject(ExpectedClass, nullptr, *ObjectPath);
            if (!ReferencedObject)
            {
                ReferencedObject = LoadObject<UObject>(nullptr, *ObjectPath);
            }
            if (!ReferencedObject && ObjectPath.StartsWith(TEXT("/")) && !ObjectPath.Contains(TEXT(".")))
            {
                FString AssetName = FPaths::GetBaseFilename(ObjectPath);
                FString ExpandedPath = FString::Printf(TEXT("%s.%s"), *ObjectPath, *AssetName);
                ReferencedObject = StaticFindObject(ExpectedClass, nullptr, *ExpandedPath);
                if (!ReferencedObject)
                {
                    ReferencedObject = LoadObject<UObject>(nullptr, *ExpandedPath);
                }
            }
            if (!ReferencedObject)
            {
                OutError = FString::Printf(TEXT("Failed to resolve object reference: %s"), *ObjectPath);
                return false;
            }
            if (!ReferencedObject->IsA(ExpectedClass))
            {
                OutError = FString::Printf(TEXT("Object '%s' is not of expected class '%s'"), *ObjectPath, *ExpectedClass->GetName());
                return false;
            }
            ObjectProp->SetObjectPropertyValue(ValuePtr, ReferencedObject);
            return true;
        }

        if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
        {
            if (StructProp->Struct == TBaseStructure<FVector>::Get())
            {
                FVector ParsedVector;
                if (!JsonValueToVector(JsonValue, ParsedVector))
                {
                    OutError = TEXT("Invalid FVector value");
                    return false;
                }
                *reinterpret_cast<FVector*>(ValuePtr) = ParsedVector;
                return true;
            }
            if (StructProp->Struct == TBaseStructure<FRotator>::Get())
            {
                FRotator ParsedRotator;
                if (!JsonValueToRotator(JsonValue, ParsedRotator))
                {
                    OutError = TEXT("Invalid FRotator value");
                    return false;
                }
                *reinterpret_cast<FRotator*>(ValuePtr) = ParsedRotator;
                return true;
            }
            if (StructProp->Struct == TBaseStructure<FTransform>::Get())
            {
                if (JsonValue->Type != EJson::Object)
                {
                    OutError = TEXT("FTransform expects object with location/rotation/scale");
                    return false;
                }
                TSharedPtr<FJsonObject> TransformObj = JsonValue->AsObject();
                if (!TransformObj.IsValid())
                {
                    OutError = TEXT("Invalid FTransform object");
                    return false;
                }
                FTransform ParsedTransform = *reinterpret_cast<FTransform*>(ValuePtr);
                if (TransformObj->HasField(TEXT("location")))
                {
                    FVector ParsedLocation;
                    if (!JsonValueToVector(TransformObj->TryGetField(TEXT("location")), ParsedLocation))
                    {
                        OutError = TEXT("Invalid transform.location");
                        return false;
                    }
                    ParsedTransform.SetLocation(ParsedLocation);
                }
                if (TransformObj->HasField(TEXT("rotation")))
                {
                    FRotator ParsedRotation;
                    if (!JsonValueToRotator(TransformObj->TryGetField(TEXT("rotation")), ParsedRotation))
                    {
                        OutError = TEXT("Invalid transform.rotation");
                        return false;
                    }
                    ParsedTransform.SetRotation(ParsedRotation.Quaternion());
                }
                if (TransformObj->HasField(TEXT("scale")))
                {
                    FVector ParsedScale;
                    if (!JsonValueToVector(TransformObj->TryGetField(TEXT("scale")), ParsedScale))
                    {
                        OutError = TEXT("Invalid transform.scale");
                        return false;
                    }
                    ParsedTransform.SetScale3D(ParsedScale);
                }
                *reinterpret_cast<FTransform*>(ValuePtr) = ParsedTransform;
                return true;
            }
        }

        OutError = FString::Printf(TEXT("Unsupported property type: %s"), *Property->GetClass()->GetName());
        return false;
    }

    static TSharedPtr<FJsonValue> PropertyValueToJson(FProperty* Property, const void* ValuePtr)
    {
        if (!Property || !ValuePtr)
        {
            return MakeShared<FJsonValueNull>();
        }

        if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
        {
            return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(ValuePtr));
        }
        if (const FIntProperty* IntProp = CastField<FIntProperty>(Property))
        {
            return MakeShared<FJsonValueNumber>(IntProp->GetPropertyValue(ValuePtr));
        }
        if (const FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
        {
            return MakeShared<FJsonValueNumber>(static_cast<double>(Int64Prop->GetPropertyValue(ValuePtr)));
        }
        if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
        {
            return MakeShared<FJsonValueNumber>(FloatProp->GetPropertyValue(ValuePtr));
        }
        if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
        {
            return MakeShared<FJsonValueNumber>(DoubleProp->GetPropertyValue(ValuePtr));
        }
        if (const FStrProperty* StrProp = CastField<FStrProperty>(Property))
        {
            return MakeShared<FJsonValueString>(StrProp->GetPropertyValue(ValuePtr));
        }
        if (const FNameProperty* NameProp = CastField<FNameProperty>(Property))
        {
            return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(ValuePtr).ToString());
        }
        if (const FTextProperty* TextProp = CastField<FTextProperty>(Property))
        {
            return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ValuePtr).ToString());
        }
        if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
        {
            uint8 ByteValue = ByteProp->GetPropertyValue(ValuePtr);
            if (UEnum* EnumDef = ByteProp->GetIntPropertyEnum())
            {
                return MakeShared<FJsonValueString>(EnumDef->GetNameStringByValue(ByteValue));
            }
            return MakeShared<FJsonValueNumber>(ByteValue);
        }
        if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
        {
            const FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
            int64 EnumValue = Underlying ? Underlying->GetSignedIntPropertyValue(ValuePtr) : 0;
            if (UEnum* EnumDef = EnumProp->GetEnum())
            {
                return MakeShared<FJsonValueString>(EnumDef->GetNameStringByValue(EnumValue));
            }
            return MakeShared<FJsonValueNumber>(static_cast<double>(EnumValue));
        }
        if (const FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property))
        {
            UObject* ReferencedObject = ObjectProp->GetObjectPropertyValue(ValuePtr);
            if (!ReferencedObject)
            {
                return MakeShared<FJsonValueNull>();
            }
            return MakeShared<FJsonValueString>(ReferencedObject->GetPathName());
        }
        if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
        {
            if (StructProp->Struct == TBaseStructure<FVector>::Get())
            {
                FVector Value = *reinterpret_cast<const FVector*>(ValuePtr);
                TArray<TSharedPtr<FJsonValue>> Arr;
                Arr.Add(MakeShared<FJsonValueNumber>(Value.X));
                Arr.Add(MakeShared<FJsonValueNumber>(Value.Y));
                Arr.Add(MakeShared<FJsonValueNumber>(Value.Z));
                return MakeShared<FJsonValueArray>(Arr);
            }
            if (StructProp->Struct == TBaseStructure<FRotator>::Get())
            {
                FRotator Value = *reinterpret_cast<const FRotator*>(ValuePtr);
                TArray<TSharedPtr<FJsonValue>> Arr;
                Arr.Add(MakeShared<FJsonValueNumber>(Value.Pitch));
                Arr.Add(MakeShared<FJsonValueNumber>(Value.Yaw));
                Arr.Add(MakeShared<FJsonValueNumber>(Value.Roll));
                return MakeShared<FJsonValueArray>(Arr);
            }
            if (StructProp->Struct == TBaseStructure<FTransform>::Get())
            {
                FTransform Value = *reinterpret_cast<const FTransform*>(ValuePtr);
                TSharedPtr<FJsonObject> TransformObj = MakeShared<FJsonObject>();
                FVector Location = Value.GetLocation();
                FRotator Rotation = Value.GetRotation().Rotator();
                FVector Scale = Value.GetScale3D();

                TArray<TSharedPtr<FJsonValue>> LocationArr;
                LocationArr.Add(MakeShared<FJsonValueNumber>(Location.X));
                LocationArr.Add(MakeShared<FJsonValueNumber>(Location.Y));
                LocationArr.Add(MakeShared<FJsonValueNumber>(Location.Z));
                TransformObj->SetArrayField(TEXT("location"), LocationArr);

                TArray<TSharedPtr<FJsonValue>> RotationArr;
                RotationArr.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
                RotationArr.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
                RotationArr.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
                TransformObj->SetArrayField(TEXT("rotation"), RotationArr);

                TArray<TSharedPtr<FJsonValue>> ScaleArr;
                ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.X));
                ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.Y));
                ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.Z));
                TransformObj->SetArrayField(TEXT("scale"), ScaleArr);

                return MakeShared<FJsonValueObject>(TransformObj);
            }
        }

        return MakeShared<FJsonValueString>(FString::Printf(TEXT("<unsupported_property_type:%s>"), *Property->GetClass()->GetName()));
    }
}

// ============================================================================
// Constructor
// ============================================================================
FEpicUnrealMCPEditorCommands::FEpicUnrealMCPEditorCommands()
{
}

// ============================================================================
// Command dispatch
// ============================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Actor manipulation
    if (CommandType == TEXT("get_actors_in_level")) { return HandleGetActorsInLevel(Params); }
    else if (CommandType == TEXT("find_actors_by_name")) { return HandleFindActorsByName(Params); }
    else if (CommandType == TEXT("spawn_actor")) { return HandleSpawnActor(Params); }
    else if (CommandType == TEXT("delete_actor")) { return HandleDeleteActor(Params); }
    else if (CommandType == TEXT("set_actor_transform")) { return HandleSetActorTransform(Params); }
    else if (CommandType == TEXT("spawn_blueprint_actor")) { return HandleSpawnBlueprintActor(Params); }
    // Editor lifecycle
    else if (CommandType == TEXT("request_editor_exit")) { return HandleRequestEditorExit(Params); }
    else if (CommandType == TEXT("restart_editor")) { return HandleRestartEditor(Params); }
    else if (CommandType == TEXT("get_editor_state")) { return HandleGetEditorState(Params); }
    else if (CommandType == TEXT("get_engine_version")) { return HandleGetEngineVersion(Params); }
    else if (CommandType == TEXT("save_all")) { return HandleSaveAll(Params); }
    // PIE / Simulation
    else if (CommandType == TEXT("play_in_editor_start")) { return HandlePlayInEditorStart(Params); }
    else if (CommandType == TEXT("play_in_editor_stop")) { return HandlePlayInEditorStop(Params); }
    else if (CommandType == TEXT("simulate_start")) { return HandleSimulateStart(Params); }
    else if (CommandType == TEXT("simulate_stop")) { return HandleSimulateStop(Params); }
    else if (CommandType == TEXT("pause_game")) { return HandlePauseGame(Params); }
    // Level lifecycle
    else if (CommandType == TEXT("list_levels")) { return HandleListLevels(Params); }
    else if (CommandType == TEXT("open_level")) { return HandleOpenLevel(Params); }
    else if (CommandType == TEXT("save_level")) { return HandleSaveLevel(Params); }
    else if (CommandType == TEXT("create_level")) { return HandleCreateLevel(Params); }
    else if (CommandType == TEXT("duplicate_level")) { return HandleDuplicateLevel(Params); }
    else if (CommandType == TEXT("delete_level")) { return HandleDeleteLevel(Params); }
    else if (CommandType == TEXT("set_current_world")) { return HandleSetCurrentWorld(Params); }
    else if (CommandType == TEXT("add_sublevel")) { return HandleAddSublevel(Params); }
    else if (CommandType == TEXT("remove_sublevel")) { return HandleRemoveSublevel(Params); }
    else if (CommandType == TEXT("toggle_sublevel_visibility")) { return HandleToggleSublevelVisibility(Params); }
    // Selection and viewport
    else if (CommandType == TEXT("get_selected_actors")) { return HandleGetSelectedActors(Params); }
    else if (CommandType == TEXT("select_actors")) { return HandleSelectActors(Params); }
    else if (CommandType == TEXT("focus_viewport_on_selection")) { return HandleFocusViewportOnSelection(Params); }
    else if (CommandType == TEXT("set_viewport_camera")) { return HandleSetViewportCamera(Params); }
    else if (CommandType == TEXT("capture_viewport_screenshot")) { return HandleCaptureViewportScreenshot(Params); }
    else if (CommandType == TEXT("get_viewport_stats")) { return HandleGetViewportStats(Params); }
    // Generic reflection
    else if (CommandType == TEXT("get_object_properties")) { return HandleGetObjectProperties(Params); }
    else if (CommandType == TEXT("set_object_properties")) { return HandleSetObjectProperties(Params); }
    else if (CommandType == TEXT("call_uobject_function")) { return HandleCallUObjectFunction(Params); }
    else if (CommandType == TEXT("build_navmesh")) { return HandleBuildNavMesh(Params); }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        MCPErrorCodes::UNKNOWN_COMMAND,
        FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

// ============================================================================
// Actor manipulation commands (existing)
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor)
        {
            ActorArray.Add(FEpicUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName().Contains(Pattern))
        {
            MatchingActors.Add(FEpicUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location"))) { Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")); }
    if (Params->HasField(TEXT("rotation"))) { Rotation = FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")); }
    if (Params->HasField(TEXT("scale")))    { Scale = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")); }

    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world")); }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    if (ActorType == TEXT("StaticMeshActor"))
    {
        AStaticMeshActor* NewMeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
        if (NewMeshActor)
        {
            FString MeshPath;
            if (Params->TryGetStringField(TEXT("static_mesh"), MeshPath))
            {
                UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
                if (Mesh) { NewMeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh); }
            }
        }
        NewActor = NewMeshActor;
    }
    else if (ActorType == TEXT("PointLight")) { NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams); }
    else if (ActorType == TEXT("SpotLight")) { NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams); }
    else if (ActorType == TEXT("DirectionalLight")) { NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams); }
    else if (ActorType == TEXT("CameraActor")) { NewActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams); }
    else { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType)); }

    if (NewActor)
    {
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);
        return FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TSharedPtr<FJsonObject> ActorInfo = FEpicUnrealMCPCommonUtils::ActorToJsonObject(Actor);
            Actor->Destroy();
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
            return ResultObj;
        }
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName) { TargetActor = Actor; break; }
    }

    if (!TargetActor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName)); }

    FTransform NewTransform = TargetActor->GetTransform();
    if (Params->HasField(TEXT("location"))) { NewTransform.SetLocation(FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"))); }
    if (Params->HasField(TEXT("rotation"))) { NewTransform.SetRotation(FQuat(FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")))); }
    if (Params->HasField(TEXT("scale")))    { NewTransform.SetScale3D(FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"))); }

    TargetActor->SetActorTransform(NewTransform);
    return FEpicUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    FEpicUnrealMCPBlueprintCommands BlueprintCommands;
    return BlueprintCommands.HandleCommand(TEXT("spawn_blueprint_actor"), Params);
}

// ============================================================================
// Editor lifecycle commands (existing + Phase 1 new)
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleRequestEditorExit(const TSharedPtr<FJsonObject>& Params)
{
    bool bForce = false;
    if (Params.IsValid()) { Params->TryGetBoolField(TEXT("force"), bForce); }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("accepted"), true);
    ResultObj->SetBoolField(TEXT("force"), bForce);
    ResultObj->SetStringField(TEXT("message"), TEXT("Editor exit requested"));
    FPlatformMisc::RequestExit(bForce);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleRestartEditor(const TSharedPtr<FJsonObject>& Params)
{
    FString AdditionalArgs;
    if (Params.IsValid()) { Params->TryGetStringField(TEXT("additional_args"), AdditionalArgs); }

    const FString ExecutablePath = FPlatformProcess::ExecutablePath();
    const FString ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());

    if (ExecutablePath.IsEmpty()) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to determine Unreal Editor executable path")); }
    if (ProjectPath.IsEmpty()) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to determine current project path")); }

    FString LaunchArgs = FString::Printf(TEXT("\"%s\""), *ProjectPath);
    if (!AdditionalArgs.IsEmpty()) { LaunchArgs += TEXT(" ") + AdditionalArgs; }

    FProcHandle NewEditorProcess = FPlatformProcess::CreateProc(*ExecutablePath, *LaunchArgs, true, false, false, nullptr, 0, nullptr, nullptr);
    if (!NewEditorProcess.IsValid()) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to launch replacement Unreal Editor process")); }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("accepted"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("Editor restart requested"));
    ResultObj->SetStringField(TEXT("executable_path"), ExecutablePath);
    ResultObj->SetStringField(TEXT("project_path"), ProjectPath);
    ResultObj->SetStringField(TEXT("launch_args"), LaunchArgs);
    FPlatformMisc::RequestExit(false);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetEditorState(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();

    FString Mode = TEXT("editing");
    bool bIsPaused = false;

    if (GEditor && GEditor->IsPlaySessionInProgress())
    {
        if (GEditor->bIsSimulatingInEditor)
        {
            Mode = TEXT("simulating");
        }
        else
        {
            Mode = TEXT("pie");
        }

        if (GEditor->PlayWorld)
        {
            bIsPaused = GEditor->PlayWorld->IsPaused();
        }
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    FString MapName = World ? World->GetMapName() : TEXT("Unknown");
    FString ProjectName = FApp::GetProjectName();

    ResultObj->SetStringField(TEXT("mode"), Mode);
    ResultObj->SetStringField(TEXT("project_name"), ProjectName);
    ResultObj->SetStringField(TEXT("map_name"), MapName);
    ResultObj->SetBoolField(TEXT("is_paused"), bIsPaused);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetEngineVersion(const TSharedPtr<FJsonObject>& Params)
{
    const FEngineVersion& Version = FEngineVersion::Current();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetNumberField(TEXT("major"), Version.GetMajor());
    ResultObj->SetNumberField(TEXT("minor"), Version.GetMinor());
    ResultObj->SetNumberField(TEXT("patch"), Version.GetPatch());
    ResultObj->SetNumberField(TEXT("changelist"), Version.GetChangelist());
    ResultObj->SetStringField(TEXT("branch"), Version.GetBranch());
    ResultObj->SetStringField(TEXT("full_string"), Version.ToString());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSaveAll(const TSharedPtr<FJsonObject>& Params)
{
    bool bResult = FEditorFileUtils::SaveDirtyPackages(
        /*bPromptUserToSave=*/ false,
        /*bSaveMapPackages=*/ true,
        /*bSaveContentPackages=*/ true);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bResult);
    ResultObj->SetStringField(TEXT("message"), bResult ? TEXT("All dirty packages saved") : TEXT("Save completed with warnings"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandlePlayInEditorStart(const TSharedPtr<FJsonObject>& Params)
{
    if (!GUnrealEd)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::EDITOR_BUSY, TEXT("GUnrealEd not available"));
    }

    if (GEditor && GEditor->IsPlaySessionInProgress())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::EDITOR_BUSY, TEXT("A play session is already in progress"));
    }

    FRequestPlaySessionParams PlayParams;
    PlayParams.WorldType = EPlaySessionWorldType::PlayInEditor;

    GUnrealEd->RequestPlaySession(PlayParams);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("Play In Editor session requested"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandlePlayInEditorStop(const TSharedPtr<FJsonObject>& Params)
{
    if (!GUnrealEd)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::EDITOR_BUSY, TEXT("GUnrealEd not available"));
    }

    if (!GEditor || !GEditor->IsPlaySessionInProgress())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, TEXT("No play session is currently active"));
    }

    GUnrealEd->RequestEndPlayMap();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("Play session stop requested"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSimulateStart(const TSharedPtr<FJsonObject>& Params)
{
    if (!GUnrealEd)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::EDITOR_BUSY, TEXT("GUnrealEd not available"));
    }

    if (GEditor && GEditor->IsPlaySessionInProgress())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::EDITOR_BUSY, TEXT("A play/simulate session is already in progress"));
    }

    FRequestPlaySessionParams SimParams;
    SimParams.WorldType = EPlaySessionWorldType::SimulateInEditor;

    GUnrealEd->RequestPlaySession(SimParams);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("Simulate In Editor session requested"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSimulateStop(const TSharedPtr<FJsonObject>& Params)
{
    if (!GUnrealEd)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::EDITOR_BUSY, TEXT("GUnrealEd not available"));
    }

    if (!GEditor || !GEditor->IsPlaySessionInProgress())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, TEXT("No simulation session is currently active"));
    }

    GUnrealEd->RequestEndPlayMap();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("Simulation stop requested"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandlePauseGame(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor || !GEditor->IsPlaySessionInProgress() || !GEditor->PlayWorld)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, TEXT("No active play session to pause/unpause"));
    }

    // Optional explicit pause state; if not provided, toggle
    bool bNewPaused;
    bool bExplicit = false;
    if (Params.IsValid())
    {
        bExplicit = Params->TryGetBoolField(TEXT("pause"), bNewPaused);
    }

    if (!bExplicit)
    {
        bNewPaused = !GEditor->PlayWorld->IsPaused();
    }

    GEditor->PlayWorld->bDebugPauseExecution = bNewPaused;

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("paused"), bNewPaused);
    ResultObj->SetStringField(TEXT("message"), bNewPaused ? TEXT("Game paused") : TEXT("Game resumed"));
    return ResultObj;
}

// ============================================================================
// Level lifecycle commands (Phase 1 new)
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleListLevels(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("persistent_level"), World->GetMapName());

    TArray<TSharedPtr<FJsonValue>> StreamingLevels;
    const TArray<ULevelStreaming*>& Levels = World->GetStreamingLevels();
    for (const ULevelStreaming* StreamingLevel : Levels)
    {
        if (!StreamingLevel) continue;

        TSharedPtr<FJsonObject> LevelObj = MakeShared<FJsonObject>();
        LevelObj->SetStringField(TEXT("package_name"), StreamingLevel->GetWorldAssetPackageName());
        LevelObj->SetBoolField(TEXT("is_loaded"), StreamingLevel->IsLevelLoaded());
        LevelObj->SetBoolField(TEXT("is_visible"), StreamingLevel->IsLevelVisible());
        LevelObj->SetBoolField(TEXT("should_be_visible"), StreamingLevel->GetShouldBeVisibleFlag());
        StreamingLevels.Add(MakeShared<FJsonValueObject>(LevelObj));
    }

    ResultObj->SetArrayField(TEXT("streaming_levels"), StreamingLevels);
    ResultObj->SetNumberField(TEXT("streaming_level_count"), StreamingLevels.Num());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleOpenLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("level_path"), LevelPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'level_path' parameter"));
    }

    if (GEditor && GEditor->IsPlaySessionInProgress())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::EDITOR_BUSY, TEXT("Cannot open level during PIE/simulation"));
    }

    FString LoadErrors;
    if (!UEditorLoadingAndSavingUtils::LoadMap(LevelPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::NOT_FOUND, FString::Printf(TEXT("Failed to open level: %s"), *LevelPath));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("map_name"), World ? World->GetMapName() : TEXT("Unknown"));
    ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Opened level: %s"), *LevelPath));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSaveLevel(const TSharedPtr<FJsonObject>& Params)
{
    bool bResult = FEditorFileUtils::SaveCurrentLevel();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bResult);
    ResultObj->SetStringField(TEXT("message"), bResult ? TEXT("Current level saved") : TEXT("Failed to save current level"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCreateLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelName;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("name"), LevelName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'name' parameter"));
    }

    FString BasePath = TEXT("/Game/Maps/");
    if (Params->HasField(TEXT("path")))
    {
        Params->TryGetStringField(TEXT("path"), BasePath);
        if (!BasePath.EndsWith(TEXT("/"))) { BasePath += TEXT("/"); }
    }

    FString FullPath = BasePath + LevelName;

    // Check if asset already exists
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, FString::Printf(TEXT("Level already exists: %s"), *FullPath));
    }

    // Create a new blank map by loading the default empty map template
    UEditorLoadingAndSavingUtils::NewBlankMap(false);

    // Save it to the specified path
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    if (World)
    {
        FEditorFileUtils::SaveLevel(World->PersistentLevel, FullPath);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("level_path"), FullPath);
    ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Created level: %s"), *FullPath));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleDuplicateLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString SourcePath, DestPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("source_path"), SourcePath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'source_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("dest_path"), DestPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'dest_path' parameter"));
    }

    UObject* DuplicatedAsset = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestPath);
    if (!DuplicatedAsset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::NOT_FOUND, FString::Printf(TEXT("Failed to duplicate '%s' to '%s'"), *SourcePath, *DestPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("source_path"), SourcePath);
    ResultObj->SetStringField(TEXT("dest_path"), DestPath);
    ResultObj->SetStringField(TEXT("message"), TEXT("Level duplicated successfully"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleDeleteLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("level_path"), LevelPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'level_path' parameter"));
    }

    // Prevent deleting the currently loaded level
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    if (World)
    {
        FString CurrentMapPath = World->GetOutermost()->GetName();
        if (LevelPath == CurrentMapPath || LevelPath == World->GetMapName())
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, TEXT("Cannot delete the currently loaded level"));
        }
    }

    bool bDeleted = UEditorAssetLibrary::DeleteAsset(LevelPath);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bDeleted);
    ResultObj->SetStringField(TEXT("message"), bDeleted ? TEXT("Level deleted") : TEXT("Failed to delete level"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetCurrentWorld(const TSharedPtr<FJsonObject>& Params)
{
    // set_current_world is semantically equivalent to open_level for the persistent level
    return HandleOpenLevel(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleAddSublevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("level_path"), LevelPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'level_path' parameter"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    ULevelStreaming* NewStreamingLevel = UEditorLevelUtils::AddLevelToWorld(
        World, *LevelPath, ULevelStreamingDynamic::StaticClass());

    if (!NewStreamingLevel)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::NOT_FOUND, FString::Printf(TEXT("Failed to add sublevel: %s"), *LevelPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("level_path"), LevelPath);
    ResultObj->SetStringField(TEXT("message"), TEXT("Sublevel added"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleRemoveSublevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelName;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("level_name"), LevelName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'level_name' parameter"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
    {
        if (!StreamingLevel) continue;

        if (StreamingLevel->GetWorldAssetPackageName().Contains(LevelName))
        {
            ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel();
            if (LoadedLevel)
            {
                UEditorLevelUtils::RemoveLevelFromWorld(LoadedLevel);
            }

            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetBoolField(TEXT("success"), true);
            ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Sublevel removed: %s"), *LevelName));
            return ResultObj;
        }
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::NOT_FOUND, FString::Printf(TEXT("Sublevel not found: %s"), *LevelName));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleToggleSublevelVisibility(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelName;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("level_name"), LevelName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'level_name' parameter"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
    {
        if (!StreamingLevel) continue;

        if (StreamingLevel->GetWorldAssetPackageName().Contains(LevelName))
        {
            ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel();
            if (!LoadedLevel)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, FString::Printf(TEXT("Level '%s' is not loaded"), *LevelName));
            }

            // Check for explicit visible parameter, otherwise toggle
            bool bNewVisible;
            bool bExplicit = Params->TryGetBoolField(TEXT("visible"), bNewVisible);
            if (!bExplicit)
            {
                bNewVisible = !StreamingLevel->IsLevelVisible();
            }

            UEditorLevelUtils::SetLevelVisibility(LoadedLevel, bNewVisible, false);

            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetBoolField(TEXT("success"), true);
            ResultObj->SetBoolField(TEXT("visible"), bNewVisible);
            ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Sublevel '%s' visibility set to %s"), *LevelName, bNewVisible ? TEXT("visible") : TEXT("hidden")));
            return ResultObj;
        }
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::NOT_FOUND, FString::Printf(TEXT("Sublevel not found: %s"), *LevelName));
}

// ============================================================================
// Selection and viewport commands (Phase 1 new)
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available"));
    }

    USelection* Selection = GEditor->GetSelectedActors();
    TArray<TSharedPtr<FJsonValue>> SelectedArray;

    if (Selection)
    {
        for (int32 i = 0; i < Selection->Num(); ++i)
        {
            AActor* Actor = Cast<AActor>(Selection->GetSelectedObject(i));
            if (Actor)
            {
                SelectedArray.Add(FEpicUnrealMCPCommonUtils::ActorToJson(Actor));
            }
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), SelectedArray);
    ResultObj->SetNumberField(TEXT("count"), SelectedArray.Num());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSelectActors(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available"));
    }

    bool bAddToSelection = false;
    if (Params.IsValid()) { Params->TryGetBoolField(TEXT("add_to_selection"), bAddToSelection); }

    if (!bAddToSelection)
    {
        GEditor->SelectNone(true, true, false);
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world")); }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

    int32 SelectedCount = 0;

    // Select by names
    const TArray<TSharedPtr<FJsonValue>>* ActorNames = nullptr;
    if (Params.IsValid() && Params->TryGetArrayField(TEXT("actor_names"), ActorNames) && ActorNames)
    {
        for (const TSharedPtr<FJsonValue>& NameVal : *ActorNames)
        {
            if (!NameVal.IsValid()) continue;
            FString Name = NameVal->AsString();
            for (AActor* Actor : AllActors)
            {
                if (Actor && Actor->GetName() == Name)
                {
                    GEditor->SelectActor(Actor, true, true);
                    SelectedCount++;
                    break;
                }
            }
        }
    }

    // Select by class
    FString ActorClass;
    if (Params.IsValid() && Params->TryGetStringField(TEXT("actor_class"), ActorClass))
    {
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetClass()->GetName().Contains(ActorClass))
            {
                GEditor->SelectActor(Actor, true, true);
                SelectedCount++;
            }
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetNumberField(TEXT("selected_count"), SelectedCount);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleFocusViewportOnSelection(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available"));
    }

    USelection* Selection = GEditor->GetSelectedActors();
    TArray<AActor*> SelectedActors;
    if (Selection)
    {
        for (int32 i = 0; i < Selection->Num(); ++i)
        {
            AActor* Actor = Cast<AActor>(Selection->GetSelectedObject(i));
            if (Actor) { SelectedActors.Add(Actor); }
        }
    }

    if (SelectedActors.Num() == 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, TEXT("No actors selected to focus on"));
    }

    GEditor->MoveViewportCamerasToActor(SelectedActors, false);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Viewport focused on %d selected actor(s)"), SelectedActors.Num()));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetViewportCamera(const TSharedPtr<FJsonObject>& Params)
{
    FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
    if (!ViewportClient)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active level editing viewport"));
    }

    if (Params->HasField(TEXT("location")))
    {
        FVector Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
        ViewportClient->SetViewLocation(Location);
    }

    if (Params->HasField(TEXT("rotation")))
    {
        FRotator Rotation = FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
        ViewportClient->SetViewRotation(Rotation);
    }

    double FOV = 0.0;
    if (Params->TryGetNumberField(TEXT("fov"), FOV) && FOV > 0.0)
    {
        ViewportClient->ViewFOV = static_cast<float>(FOV);
    }

    ViewportClient->Invalidate();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("Viewport camera updated"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCaptureViewportScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    FString Filename;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("filename"), Filename))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'filename' parameter"));
    }

    // Ensure path is absolute
    if (FPaths::IsRelative(Filename))
    {
        Filename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), Filename);
    }

    // Ensure .png extension
    if (!Filename.EndsWith(TEXT(".png")))
    {
        Filename += TEXT(".png");
    }

    FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
    if (!ViewportClient || !ViewportClient->Viewport)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active viewport for screenshot"));
    }

    FViewport* Viewport = ViewportClient->Viewport;
    TArray<FColor> Bitmap;
    if (!Viewport->ReadPixels(Bitmap))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to read viewport pixels"));
    }

    int32 Width = Viewport->GetSizeXY().X;
    int32 Height = Viewport->GetSizeXY().Y;

    TArray<uint8> CompressedData;
    FImageUtils::PNGCompressImageArray(Width, Height, Bitmap, CompressedData);

    if (!FFileHelper::SaveArrayToFile(CompressedData, *Filename))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to save screenshot to: %s"), *Filename));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("file_path"), Filename);
    ResultObj->SetNumberField(TEXT("width"), Width);
    ResultObj->SetNumberField(TEXT("height"), Height);
    ResultObj->SetStringField(TEXT("message"), TEXT("Screenshot captured"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetViewportStats(const TSharedPtr<FJsonObject>& Params)
{
    FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
    if (!ViewportClient)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active level editing viewport"));
    }

    FVector Location = ViewportClient->GetViewLocation();
    FRotator Rotation = ViewportClient->GetViewRotation();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();

    TArray<TSharedPtr<FJsonValue>> LocationArr;
    LocationArr.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArr.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArr.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ResultObj->SetArrayField(TEXT("location"), LocationArr);

    TArray<TSharedPtr<FJsonValue>> RotationArr;
    RotationArr.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArr.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArr.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ResultObj->SetArrayField(TEXT("rotation"), RotationArr);

    ResultObj->SetNumberField(TEXT("fov"), ViewportClient->ViewFOV);

    if (ViewportClient->Viewport)
    {
        ResultObj->SetNumberField(TEXT("width"), ViewportClient->Viewport->GetSizeXY().X);
        ResultObj->SetNumberField(TEXT("height"), ViewportClient->Viewport->GetSizeXY().Y);
    }

    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// ============================================================================
// Generic object reflection commands (merged from standalone)
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleBuildNavMesh(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    UClass* NavSysClass = FindObject<UClass>(nullptr, TEXT("/Script/NavigationSystem.NavigationSystemV1"));
    if (!NavSysClass) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("NavigationSystemV1 class not found. Is NavigationSystem module loaded?"));

    UObject* NavSys = World->GetNavigationSystem();
    if (!NavSys) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Navigation System not found in world"));

    UFunction* BuildFunc = NavSys->FindFunction(TEXT("Build"));
    if (!BuildFunc) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("NavigationSystemV1::Build() function not found"));

    NavSys->ProcessEvent(BuildFunc, nullptr);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("NavMesh build triggered"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetObjectProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString ResolveError;
    UObject* TargetObject = ResolveTargetObject(Params, ResolveError);
    if (!TargetObject)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(ResolveError);
    }

    TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();
    const TArray<TSharedPtr<FJsonValue>>* RequestedPropertyNames = nullptr;
    const bool bHasFilter = Params.IsValid() &&
                            Params->TryGetArrayField(TEXT("property_names"), RequestedPropertyNames) &&
                            RequestedPropertyNames;

    if (bHasFilter)
    {
        for (const TSharedPtr<FJsonValue>& NameValue : *RequestedPropertyNames)
        {
            if (!NameValue.IsValid() || NameValue->Type != EJson::String) continue;
            const FString PropertyName = NameValue->AsString();
            FProperty* Property = TargetObject->GetClass()->FindPropertyByName(*PropertyName);
            if (!Property) { PropertiesObj->SetField(PropertyName, MakeShared<FJsonValueNull>()); continue; }
            void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);
            PropertiesObj->SetField(PropertyName, PropertyValueToJson(Property, PropertyValuePtr));
        }
    }
    else
    {
        for (TFieldIterator<FProperty> It(TargetObject->GetClass()); It; ++It)
        {
            FProperty* Property = *It;
            if (!Property) continue;
            void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);
            PropertiesObj->SetField(Property->GetName(), PropertyValueToJson(Property, PropertyValuePtr));
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("object_name"), TargetObject->GetName());
    ResultObj->SetStringField(TEXT("object_class"), TargetObject->GetClass()->GetName());
    ResultObj->SetStringField(TEXT("object_path"), TargetObject->GetPathName());
    ResultObj->SetObjectField(TEXT("properties"), PropertiesObj);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetObjectProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString ResolveError;
    UObject* TargetObject = ResolveTargetObject(Params, ResolveError);
    if (!TargetObject) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(ResolveError); }

    const TSharedPtr<FJsonObject>* PropertiesToSetPtr = nullptr;
    if (!Params.IsValid() || !Params->TryGetObjectField(TEXT("properties"), PropertiesToSetPtr) || !PropertiesToSetPtr || !PropertiesToSetPtr->IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'properties' object"));
    }

    const TSharedPtr<FJsonObject>& PropertiesToSet = *PropertiesToSetPtr;
    TArray<TSharedPtr<FJsonValue>> UpdatedProperties;
    TSharedPtr<FJsonObject> FailedProperties = MakeShared<FJsonObject>();

    TargetObject->Modify();

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : PropertiesToSet->Values)
    {
        const FString& PropertyName = Pair.Key;
        FProperty* Property = TargetObject->GetClass()->FindPropertyByName(*PropertyName);
        if (!Property) { FailedProperties->SetStringField(PropertyName, FString::Printf(TEXT("Property not found: %s"), *PropertyName)); continue; }
        void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);
        FString SetError;
        if (!SetPropertyFromJsonValue(Property, PropertyValuePtr, Pair.Value, SetError)) { FailedProperties->SetStringField(PropertyName, SetError); continue; }
        UpdatedProperties.Add(MakeShared<FJsonValueString>(PropertyName));
    }

    TargetObject->PostEditChange();
    TargetObject->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("object_name"), TargetObject->GetName());
    ResultObj->SetStringField(TEXT("object_class"), TargetObject->GetClass()->GetName());
    ResultObj->SetStringField(TEXT("object_path"), TargetObject->GetPathName());
    ResultObj->SetArrayField(TEXT("updated_properties"), UpdatedProperties);
    ResultObj->SetNumberField(TEXT("updated_count"), UpdatedProperties.Num());

    if (FailedProperties->Values.Num() > 0)
    {
        ResultObj->SetObjectField(TEXT("failed_properties"), FailedProperties);
        ResultObj->SetNumberField(TEXT("failed_count"), FailedProperties->Values.Num());
    }
    else
    {
        ResultObj->SetNumberField(TEXT("failed_count"), 0);
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCallUObjectFunction(const TSharedPtr<FJsonObject>& Params)
{
    FString ResolveError;
    UObject* TargetObject = ResolveTargetObject(Params, ResolveError);
    if (!TargetObject) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(ResolveError); }

    FString FunctionName;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    UFunction* Function = TargetObject->FindFunction(FName(*FunctionName));
    if (!Function)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Function '%s' not found on object '%s'"), *FunctionName, *TargetObject->GetName()));
    }

    TSharedPtr<FJsonObject> ArgumentsObj = MakeShared<FJsonObject>();
    const TSharedPtr<FJsonObject>* ArgumentsObjPtr = nullptr;
    if (Params->TryGetObjectField(TEXT("arguments"), ArgumentsObjPtr) && ArgumentsObjPtr && ArgumentsObjPtr->IsValid())
    {
        ArgumentsObj = *ArgumentsObjPtr;
    }

    uint8* ParamsBuffer = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
    FMemory::Memzero(ParamsBuffer, Function->ParmsSize);

    for (TFieldIterator<FProperty> It(Function); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
        const FString ParamName = Property->GetName();
        if (!ArgumentsObj->HasField(ParamName)) continue;
        TSharedPtr<FJsonValue> ParamValue = ArgumentsObj->TryGetField(ParamName);
        if (!ParamValue.IsValid()) continue;
        void* ParamValuePtr = Property->ContainerPtrToValuePtr<void>(ParamsBuffer);
        FString SetError;
        if (!SetPropertyFromJsonValue(Property, ParamValuePtr, ParamValue, SetError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Failed to set parameter '%s': %s"), *ParamName, *SetError));
        }
    }

    TargetObject->ProcessEvent(Function, ParamsBuffer);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("object_name"), TargetObject->GetName());
    ResultObj->SetStringField(TEXT("object_class"), TargetObject->GetClass()->GetName());
    ResultObj->SetStringField(TEXT("object_path"), TargetObject->GetPathName());
    ResultObj->SetStringField(TEXT("function_name"), FunctionName);

    TSharedPtr<FJsonObject> OutParamsObj = MakeShared<FJsonObject>();
    for (TFieldIterator<FProperty> It(Function); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm)) continue;
        void* ParamValuePtr = Property->ContainerPtrToValuePtr<void>(ParamsBuffer);
        TSharedPtr<FJsonValue> JsonValue = PropertyValueToJson(Property, ParamValuePtr);

        if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
        {
            ResultObj->SetField(TEXT("return_value"), JsonValue);
        }
        else if (Property->HasAnyPropertyFlags(CPF_OutParm))
        {
            OutParamsObj->SetField(Property->GetName(), JsonValue);
        }
    }

    if (OutParamsObj->Values.Num() > 0)
    {
        ResultObj->SetObjectField(TEXT("out_params"), OutParamsObj);
    }

    return ResultObj;
}
