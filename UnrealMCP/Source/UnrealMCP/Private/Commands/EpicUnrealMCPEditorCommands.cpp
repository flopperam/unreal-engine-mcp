#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
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

FEpicUnrealMCPEditorCommands::FEpicUnrealMCPEditorCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Actor manipulation commands
    if (CommandType == TEXT("get_actors_in_level"))
    {
        return HandleGetActorsInLevel(Params);
    }
    else if (CommandType == TEXT("find_actors_by_name"))
    {
        return HandleFindActorsByName(Params);
    }
    else if (CommandType == TEXT("spawn_actor"))
    {
        return HandleSpawnActor(Params);
    }
    else if (CommandType == TEXT("delete_actor"))
    {
        return HandleDeleteActor(Params);
    }
    else if (CommandType == TEXT("set_actor_transform"))
    {
        return HandleSetActorTransform(Params);
    }
    // Blueprint actor spawning
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    else if (CommandType == TEXT("request_editor_exit"))
    {
        return HandleRequestEditorExit(Params);
    }
    else if (CommandType == TEXT("restart_editor"))
    {
        return HandleRestartEditor(Params);
    }
    else if (CommandType == TEXT("get_object_properties"))
    {
        return HandleGetObjectProperties(Params);
    }
    else if (CommandType == TEXT("set_object_properties"))
    {
        return HandleSetObjectProperties(Params);
    }
    else if (CommandType == TEXT("call_uobject_function"))
    {
        return HandleCallUObjectFunction(Params);
    }
    else if (CommandType == TEXT("build_navmesh"))
    {
        return HandleBuildNavMesh(Params);
    }
    
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleBuildNavMesh(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Use reflection to call NavigationSystemV1::Build() to avoid hard dependency on NavigationSystem module in Build.cs if not already there
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
    // Get required parameters
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    // Get actor name (required parameter)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Create the actor based on type
    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists
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
            // Check for an optional static_mesh parameter to assign a mesh
            FString MeshPath;
            if (Params->TryGetStringField(TEXT("static_mesh"), MeshPath))
            {
                UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
                if (Mesh)
                {
                    NewMeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Could not find static mesh at path: %s"), *MeshPath);
                }
            }
        }
        NewActor = NewMeshActor;
    }
    else if (ActorType == TEXT("PointLight"))
    {
        NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("SpotLight"))
    {
        NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("DirectionalLight"))
    {
        NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("CameraActor"))
    {
        NewActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType));
    }

    if (NewActor)
    {
        // Set scale (since SpawnActor only takes location and rotation)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);

        // Return the created actor's details
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
            // Store actor info before deletion for the response
            TSharedPtr<FJsonObject> ActorInfo = FEpicUnrealMCPCommonUtils::ActorToJsonObject(Actor);
            
            // Delete the actor
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
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get transform parameters
    FTransform NewTransform = TargetActor->GetTransform();

    if (Params->HasField(TEXT("location")))
    {
        NewTransform.SetLocation(FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        NewTransform.SetRotation(FQuat(FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
    }
    if (Params->HasField(TEXT("scale")))
    {
        NewTransform.SetScale3D(FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
    }

    // Set the new transform
    TargetActor->SetActorTransform(NewTransform);

    // Return updated actor info
    return FEpicUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    // This function will now correctly call the implementation in BlueprintCommands
    FEpicUnrealMCPBlueprintCommands BlueprintCommands;
    return BlueprintCommands.HandleCommand(TEXT("spawn_blueprint_actor"), Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleRequestEditorExit(const TSharedPtr<FJsonObject>& Params)
{
    bool bForce = false;
    if (Params.IsValid())
    {
        Params->TryGetBoolField(TEXT("force"), bForce);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("accepted"), true);
    ResultObj->SetBoolField(TEXT("force"), bForce);
    ResultObj->SetStringField(TEXT("message"), TEXT("Editor exit requested"));

    // Graceful termination unless force=true is explicitly requested.
    FPlatformMisc::RequestExit(bForce);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleRestartEditor(const TSharedPtr<FJsonObject>& Params)
{
    FString AdditionalArgs;
    if (Params.IsValid())
    {
        Params->TryGetStringField(TEXT("additional_args"), AdditionalArgs);
    }

    const FString ExecutablePath = FPlatformProcess::ExecutablePath();
    const FString ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());

    if (ExecutablePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to determine Unreal Editor executable path"));
    }

    if (ProjectPath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to determine current project path"));
    }

    FString LaunchArgs = FString::Printf(TEXT("\"%s\""), *ProjectPath);
    if (!AdditionalArgs.IsEmpty())
    {
        LaunchArgs += TEXT(" ");
        LaunchArgs += AdditionalArgs;
    }

    FProcHandle NewEditorProcess = FPlatformProcess::CreateProc(
        *ExecutablePath,
        *LaunchArgs,
        true,
        false,
        false,
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (!NewEditorProcess.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to launch replacement Unreal Editor process"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("accepted"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("Editor restart requested"));
    ResultObj->SetStringField(TEXT("executable_path"), ExecutablePath);
    ResultObj->SetStringField(TEXT("project_path"), ProjectPath);
    ResultObj->SetStringField(TEXT("launch_args"), LaunchArgs);

    // Ask current editor instance to exit after spawning replacement.
    FPlatformMisc::RequestExit(false);
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
            if (!NameValue.IsValid() || NameValue->Type != EJson::String)
            {
                continue;
            }

            const FString PropertyName = NameValue->AsString();
            FProperty* Property = TargetObject->GetClass()->FindPropertyByName(*PropertyName);
            if (!Property)
            {
                PropertiesObj->SetField(PropertyName, MakeShared<FJsonValueNull>());
                continue;
            }

            void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);
            PropertiesObj->SetField(PropertyName, PropertyValueToJson(Property, PropertyValuePtr));
        }
    }
    else
    {
        for (TFieldIterator<FProperty> It(TargetObject->GetClass()); It; ++It)
        {
            FProperty* Property = *It;
            if (!Property)
            {
                continue;
            }

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
    if (!TargetObject)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(ResolveError);
    }

    const TSharedPtr<FJsonObject>* PropertiesToSetPtr = nullptr;
    if (!Params.IsValid() ||
        !Params->TryGetObjectField(TEXT("properties"), PropertiesToSetPtr) ||
        !PropertiesToSetPtr ||
        !PropertiesToSetPtr->IsValid())
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
        if (!Property)
        {
            FailedProperties->SetStringField(PropertyName, FString::Printf(TEXT("Property not found: %s"), *PropertyName));
            continue;
        }

        void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);
        FString SetError;
        if (!SetPropertyFromJsonValue(Property, PropertyValuePtr, Pair.Value, SetError))
        {
            FailedProperties->SetStringField(PropertyName, SetError);
            continue;
        }

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
    if (!TargetObject)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(ResolveError);
    }

    FString FunctionName;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    UFunction* Function = TargetObject->FindFunction(FName(*FunctionName));
    if (!Function)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Function '%s' not found on object '%s'"), *FunctionName, *TargetObject->GetName())
        );
    }

    TSharedPtr<FJsonObject> ArgumentsObj = MakeShared<FJsonObject>();
    const TSharedPtr<FJsonObject>* ArgumentsObjPtr = nullptr;
    if (Params->TryGetObjectField(TEXT("arguments"), ArgumentsObjPtr) &&
        ArgumentsObjPtr &&
        ArgumentsObjPtr->IsValid())
    {
        ArgumentsObj = *ArgumentsObjPtr;
    }

    uint8* ParamsBuffer = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
    FMemory::Memzero(ParamsBuffer, Function->ParmsSize);

    for (TFieldIterator<FProperty> It(Function); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_ReturnParm))
        {
            continue;
        }

        const FString ParamName = Property->GetName();
        if (!ArgumentsObj->HasField(ParamName))
        {
            continue;
        }

        TSharedPtr<FJsonValue> ParamValue = ArgumentsObj->TryGetField(ParamName);
        if (!ParamValue.IsValid())
        {
            continue;
        }

        void* ParamValuePtr = Property->ContainerPtrToValuePtr<void>(ParamsBuffer);
        FString SetError;
        if (!SetPropertyFromJsonValue(Property, ParamValuePtr, ParamValue, SetError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Failed to set parameter '%s': %s"), *ParamName, *SetError)
            );
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
        if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm))
        {
            continue;
        }

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
