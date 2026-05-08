#include "Commands/EpicUnrealMCPEnhancedInputCommands.h"

#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputDeveloperSettings.h"
#include "EnhancedInputLibrary.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "K2Node_CallFunction.h"
#include "K2Node_EnhancedInputAction.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PlayerMappableKeySettings.h"
#include "UObject/Package.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace
{
constexpr const TCHAR* DefaultInputSettingsSection = TEXT("/Script/Engine.InputSettings");

TSharedPtr<FJsonObject> CreateSuccess(const TSharedPtr<FJsonObject>& Data = nullptr)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    if (Data.IsValid())
    {
        Response->SetObjectField(TEXT("data"), Data);
    }
    return Response;
}

FString NormalizePackagePath(const FString& InAssetPath)
{
    FString Path = InAssetPath;
    Path.TrimStartAndEndInline();
    if (Path.Contains(TEXT(".")))
    {
        Path = FPackageName::ObjectPathToPackageName(Path);
    }
    int32 DotIndex = INDEX_NONE;
    if (Path.FindChar(TEXT('.'), DotIndex))
    {
        Path.LeftInline(DotIndex);
    }
    return Path;
}

FString ToObjectPath(const FString& InAssetPath)
{
    const FString PackagePath = NormalizePackagePath(InAssetPath);
    if (PackagePath.IsEmpty())
    {
        return FString();
    }
    return FString::Printf(TEXT("%s.%s"), *PackagePath, *FPackageName::GetShortName(PackagePath));
}

bool ValidateLongPackagePath(const FString& AssetPath, FString& OutPackagePath, FString& OutError)
{
    OutPackagePath = NormalizePackagePath(AssetPath);
    if (OutPackagePath.IsEmpty())
    {
        OutError = TEXT("Asset path is empty");
        return false;
    }
    if (!OutPackagePath.StartsWith(TEXT("/Game/")))
    {
        OutError = FString::Printf(TEXT("Asset path must be a /Game long package path, got '%s'"), *AssetPath);
        return false;
    }
    FText InvalidPackageReason;
    if (!FPackageName::IsValidLongPackageName(OutPackagePath, false, &InvalidPackageReason))
    {
        OutError = InvalidPackageReason.ToString();
        return false;
    }
    return true;
}

template <typename T>
T* LoadEnhancedInputAsset(const FString& AssetPath, FString& OutError)
{
    if (AssetPath.IsEmpty())
    {
        OutError = TEXT("Missing asset path");
        return nullptr;
    }

    const FString ObjectPath = ToObjectPath(AssetPath);
    T* Asset = LoadObject<T>(nullptr, *ObjectPath);
    if (!Asset)
    {
        OutError = FString::Printf(TEXT("Failed to load %s at '%s'. Use a valid object path like /Game/Input/IA_Jump or /Game/Input/IA_Jump.IA_Jump."),
            *T::StaticClass()->GetName(),
            *AssetPath);
    }
    return Asset;
}

bool SaveAsset(UObject* Asset, FString& OutError)
{
    if (!Asset)
    {
        OutError = TEXT("Cannot save null asset");
        return false;
    }

    if (UPackage* Package = Asset->GetOutermost())
    {
        Package->SetDirtyFlag(true);
        Package->MarkPackageDirty();
    }

    if (!UEditorAssetLibrary::SaveLoadedAsset(Asset, false))
    {
        OutError = FString::Printf(TEXT("Failed to save asset '%s'. Check source control, file permissions, or package validity."), *Asset->GetPathName());
        return false;
    }
    return true;
}

bool TryGetKey(const FString& KeyName, FKey& OutKey, FString& OutError)
{
    FString Normalized = KeyName;
    Normalized.TrimStartAndEndInline();
    if (Normalized.IsEmpty())
    {
        OutError = TEXT("Missing key name");
        return false;
    }

    static const TMap<FString, FName> KeyAliases = {
        {TEXT("space"), EKeys::SpaceBar.GetFName()},
        {TEXT("spacebar"), EKeys::SpaceBar.GetFName()},
        {TEXT("left_mouse"), EKeys::LeftMouseButton.GetFName()},
        {TEXT("right_mouse"), EKeys::RightMouseButton.GetFName()},
        {TEXT("middle_mouse"), EKeys::MiddleMouseButton.GetFName()},
        {TEXT("mouse_x"), EKeys::MouseX.GetFName()},
        {TEXT("mouse_y"), EKeys::MouseY.GetFName()},
        {TEXT("gamepad_a"), EKeys::Gamepad_FaceButton_Bottom.GetFName()},
        {TEXT("gamepad_b"), EKeys::Gamepad_FaceButton_Right.GetFName()},
        {TEXT("gamepad_x"), EKeys::Gamepad_FaceButton_Left.GetFName()},
        {TEXT("gamepad_y"), EKeys::Gamepad_FaceButton_Top.GetFName()},
    };

    if (const FName* Alias = KeyAliases.Find(Normalized.ToLower()))
    {
        OutKey = FKey(*Alias);
    }
    else
    {
        OutKey = FKey(FName(*Normalized));
    }

    TArray<FKey> AllKeys;
    EKeys::GetAllKeys(AllKeys);
    if (!AllKeys.Contains(OutKey))
    {
        OutError = FString::Printf(TEXT("Unknown key '%s'. Use Unreal FKey names such as W, SpaceBar, LeftMouseButton, MouseX, Gamepad_LeftX, or Gamepad_FaceButton_Bottom."), *KeyName);
        return false;
    }
    return true;
}

FString KeyDeviceType(FKey Key)
{
    if (Key.IsGamepadKey())
    {
        return TEXT("gamepad");
    }
    if (Key.IsMouseButton() || Key == EKeys::MouseX || Key == EKeys::MouseY || Key == EKeys::Mouse2D || Key == EKeys::MouseWheelAxis)
    {
        return TEXT("mouse");
    }
    if (Key.IsTouch())
    {
        return TEXT("touch");
    }
    if (Key.IsGesture())
    {
        return TEXT("gesture");
    }
    return TEXT("keyboard");
}

TSharedPtr<FJsonObject> KeyToJson(FKey Key)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("name"), Key.GetFName().ToString());
    Object->SetStringField(TEXT("device_type"), KeyDeviceType(Key));
    Object->SetBoolField(TEXT("is_gamepad"), Key.IsGamepadKey());
    Object->SetBoolField(TEXT("is_mouse_button"), Key.IsMouseButton());
    Object->SetBoolField(TEXT("is_touch"), Key.IsTouch());
    Object->SetBoolField(TEXT("is_axis_1d"), Key.IsAxis1D());
    Object->SetBoolField(TEXT("is_axis_2d"), Key.IsAxis2D());
    Object->SetBoolField(TEXT("is_axis_3d"), Key.IsAxis3D());
    Object->SetBoolField(TEXT("is_analog"), Key.IsAnalog());
    if (TSharedPtr<FKeyDetails> Details = EKeys::GetKeyDetails(Key))
    {
        Object->SetStringField(TEXT("display_name"), Details->GetDisplayName(true).ToString());
        Object->SetStringField(TEXT("short_display_name"), Details->GetDisplayName(false).ToString());
        Object->SetStringField(TEXT("menu_category"), Details->GetMenuCategory().ToString());
        Object->SetBoolField(TEXT("is_bindable_to_actions"), Details->IsBindableToActions());
        Object->SetBoolField(TEXT("is_bindable_in_blueprints"), Details->IsBindableInBlueprints());
    }
    return Object;
}

EInputActionValueType ParseValueType(const FString& ValueType)
{
    const FString Lower = ValueType.ToLower();
    if (Lower == TEXT("axis1d") || Lower == TEXT("1d") || Lower == TEXT("float"))
    {
        return EInputActionValueType::Axis1D;
    }
    if (Lower == TEXT("axis2d") || Lower == TEXT("2d") || Lower == TEXT("vector2d"))
    {
        return EInputActionValueType::Axis2D;
    }
    if (Lower == TEXT("axis3d") || Lower == TEXT("3d") || Lower == TEXT("vector"))
    {
        return EInputActionValueType::Axis3D;
    }
    return EInputActionValueType::Boolean;
}

FString ValueTypeToString(EInputActionValueType ValueType)
{
    switch (ValueType)
    {
    case EInputActionValueType::Boolean:
        return TEXT("Boolean");
    case EInputActionValueType::Axis1D:
        return TEXT("Axis1D");
    case EInputActionValueType::Axis2D:
        return TEXT("Axis2D");
    case EInputActionValueType::Axis3D:
        return TEXT("Axis3D");
    default:
        return TEXT("Unknown");
    }
}

EInputActionAccumulationBehavior ParseAccumulationBehavior(const FString& Value)
{
    const FString Lower = Value.ToLower();
    if (Lower == TEXT("cumulative") || Lower == TEXT("sum"))
    {
        return EInputActionAccumulationBehavior::Cumulative;
    }
    return EInputActionAccumulationBehavior::TakeHighestAbsoluteValue;
}

ETriggerEvent ParseTriggerEvent(const FString& EventName)
{
    const FString Lower = EventName.ToLower();
    if (Lower == TEXT("started"))
    {
        return ETriggerEvent::Started;
    }
    if (Lower == TEXT("ongoing"))
    {
        return ETriggerEvent::Ongoing;
    }
    if (Lower == TEXT("canceled") || Lower == TEXT("cancelled"))
    {
        return ETriggerEvent::Canceled;
    }
    if (Lower == TEXT("completed"))
    {
        return ETriggerEvent::Completed;
    }
    return ETriggerEvent::Triggered;
}

FString TriggerEventToPinName(ETriggerEvent Event)
{
    switch (Event)
    {
    case ETriggerEvent::Started:
        return TEXT("Started");
    case ETriggerEvent::Ongoing:
        return TEXT("Ongoing");
    case ETriggerEvent::Canceled:
        return TEXT("Canceled");
    case ETriggerEvent::Completed:
        return TEXT("Completed");
    case ETriggerEvent::Triggered:
    default:
        return TEXT("Triggered");
    }
}

FVector ReadVector(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, const FVector& DefaultValue)
{
    const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
    if (Object->TryGetArrayField(FieldName, Array) && Array && Array->Num() >= 3)
    {
        return FVector((*Array)[0]->AsNumber(), (*Array)[1]->AsNumber(), (*Array)[2]->AsNumber());
    }
    return DefaultValue;
}

EDeadZoneType ParseDeadZoneType(const FString& Type)
{
    const FString Lower = Type.ToLower();
    if (Lower == TEXT("axial"))
    {
        return EDeadZoneType::Axial;
    }
    if (Lower == TEXT("unscaled_radial") || Lower == TEXT("unscaledradial"))
    {
        return EDeadZoneType::UnscaledRadial;
    }
    return EDeadZoneType::Radial;
}

EInputAxisSwizzle ParseSwizzle(const FString& Order)
{
    const FString Upper = Order.ToUpper();
    if (Upper == TEXT("ZYX"))
    {
        return EInputAxisSwizzle::ZYX;
    }
    if (Upper == TEXT("XZY"))
    {
        return EInputAxisSwizzle::XZY;
    }
    if (Upper == TEXT("YZX"))
    {
        return EInputAxisSwizzle::YZX;
    }
    if (Upper == TEXT("ZXY"))
    {
        return EInputAxisSwizzle::ZXY;
    }
    return EInputAxisSwizzle::YXZ;
}

template <typename TEnum>
void SetEnumStructProperty(UScriptStruct* Struct, void* Container, const FName PropertyName, TEnum Value)
{
    FProperty* Property = Struct ? Struct->FindPropertyByName(PropertyName) : nullptr;
    if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
    {
        void* ValuePtr = EnumProperty->ContainerPtrToValuePtr<void>(Container);
        EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, static_cast<int64>(Value));
    }
    else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
    {
        void* ValuePtr = ByteProperty->ContainerPtrToValuePtr<void>(Container);
        ByteProperty->SetPropertyValue(ValuePtr, static_cast<uint8>(Value));
    }
}

void SetObjectStructProperty(UScriptStruct* Struct, void* Container, const FName PropertyName, UObject* Value)
{
    FProperty* Property = Struct ? Struct->FindPropertyByName(PropertyName) : nullptr;
    if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
    {
        void* ValuePtr = ObjectProperty->ContainerPtrToValuePtr<void>(Container);
        ObjectProperty->SetObjectPropertyValue(ValuePtr, Value);
    }
}

void SetObjectProperty(UObject* Object, const FName PropertyName, UObject* Value)
{
    FProperty* Property = Object ? Object->GetClass()->FindPropertyByName(PropertyName) : nullptr;
    if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
    {
        ObjectProperty->SetObjectPropertyValue_InContainer(Object, Value);
    }
}

UInputTrigger* CreateTriggerFromSpec(UObject* Outer, const TSharedPtr<FJsonObject>& Spec, FString& OutError)
{
    if (!Spec.IsValid())
    {
        OutError = TEXT("Trigger spec must be an object");
        return nullptr;
    }

    FString Type;
    Spec->TryGetStringField(TEXT("type"), Type);
    if (Type.IsEmpty())
    {
        Spec->TryGetStringField(TEXT("trigger_type"), Type);
    }
    const FString Lower = Type.ToLower();

    UInputTrigger* Trigger = nullptr;
    if (Lower == TEXT("down") || Lower.IsEmpty())
    {
        Trigger = NewObject<UInputTriggerDown>(Outer, NAME_None, RF_Transactional);
    }
    else if (Lower == TEXT("pressed") || Lower == TEXT("press"))
    {
        Trigger = NewObject<UInputTriggerPressed>(Outer, NAME_None, RF_Transactional);
    }
    else if (Lower == TEXT("released") || Lower == TEXT("release"))
    {
        Trigger = NewObject<UInputTriggerReleased>(Outer, NAME_None, RF_Transactional);
    }
    else if (Lower == TEXT("hold"))
    {
        UInputTriggerHold* Hold = NewObject<UInputTriggerHold>(Outer, NAME_None, RF_Transactional);
        double HoldTime = 0.0;
        if (Spec->TryGetNumberField(TEXT("hold_time_threshold"), HoldTime) || Spec->TryGetNumberField(TEXT("hold_time"), HoldTime))
        {
            Hold->HoldTimeThreshold = static_cast<float>(HoldTime);
        }
        bool bOneShot = false;
        if (Spec->TryGetBoolField(TEXT("is_one_shot"), bOneShot) || Spec->TryGetBoolField(TEXT("one_shot"), bOneShot))
        {
            Hold->bIsOneShot = bOneShot;
        }
        Trigger = Hold;
    }
    else if (Lower == TEXT("hold_and_release") || Lower == TEXT("holdrelease"))
    {
        UInputTriggerHoldAndRelease* HoldRelease = NewObject<UInputTriggerHoldAndRelease>(Outer, NAME_None, RF_Transactional);
        double HoldTime = 0.0;
        if (Spec->TryGetNumberField(TEXT("hold_time_threshold"), HoldTime) || Spec->TryGetNumberField(TEXT("hold_time"), HoldTime))
        {
            HoldRelease->HoldTimeThreshold = static_cast<float>(HoldTime);
        }
        Trigger = HoldRelease;
    }
    else if (Lower == TEXT("tap"))
    {
        UInputTriggerTap* Tap = NewObject<UInputTriggerTap>(Outer, NAME_None, RF_Transactional);
        double TapTime = 0.0;
        if (Spec->TryGetNumberField(TEXT("tap_release_time_threshold"), TapTime) || Spec->TryGetNumberField(TEXT("tap_time"), TapTime))
        {
            Tap->TapReleaseTimeThreshold = static_cast<float>(TapTime);
        }
        Trigger = Tap;
    }
    else if (Lower == TEXT("pulse"))
    {
        UInputTriggerPulse* Pulse = NewObject<UInputTriggerPulse>(Outer, NAME_None, RF_Transactional);
        bool bTriggerOnStart = true;
        if (Spec->TryGetBoolField(TEXT("trigger_on_start"), bTriggerOnStart))
        {
            Pulse->bTriggerOnStart = bTriggerOnStart;
        }
        double Interval = 0.0;
        if (Spec->TryGetNumberField(TEXT("interval"), Interval))
        {
            Pulse->Interval = static_cast<float>(Interval);
        }
        double TriggerLimit = 0.0;
        if (Spec->TryGetNumberField(TEXT("trigger_limit"), TriggerLimit))
        {
            Pulse->TriggerLimit = static_cast<int32>(TriggerLimit);
        }
        Trigger = Pulse;
    }
    else if (Lower == TEXT("chord") || Lower == TEXT("chord_action"))
    {
        UInputTriggerChordAction* Chord = NewObject<UInputTriggerChordAction>(Outer, NAME_None, RF_Transactional);
        FString ChordActionPath;
        if (Spec->TryGetStringField(TEXT("chord_action_path"), ChordActionPath) || Spec->TryGetStringField(TEXT("action_path"), ChordActionPath))
        {
            FString Error;
            Chord->ChordAction = LoadEnhancedInputAsset<UInputAction>(ChordActionPath, Error);
            if (!Chord->ChordAction)
            {
                OutError = Error;
                return nullptr;
            }
        }
        Trigger = Chord;
    }
    else
    {
        OutError = FString::Printf(TEXT("Unsupported trigger type '%s'. Supported: down, pressed, released, hold, hold_and_release, tap, pulse, chord."), *Type);
        return nullptr;
    }

    double ActuationThreshold = 0.0;
    if (Spec->TryGetNumberField(TEXT("actuation_threshold"), ActuationThreshold))
    {
        Trigger->ActuationThreshold = static_cast<float>(ActuationThreshold);
    }
    bool bAlwaysTick = false;
    if (Spec->TryGetBoolField(TEXT("should_always_tick"), bAlwaysTick) || Spec->TryGetBoolField(TEXT("always_tick"), bAlwaysTick))
    {
        Trigger->bShouldAlwaysTick = bAlwaysTick;
    }
    if (UInputTriggerTimedBase* Timed = Cast<UInputTriggerTimedBase>(Trigger))
    {
        bool bAffectedByTimeDilation = false;
        if (Spec->TryGetBoolField(TEXT("affected_by_time_dilation"), bAffectedByTimeDilation))
        {
            Timed->bAffectedByTimeDilation = bAffectedByTimeDilation;
        }
    }
    return Trigger;
}

UInputModifier* CreateModifierFromSpec(UObject* Outer, const TSharedPtr<FJsonObject>& Spec, FString& OutError)
{
    if (!Spec.IsValid())
    {
        OutError = TEXT("Modifier spec must be an object");
        return nullptr;
    }

    FString Type;
    Spec->TryGetStringField(TEXT("type"), Type);
    if (Type.IsEmpty())
    {
        Spec->TryGetStringField(TEXT("modifier_type"), Type);
    }
    const FString Lower = Type.ToLower();

    if (Lower == TEXT("dead_zone") || Lower == TEXT("deadzone"))
    {
        UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(Outer, NAME_None, RF_Transactional);
        double LowerThreshold = 0.0;
        if (Spec->TryGetNumberField(TEXT("lower_threshold"), LowerThreshold))
        {
            DeadZone->LowerThreshold = static_cast<float>(LowerThreshold);
        }
        double UpperThreshold = 0.0;
        if (Spec->TryGetNumberField(TEXT("upper_threshold"), UpperThreshold))
        {
            DeadZone->UpperThreshold = static_cast<float>(UpperThreshold);
        }
        FString DeadZoneType;
        if (Spec->TryGetStringField(TEXT("dead_zone_type"), DeadZoneType) || Spec->TryGetStringField(TEXT("zone_type"), DeadZoneType))
        {
            DeadZone->Type = ParseDeadZoneType(DeadZoneType);
        }
        return DeadZone;
    }
    if (Lower == TEXT("swizzle_axis") || Lower == TEXT("swizzle"))
    {
        UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Outer, NAME_None, RF_Transactional);
        FString Order;
        if (Spec->TryGetStringField(TEXT("order"), Order))
        {
            Swizzle->Order = ParseSwizzle(Order);
        }
        return Swizzle;
    }
    if (Lower == TEXT("negate"))
    {
        UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(Outer, NAME_None, RF_Transactional);
        bool bValue = true;
        if (Spec->TryGetBoolField(TEXT("x"), bValue) || Spec->TryGetBoolField(TEXT("negate_x"), bValue))
        {
            Negate->bX = bValue;
        }
        if (Spec->TryGetBoolField(TEXT("y"), bValue) || Spec->TryGetBoolField(TEXT("negate_y"), bValue))
        {
            Negate->bY = bValue;
        }
        if (Spec->TryGetBoolField(TEXT("z"), bValue) || Spec->TryGetBoolField(TEXT("negate_z"), bValue))
        {
            Negate->bZ = bValue;
        }
        return Negate;
    }
    if (Lower == TEXT("scalar") || Lower == TEXT("scale"))
    {
        UInputModifierScalar* Scalar = NewObject<UInputModifierScalar>(Outer, NAME_None, RF_Transactional);
        Scalar->Scalar = ReadVector(Spec, TEXT("scalar"), FVector::OneVector);
        double X = 0.0;
        if (Spec->TryGetNumberField(TEXT("x"), X))
        {
            Scalar->Scalar.X = X;
        }
        double Y = 0.0;
        if (Spec->TryGetNumberField(TEXT("y"), Y))
        {
            Scalar->Scalar.Y = Y;
        }
        double Z = 0.0;
        if (Spec->TryGetNumberField(TEXT("z"), Z))
        {
            Scalar->Scalar.Z = Z;
        }
        return Scalar;
    }
    if (Lower == TEXT("smooth"))
    {
        return NewObject<UInputModifierSmooth>(Outer, NAME_None, RF_Transactional);
    }
    if (Lower == TEXT("smooth_delta"))
    {
        UInputModifierSmoothDelta* SmoothDelta = NewObject<UInputModifierSmoothDelta>(Outer, NAME_None, RF_Transactional);
        double Speed = 0.0;
        if (Spec->TryGetNumberField(TEXT("speed"), Speed))
        {
            SmoothDelta->Speed = static_cast<float>(Speed);
        }
        double EasingExponent = 0.0;
        if (Spec->TryGetNumberField(TEXT("easing_exponent"), EasingExponent))
        {
            SmoothDelta->EasingExponent = static_cast<float>(EasingExponent);
        }
        return SmoothDelta;
    }
    if (Lower == TEXT("scale_by_delta_time") || Lower == TEXT("delta_time"))
    {
        return NewObject<UInputModifierScaleByDeltaTime>(Outer, NAME_None, RF_Transactional);
    }
    if (Lower == TEXT("response_curve_exponential") || Lower == TEXT("response_curve"))
    {
        UInputModifierResponseCurveExponential* Curve = NewObject<UInputModifierResponseCurveExponential>(Outer, NAME_None, RF_Transactional);
        Curve->CurveExponent = ReadVector(Spec, TEXT("curve_exponent"), FVector::OneVector);
        return Curve;
    }
    if (Lower == TEXT("fov_scaling") || Lower == TEXT("fov"))
    {
        UInputModifierFOVScaling* FOV = NewObject<UInputModifierFOVScaling>(Outer, NAME_None, RF_Transactional);
        double FOVScale = 0.0;
        if (Spec->TryGetNumberField(TEXT("fov_scale"), FOVScale))
        {
            FOV->FOVScale = static_cast<float>(FOVScale);
        }
        return FOV;
    }
    if (Lower == TEXT("to_world_space") || Lower == TEXT("world_space"))
    {
        return NewObject<UInputModifierToWorldSpace>(Outer, NAME_None, RF_Transactional);
    }

    OutError = FString::Printf(TEXT("Unsupported modifier type '%s'. Supported: dead_zone, swizzle_axis, negate, scalar, smooth, smooth_delta, scale_by_delta_time, response_curve_exponential, fov_scaling, to_world_space."), *Type);
    return nullptr;
}

bool ApplyTriggerSpecs(UObject* Outer, const TArray<TSharedPtr<FJsonValue>>& Specs, TArray<TObjectPtr<UInputTrigger>>& Target, FString& OutError)
{
    Target.Reset();
    for (const TSharedPtr<FJsonValue>& Value : Specs)
    {
        const TSharedPtr<FJsonObject> Spec = Value.IsValid() ? Value->AsObject() : nullptr;
        UInputTrigger* Trigger = CreateTriggerFromSpec(Outer, Spec, OutError);
        if (!Trigger)
        {
            return false;
        }
        Target.Add(Trigger);
    }
    return true;
}

bool ApplyModifierSpecs(UObject* Outer, const TArray<TSharedPtr<FJsonValue>>& Specs, TArray<TObjectPtr<UInputModifier>>& Target, FString& OutError)
{
    Target.Reset();
    for (const TSharedPtr<FJsonValue>& Value : Specs)
    {
        const TSharedPtr<FJsonObject> Spec = Value.IsValid() ? Value->AsObject() : nullptr;
        UInputModifier* Modifier = CreateModifierFromSpec(Outer, Spec, OutError);
        if (!Modifier)
        {
            return false;
        }
        Target.Add(Modifier);
    }
    return true;
}

void ApplyPlayerMappableSettings(FEnhancedActionKeyMapping& Mapping, UObject* Outer, const TSharedPtr<FJsonObject>& Params)
{
    bool bPlayerMappable = false;
    if (!Params->TryGetBoolField(TEXT("player_mappable"), bPlayerMappable) && !Params->HasField(TEXT("player_mappable_settings")))
    {
        return;
    }

    if (!bPlayerMappable)
    {
        SetEnumStructProperty(FEnhancedActionKeyMapping::StaticStruct(), &Mapping, TEXT("SettingBehavior"), EPlayerMappableKeySettingBehaviors::IgnoreSettings);
        SetObjectStructProperty(FEnhancedActionKeyMapping::StaticStruct(), &Mapping, TEXT("PlayerMappableKeySettings"), nullptr);
        return;
    }

    FString MappingName;
    FString DisplayName;
    FString DisplayCategory;
    const TSharedPtr<FJsonObject>* SettingsObject = nullptr;
    if (Params->TryGetObjectField(TEXT("player_mappable_settings"), SettingsObject) && SettingsObject && SettingsObject->IsValid())
    {
        (*SettingsObject)->TryGetStringField(TEXT("mapping_name"), MappingName);
        (*SettingsObject)->TryGetStringField(TEXT("display_name"), DisplayName);
        (*SettingsObject)->TryGetStringField(TEXT("display_category"), DisplayCategory);
    }
    Params->TryGetStringField(TEXT("mapping_name"), MappingName);
    Params->TryGetStringField(TEXT("display_name"), DisplayName);
    Params->TryGetStringField(TEXT("display_category"), DisplayCategory);

    if (MappingName.IsEmpty())
    {
        MappingName = Mapping.Action ? Mapping.Action->GetName() : Mapping.Key.GetFName().ToString();
    }
    if (DisplayName.IsEmpty())
    {
        DisplayName = MappingName;
    }

    UPlayerMappableKeySettings* Settings = NewObject<UPlayerMappableKeySettings>(Outer, NAME_None, RF_Transactional);
    Settings->Name = FName(*MappingName);
    Settings->DisplayName = FText::FromString(DisplayName);
    Settings->DisplayCategory = FText::FromString(DisplayCategory);

    SetEnumStructProperty(FEnhancedActionKeyMapping::StaticStruct(), &Mapping, TEXT("SettingBehavior"), EPlayerMappableKeySettingBehaviors::OverrideSettings);
    SetObjectStructProperty(FEnhancedActionKeyMapping::StaticStruct(), &Mapping, TEXT("PlayerMappableKeySettings"), Settings);

    // Runtime user settings rebuilds can fall back to action-level mappable settings.
    // Keep the action metadata in sync so rebind rows use the requested stable name.
    if (UInputAction* Action = const_cast<UInputAction*>(Mapping.Action.Get()))
    {
        Action->Modify();
        UPlayerMappableKeySettings* ActionSettings = Action->GetPlayerMappableKeySettings().Get();
        if (!ActionSettings)
        {
            ActionSettings = NewObject<UPlayerMappableKeySettings>(Action, NAME_None, RF_Transactional);
            SetObjectProperty(Action, TEXT("PlayerMappableKeySettings"), ActionSettings);
        }
        if (ActionSettings)
        {
            ActionSettings->Name = FName(*MappingName);
            ActionSettings->DisplayName = FText::FromString(DisplayName);
            ActionSettings->DisplayCategory = FText::FromString(DisplayCategory);
            Action->MarkPackageDirty();
        }
    }
}

TSharedPtr<FJsonObject> TriggerToJson(const UInputTrigger* Trigger)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    if (!Trigger)
    {
        Object->SetStringField(TEXT("type"), TEXT("null"));
        return Object;
    }
    Object->SetStringField(TEXT("class"), Trigger->GetClass()->GetName());
    Object->SetStringField(TEXT("type"), Trigger->GetClass()->GetName().Replace(TEXT("InputTrigger"), TEXT("")).ToLower());
    Object->SetNumberField(TEXT("actuation_threshold"), Trigger->ActuationThreshold);
    Object->SetBoolField(TEXT("should_always_tick"), Trigger->bShouldAlwaysTick);
    if (const UInputTriggerHold* Hold = Cast<UInputTriggerHold>(Trigger))
    {
        Object->SetNumberField(TEXT("hold_time_threshold"), Hold->HoldTimeThreshold);
        Object->SetBoolField(TEXT("is_one_shot"), Hold->bIsOneShot);
    }
    if (const UInputTriggerHoldAndRelease* HoldRelease = Cast<UInputTriggerHoldAndRelease>(Trigger))
    {
        Object->SetNumberField(TEXT("hold_time_threshold"), HoldRelease->HoldTimeThreshold);
    }
    if (const UInputTriggerTap* Tap = Cast<UInputTriggerTap>(Trigger))
    {
        Object->SetNumberField(TEXT("tap_release_time_threshold"), Tap->TapReleaseTimeThreshold);
    }
    if (const UInputTriggerChordAction* Chord = Cast<UInputTriggerChordAction>(Trigger))
    {
        Object->SetStringField(TEXT("chord_action_path"), Chord->ChordAction ? Chord->ChordAction->GetPathName() : FString());
    }
    return Object;
}

TSharedPtr<FJsonObject> ModifierToJson(const UInputModifier* Modifier)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    if (!Modifier)
    {
        Object->SetStringField(TEXT("type"), TEXT("null"));
        return Object;
    }
    Object->SetStringField(TEXT("class"), Modifier->GetClass()->GetName());
    Object->SetStringField(TEXT("type"), Modifier->GetClass()->GetName().Replace(TEXT("InputModifier"), TEXT("")).ToLower());
    if (const UInputModifierDeadZone* DeadZone = Cast<UInputModifierDeadZone>(Modifier))
    {
        Object->SetNumberField(TEXT("lower_threshold"), DeadZone->LowerThreshold);
        Object->SetNumberField(TEXT("upper_threshold"), DeadZone->UpperThreshold);
        Object->SetStringField(TEXT("dead_zone_type"), StaticEnum<EDeadZoneType>()->GetNameStringByValue(static_cast<int64>(DeadZone->Type)));
    }
    if (const UInputModifierSwizzleAxis* Swizzle = Cast<UInputModifierSwizzleAxis>(Modifier))
    {
        Object->SetStringField(TEXT("order"), StaticEnum<EInputAxisSwizzle>()->GetNameStringByValue(static_cast<int64>(Swizzle->Order)));
    }
    if (const UInputModifierNegate* Negate = Cast<UInputModifierNegate>(Modifier))
    {
        Object->SetBoolField(TEXT("x"), Negate->bX);
        Object->SetBoolField(TEXT("y"), Negate->bY);
        Object->SetBoolField(TEXT("z"), Negate->bZ);
    }
    if (const UInputModifierScalar* Scalar = Cast<UInputModifierScalar>(Modifier))
    {
        TArray<TSharedPtr<FJsonValue>> ScalarArray;
        ScalarArray.Add(MakeShared<FJsonValueNumber>(Scalar->Scalar.X));
        ScalarArray.Add(MakeShared<FJsonValueNumber>(Scalar->Scalar.Y));
        ScalarArray.Add(MakeShared<FJsonValueNumber>(Scalar->Scalar.Z));
        Object->SetArrayField(TEXT("scalar"), ScalarArray);
    }
    return Object;
}

TSharedPtr<FJsonObject> MappingToJson(const FEnhancedActionKeyMapping& Mapping, int32 Index)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("index"), Index);
    Object->SetStringField(TEXT("action_path"), Mapping.Action ? Mapping.Action->GetPathName() : FString());
    Object->SetStringField(TEXT("action_name"), Mapping.Action ? Mapping.Action->GetName() : FString());
    Object->SetObjectField(TEXT("key"), KeyToJson(Mapping.Key));
    Object->SetStringField(TEXT("mapping_name"), Mapping.GetMappingName().ToString());
    Object->SetStringField(TEXT("display_name"), Mapping.GetDisplayName().ToString());
    Object->SetStringField(TEXT("display_category"), Mapping.GetDisplayCategory().ToString());
    Object->SetBoolField(TEXT("player_mappable"), Mapping.IsPlayerMappable());

    TArray<TSharedPtr<FJsonValue>> Triggers;
    for (const UInputTrigger* Trigger : Mapping.Triggers)
    {
        Triggers.Add(MakeShared<FJsonValueObject>(TriggerToJson(Trigger)));
    }
    Object->SetArrayField(TEXT("triggers"), Triggers);

    TArray<TSharedPtr<FJsonValue>> Modifiers;
    for (const UInputModifier* Modifier : Mapping.Modifiers)
    {
        Modifiers.Add(MakeShared<FJsonValueObject>(ModifierToJson(Modifier)));
    }
    Object->SetArrayField(TEXT("modifiers"), Modifiers);
    return Object;
}

TSharedPtr<FJsonObject> InputActionToJson(const UInputAction* Action)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    if (!Action)
    {
        return Object;
    }
    Object->SetStringField(TEXT("asset_path"), Action->GetPathName());
    Object->SetStringField(TEXT("name"), Action->GetName());
    Object->SetStringField(TEXT("value_type"), ValueTypeToString(Action->ValueType));
    Object->SetStringField(TEXT("description"), Action->ActionDescription.ToString());
    Object->SetBoolField(TEXT("trigger_when_paused"), Action->bTriggerWhenPaused);
    Object->SetBoolField(TEXT("consume_input"), Action->bConsumeInput);
    Object->SetBoolField(TEXT("reserve_all_mappings"), Action->bReserveAllMappings);

    TArray<TSharedPtr<FJsonValue>> Triggers;
    for (const UInputTrigger* Trigger : Action->Triggers)
    {
        Triggers.Add(MakeShared<FJsonValueObject>(TriggerToJson(Trigger)));
    }
    Object->SetArrayField(TEXT("triggers"), Triggers);

    TArray<TSharedPtr<FJsonValue>> Modifiers;
    for (const UInputModifier* Modifier : Action->Modifiers)
    {
        Modifiers.Add(MakeShared<FJsonValueObject>(ModifierToJson(Modifier)));
    }
    Object->SetArrayField(TEXT("modifiers"), Modifiers);
    return Object;
}

FEnhancedActionKeyMapping* FindMapping(UInputMappingContext* Context, const TSharedPtr<FJsonObject>& Params, FString& OutError, int32* OutIndex = nullptr)
{
    if (!Context)
    {
        OutError = TEXT("Mapping context is null");
        return nullptr;
    }

    double IndexNumber = -1.0;
    if (Params->TryGetNumberField(TEXT("mapping_index"), IndexNumber))
    {
        const int32 Index = static_cast<int32>(IndexNumber);
        if (Index >= 0 && Index < Context->GetMappings().Num())
        {
            if (OutIndex)
            {
                *OutIndex = Index;
            }
            return &Context->GetMapping(Index);
        }
        OutError = FString::Printf(TEXT("mapping_index %d is out of range for context '%s'"), Index, *Context->GetPathName());
        return nullptr;
    }

    FString ActionPath;
    Params->TryGetStringField(TEXT("input_action_path"), ActionPath);
    if (ActionPath.IsEmpty())
    {
        Params->TryGetStringField(TEXT("action_path"), ActionPath);
    }
    FString KeyName;
    Params->TryGetStringField(TEXT("key"), KeyName);
    if (KeyName.IsEmpty())
    {
        Params->TryGetStringField(TEXT("key_name"), KeyName);
    }

    UInputAction* Action = nullptr;
    if (!ActionPath.IsEmpty())
    {
        FString Error;
        Action = LoadEnhancedInputAsset<UInputAction>(ActionPath, Error);
        if (!Action)
        {
            OutError = Error;
            return nullptr;
        }
    }

    FKey Key;
    const bool bHasKey = !KeyName.IsEmpty();
    if (bHasKey && !TryGetKey(KeyName, Key, OutError))
    {
        return nullptr;
    }

    for (int32 Index = 0; Index < Context->GetMappings().Num(); ++Index)
    {
        FEnhancedActionKeyMapping& Mapping = Context->GetMapping(Index);
        const bool bActionMatches = !Action || Mapping.Action == Action;
        const bool bKeyMatches = !bHasKey || Mapping.Key == Key;
        if (bActionMatches && bKeyMatches)
        {
            if (OutIndex)
            {
                *OutIndex = Index;
            }
            return &Mapping;
        }
    }

    OutError = FString::Printf(TEXT("No mapping found in '%s' for action '%s' and key '%s'"),
        *Context->GetPathName(),
        Action ? *Action->GetPathName() : TEXT("<any>"),
        bHasKey ? *Key.GetFName().ToString() : TEXT("<any>"));
    return nullptr;
}

UWorld* GetRuntimeWorld()
{
    if (GEditor && GEditor->PlayWorld)
    {
        return GEditor->PlayWorld;
    }
    if (GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if ((Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game) && Context.World())
            {
                return Context.World();
            }
        }
    }
    return nullptr;
}

UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem(int32 PlayerIndex, FString& OutError, ULocalPlayer** OutLocalPlayer = nullptr)
{
    UWorld* World = GetRuntimeWorld();
    if (!World)
    {
        OutError = TEXT("No PIE/game world is active. Start PIE before using runtime Enhanced Input subsystem commands.");
        return nullptr;
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(World, PlayerIndex);
    if (!PC)
    {
        OutError = FString::Printf(TEXT("No PlayerController found for local player index %d. For local multiplayer, create the local player first."), PlayerIndex);
        return nullptr;
    }

    ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
    if (!LocalPlayer)
    {
        OutError = FString::Printf(TEXT("PlayerController '%s' has no LocalPlayer; Enhanced Input local subsystem is unavailable."), *PC->GetName());
        return nullptr;
    }

    if (OutLocalPlayer)
    {
        *OutLocalPlayer = LocalPlayer;
    }

    UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
    if (!Subsystem)
    {
        OutError = TEXT("Enhanced Input Local Player Subsystem is not available. Ensure the EnhancedInput plugin is enabled and DefaultPlayerInputClass/DefaultInputComponentClass use Enhanced Input.");
        return nullptr;
    }
    return Subsystem;
}

FModifyContextOptions ReadModifyContextOptions(const TSharedPtr<FJsonObject>& Params)
{
    FModifyContextOptions Options;
    bool bValue = false;
    if (Params->TryGetBoolField(TEXT("ignore_all_pressed_keys_until_release"), bValue))
    {
        Options.bIgnoreAllPressedKeysUntilRelease = bValue;
    }
    if (Params->TryGetBoolField(TEXT("force_immediately"), bValue))
    {
        Options.bForceImmediately = bValue;
    }
    if (Params->TryGetBoolField(TEXT("notify_user_settings"), bValue))
    {
        Options.bNotifyUserSettings = bValue;
    }
    return Options;
}

EPlayerMappableKeySlot ParseKeySlot(const FString& SlotName)
{
    const FString Lower = SlotName.ToLower();
    if (Lower == TEXT("second") || Lower == TEXT("slot2") || Lower == TEXT("2"))
    {
        return EPlayerMappableKeySlot::Second;
    }
    if (Lower == TEXT("third") || Lower == TEXT("slot3") || Lower == TEXT("3"))
    {
        return EPlayerMappableKeySlot::Third;
    }
    if (Lower == TEXT("fourth") || Lower == TEXT("slot4") || Lower == TEXT("4"))
    {
        return EPlayerMappableKeySlot::Fourth;
    }
    if (Lower == TEXT("fifth") || Lower == TEXT("slot5") || Lower == TEXT("5"))
    {
        return EPlayerMappableKeySlot::Fifth;
    }
    if (Lower == TEXT("sixth") || Lower == TEXT("slot6") || Lower == TEXT("6"))
    {
        return EPlayerMappableKeySlot::Sixth;
    }
    if (Lower == TEXT("seventh") || Lower == TEXT("slot7") || Lower == TEXT("7"))
    {
        return EPlayerMappableKeySlot::Seventh;
    }
    if (Lower == TEXT("unspecified"))
    {
        return EPlayerMappableKeySlot::Unspecified;
    }
    return EPlayerMappableKeySlot::First;
}

TArray<FString> ReadStringArrayField(const TSharedPtr<FJsonObject>& Params, const FString& FieldName)
{
    TArray<FString> Result;
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (Params->TryGetArrayField(FieldName, Values) && Values)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            Result.Add(Value->AsString());
        }
    }
    return Result;
}

TSharedPtr<FJsonObject> ConfigureInputActionFromParams(UInputAction* Action, const TSharedPtr<FJsonObject>& Params)
{
    if (!Action)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Input Action is null"));
    }

    Action->Modify();

    FString StringValue;
    if (Params->TryGetStringField(TEXT("value_type"), StringValue))
    {
        Action->ValueType = ParseValueType(StringValue);
    }
    if (Params->TryGetStringField(TEXT("description"), StringValue))
    {
        Action->ActionDescription = FText::FromString(StringValue);
    }
    if (Params->TryGetStringField(TEXT("accumulation_behavior"), StringValue))
    {
        Action->AccumulationBehavior = ParseAccumulationBehavior(StringValue);
    }

    bool bBool = false;
    if (Params->TryGetBoolField(TEXT("trigger_when_paused"), bBool))
    {
        Action->bTriggerWhenPaused = bBool;
    }
    if (Params->TryGetBoolField(TEXT("consume_input"), bBool))
    {
        Action->bConsumeInput = bBool;
    }
    if (Params->TryGetBoolField(TEXT("consume_legacy_action_and_axis_mappings"), bBool))
    {
        Action->bConsumesActionAndAxisMappings = bBool;
    }
    if (Params->TryGetBoolField(TEXT("reserve_all_mappings"), bBool))
    {
        Action->bReserveAllMappings = bBool;
    }

    const TArray<TSharedPtr<FJsonValue>>* TriggerSpecs = nullptr;
    if (Params->TryGetArrayField(TEXT("triggers"), TriggerSpecs) && TriggerSpecs)
    {
        FString Error;
        if (!ApplyTriggerSpecs(Action, *TriggerSpecs, Action->Triggers, Error))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* ModifierSpecs = nullptr;
    if (Params->TryGetArrayField(TEXT("modifiers"), ModifierSpecs) && ModifierSpecs)
    {
        FString Error;
        if (!ApplyModifierSpecs(Action, *ModifierSpecs, Action->Modifiers, Error))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
        }
    }

    Action->MarkPackageDirty();
    return nullptr;
}

TSharedPtr<FJsonObject> ApplyMappingConfiguration(UInputMappingContext* Context, FEnhancedActionKeyMapping& Mapping, const TSharedPtr<FJsonObject>& Params)
{
    if (!Context)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Input Mapping Context is null"));
    }

    FString KeyName;
    if (Params->TryGetStringField(TEXT("new_key"), KeyName) || Params->TryGetStringField(TEXT("key"), KeyName) || Params->TryGetStringField(TEXT("key_name"), KeyName))
    {
        FKey Key;
        FString Error;
        if (!TryGetKey(KeyName, Key, Error))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
        }
        Mapping.Key = Key;
    }

    const TArray<TSharedPtr<FJsonValue>>* TriggerSpecs = nullptr;
    if (Params->TryGetArrayField(TEXT("triggers"), TriggerSpecs) && TriggerSpecs)
    {
        FString Error;
        if (!ApplyTriggerSpecs(Context, *TriggerSpecs, Mapping.Triggers, Error))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* ModifierSpecs = nullptr;
    if (Params->TryGetArrayField(TEXT("modifiers"), ModifierSpecs) && ModifierSpecs)
    {
        FString Error;
        if (!ApplyModifierSpecs(Context, *ModifierSpecs, Mapping.Modifiers, Error))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
        }
    }

    ApplyPlayerMappableSettings(Mapping, Context, Params);
    return nullptr;
}

void AddDefaultMappingContextSetting(UEnhancedInputDeveloperSettings* Settings, UInputMappingContext* Context, int32 Priority, bool bAddImmediately, bool bRegisterWithUserSettings)
{
    if (!Settings || !Context)
    {
        return;
    }
    FDefaultContextSetting Setting;
    Setting.InputMappingContext = Context;
    Setting.Priority = Priority;
    Setting.bAddImmediately = bAddImmediately;
    Setting.bRegisterWithUserSettings = bRegisterWithUserSettings;
    Settings->DefaultMappingContexts.Add(Setting);
}
}

FEpicUnrealMCPEnhancedInputCommands::FEpicUnrealMCPEnhancedInputCommands() {}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPEnhancedInputCommands::*)(const TSharedPtr<FJsonObject>&);
    static const TMap<FString, Handler> Dispatch = {
        {TEXT("create_input_action"), &FEpicUnrealMCPEnhancedInputCommands::HandleCreateInputAction},
        {TEXT("create_input_mapping_context"), &FEpicUnrealMCPEnhancedInputCommands::HandleCreateInputMappingContext},
        {TEXT("add_enhanced_input_mapping"), &FEpicUnrealMCPEnhancedInputCommands::HandleAddEnhancedInputMapping},
        {TEXT("remove_enhanced_input_mapping"), &FEpicUnrealMCPEnhancedInputCommands::HandleRemoveEnhancedInputMapping},
        {TEXT("configure_enhanced_input_action"), &FEpicUnrealMCPEnhancedInputCommands::HandleConfigureEnhancedInputAction},
        {TEXT("configure_enhanced_input_mapping"), &FEpicUnrealMCPEnhancedInputCommands::HandleConfigureEnhancedInputMapping},
        {TEXT("list_enhanced_input_assets"), &FEpicUnrealMCPEnhancedInputCommands::HandleListEnhancedInputAssets},
        {TEXT("get_enhanced_input_debug_info"), &FEpicUnrealMCPEnhancedInputCommands::HandleGetEnhancedInputDebugInfo},
        {TEXT("add_runtime_mapping_context"), &FEpicUnrealMCPEnhancedInputCommands::HandleAddRuntimeMappingContext},
        {TEXT("remove_runtime_mapping_context"), &FEpicUnrealMCPEnhancedInputCommands::HandleRemoveRuntimeMappingContext},
        {TEXT("setup_enhanced_input_binding"), &FEpicUnrealMCPEnhancedInputCommands::HandleSetupEnhancedInputBinding},
        {TEXT("setup_rebind_ui"), &FEpicUnrealMCPEnhancedInputCommands::HandleSetupRebindUI},
        {TEXT("rebind_enhanced_input_key"), &FEpicUnrealMCPEnhancedInputCommands::HandleRebindEnhancedInputKey},
        {TEXT("configure_local_multiplayer_input"), &FEpicUnrealMCPEnhancedInputCommands::HandleConfigureLocalMultiplayerInput},
    };

    const Handler* Found = Dispatch.Find(CommandType);
    if (!Found)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown Enhanced Input command: %s"), *CommandType));
    }
    return (this->*(*Found))(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleCreateInputAction(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) && !Params->TryGetStringField(TEXT("input_action_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path"));
    }

    FString PackagePath;
    FString Error;
    if (!ValidateLongPackagePath(AssetPath, PackagePath, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    bool bOverwrite = false;
    Params->TryGetBoolField(TEXT("overwrite"), bOverwrite);
    if (UInputAction* Existing = LoadObject<UInputAction>(nullptr, *ToObjectPath(PackagePath)))
    {
        if (!bOverwrite)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Input Action already exists: %s. Pass overwrite=true to reconfigure it."), *Existing->GetPathName()));
        }
        if (TSharedPtr<FJsonObject> ConfigureError = ConfigureInputActionFromParams(Existing, Params))
        {
            return ConfigureError;
        }
        if (!SaveAsset(Existing, Error))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
        }
        TSharedPtr<FJsonObject> Data = InputActionToJson(Existing);
        Data->SetBoolField(TEXT("created"), false);
        return CreateSuccess(Data);
    }

    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create package '%s'"), *PackagePath));
    }

    UInputAction* Action = NewObject<UInputAction>(Package, FName(*FPackageName::GetShortName(PackagePath)), RF_Public | RF_Standalone | RF_Transactional);
    if (!Action)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create UInputAction"));
    }

    if (TSharedPtr<FJsonObject> ConfigureError = ConfigureInputActionFromParams(Action, Params))
    {
        return ConfigureError;
    }

    FAssetRegistryModule::AssetCreated(Action);
    if (!SaveAsset(Action, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    TSharedPtr<FJsonObject> Data = InputActionToJson(Action);
    Data->SetBoolField(TEXT("created"), true);
    return CreateSuccess(Data);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleCreateInputMappingContext(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) && !Params->TryGetStringField(TEXT("mapping_context_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path"));
    }

    FString PackagePath;
    FString Error;
    if (!ValidateLongPackagePath(AssetPath, PackagePath, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    bool bOverwrite = false;
    Params->TryGetBoolField(TEXT("overwrite"), bOverwrite);
    UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, *ToObjectPath(PackagePath));
    bool bCreated = false;
    if (Context)
    {
        if (!bOverwrite)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Input Mapping Context already exists: %s. Pass overwrite=true to clear/reconfigure it."), *Context->GetPathName()));
        }
        Context->Modify();
        bool bClearMappings = true;
        Params->TryGetBoolField(TEXT("clear_mappings"), bClearMappings);
        if (bClearMappings)
        {
            Context->UnmapAll();
        }
    }
    else
    {
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create package '%s'"), *PackagePath));
        }
        Context = NewObject<UInputMappingContext>(Package, FName(*FPackageName::GetShortName(PackagePath)), RF_Public | RF_Standalone | RF_Transactional);
        if (!Context)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create UInputMappingContext"));
        }
        FAssetRegistryModule::AssetCreated(Context);
        bCreated = true;
    }

    FString Description;
    if (Params->TryGetStringField(TEXT("description"), Description))
    {
        Context->ContextDescription = FText::FromString(Description);
    }

    const TArray<TSharedPtr<FJsonValue>>* Mappings = nullptr;
    if (Params->TryGetArrayField(TEXT("mappings"), Mappings) && Mappings)
    {
        for (const TSharedPtr<FJsonValue>& MappingValue : *Mappings)
        {
            TSharedPtr<FJsonObject> MappingSpec = MappingValue->AsObject();
            if (!MappingSpec.IsValid())
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Each mapping entry must be an object"));
            }
            FString ActionPath;
            if (!MappingSpec->TryGetStringField(TEXT("input_action_path"), ActionPath) && !MappingSpec->TryGetStringField(TEXT("action_path"), ActionPath))
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Mapping entry missing input_action_path"));
            }
            FString KeyName;
            if (!MappingSpec->TryGetStringField(TEXT("key"), KeyName) && !MappingSpec->TryGetStringField(TEXT("key_name"), KeyName))
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Mapping entry missing key"));
            }

            UInputAction* Action = LoadEnhancedInputAsset<UInputAction>(ActionPath, Error);
            if (!Action)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
            }
            FKey Key;
            if (!TryGetKey(KeyName, Key, Error))
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
            }

            FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
            if (TSharedPtr<FJsonObject> MappingError = ApplyMappingConfiguration(Context, Mapping, MappingSpec))
            {
                return MappingError;
            }
        }
    }

    Context->MarkPackageDirty();
    UEnhancedInputLibrary::RequestRebuildControlMappingsUsingContext(Context, true);
    if (!SaveAsset(Context, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("asset_path"), Context->GetPathName());
    Data->SetStringField(TEXT("name"), Context->GetName());
    Data->SetBoolField(TEXT("created"), bCreated);
    Data->SetNumberField(TEXT("mapping_count"), Context->GetMappings().Num());
    return CreateSuccess(Data);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleAddEnhancedInputMapping(const TSharedPtr<FJsonObject>& Params)
{
    FString ContextPath;
    if (!Params->TryGetStringField(TEXT("mapping_context_path"), ContextPath) && !Params->TryGetStringField(TEXT("input_mapping_context_path"), ContextPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing mapping_context_path"));
    }
    FString ActionPath;
    if (!Params->TryGetStringField(TEXT("input_action_path"), ActionPath) && !Params->TryGetStringField(TEXT("action_path"), ActionPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing input_action_path"));
    }
    FString KeyName;
    if (!Params->TryGetStringField(TEXT("key"), KeyName) && !Params->TryGetStringField(TEXT("key_name"), KeyName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing key"));
    }

    FString Error;
    UInputMappingContext* Context = LoadEnhancedInputAsset<UInputMappingContext>(ContextPath, Error);
    if (!Context)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }
    UInputAction* Action = LoadEnhancedInputAsset<UInputAction>(ActionPath, Error);
    if (!Action)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }
    FKey Key;
    if (!TryGetKey(KeyName, Key, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    Context->Modify();
    FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
    if (TSharedPtr<FJsonObject> MappingError = ApplyMappingConfiguration(Context, Mapping, Params))
    {
        return MappingError;
    }

    Context->MarkPackageDirty();
    UEnhancedInputLibrary::RequestRebuildControlMappingsUsingContext(Context, true);
    if (!SaveAsset(Context, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    TSharedPtr<FJsonObject> Data = MappingToJson(Mapping, Context->GetMappings().Num() - 1);
    Data->SetStringField(TEXT("mapping_context_path"), Context->GetPathName());
    return CreateSuccess(Data);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleRemoveEnhancedInputMapping(const TSharedPtr<FJsonObject>& Params)
{
    FString ContextPath;
    if (!Params->TryGetStringField(TEXT("mapping_context_path"), ContextPath) && !Params->TryGetStringField(TEXT("input_mapping_context_path"), ContextPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing mapping_context_path"));
    }

    FString Error;
    UInputMappingContext* Context = LoadEnhancedInputAsset<UInputMappingContext>(ContextPath, Error);
    if (!Context)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    bool bClearAll = false;
    Params->TryGetBoolField(TEXT("clear_all"), bClearAll);
    Context->Modify();
    const int32 BeforeCount = Context->GetMappings().Num();
    if (bClearAll)
    {
        Context->UnmapAll();
    }
    else
    {
        FString ActionPath;
        if (!Params->TryGetStringField(TEXT("input_action_path"), ActionPath) && !Params->TryGetStringField(TEXT("action_path"), ActionPath))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing input_action_path unless clear_all=true"));
        }
        UInputAction* Action = LoadEnhancedInputAsset<UInputAction>(ActionPath, Error);
        if (!Action)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
        }

        FString KeyName;
        if (Params->TryGetStringField(TEXT("key"), KeyName) || Params->TryGetStringField(TEXT("key_name"), KeyName))
        {
            FKey Key;
            if (!TryGetKey(KeyName, Key, Error))
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
            }
            Context->UnmapKey(Action, Key);
        }
        else
        {
            bool bAllForAction = false;
            Params->TryGetBoolField(TEXT("remove_all_for_action"), bAllForAction);
            if (!bAllForAction)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing key. Pass remove_all_for_action=true to remove every key for the action."));
            }
            Context->UnmapAllKeysFromAction(Action);
        }
    }

    UEnhancedInputLibrary::RequestRebuildControlMappingsUsingContext(Context, true);
    if (!SaveAsset(Context, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("mapping_context_path"), Context->GetPathName());
    Data->SetNumberField(TEXT("previous_mapping_count"), BeforeCount);
    Data->SetNumberField(TEXT("mapping_count"), Context->GetMappings().Num());
    Data->SetNumberField(TEXT("removed_count"), BeforeCount - Context->GetMappings().Num());
    return CreateSuccess(Data);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleConfigureEnhancedInputAction(const TSharedPtr<FJsonObject>& Params)
{
    FString ActionPath;
    if (!Params->TryGetStringField(TEXT("input_action_path"), ActionPath) && !Params->TryGetStringField(TEXT("asset_path"), ActionPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing input_action_path"));
    }

    FString Error;
    UInputAction* Action = LoadEnhancedInputAsset<UInputAction>(ActionPath, Error);
    if (!Action)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }
    if (TSharedPtr<FJsonObject> ConfigureError = ConfigureInputActionFromParams(Action, Params))
    {
        return ConfigureError;
    }
    if (!SaveAsset(Action, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }
    return CreateSuccess(InputActionToJson(Action));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleConfigureEnhancedInputMapping(const TSharedPtr<FJsonObject>& Params)
{
    FString ContextPath;
    if (!Params->TryGetStringField(TEXT("mapping_context_path"), ContextPath) && !Params->TryGetStringField(TEXT("input_mapping_context_path"), ContextPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing mapping_context_path"));
    }

    FString Error;
    UInputMappingContext* Context = LoadEnhancedInputAsset<UInputMappingContext>(ContextPath, Error);
    if (!Context)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    int32 MappingIndex = INDEX_NONE;
    FEnhancedActionKeyMapping* Mapping = FindMapping(Context, Params, Error, &MappingIndex);
    if (!Mapping)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    Context->Modify();
    if (TSharedPtr<FJsonObject> MappingError = ApplyMappingConfiguration(Context, *Mapping, Params))
    {
        return MappingError;
    }
    Context->MarkPackageDirty();
    UEnhancedInputLibrary::RequestRebuildControlMappingsUsingContext(Context, true);
    if (!SaveAsset(Context, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    TSharedPtr<FJsonObject> Data = MappingToJson(*Mapping, MappingIndex);
    Data->SetStringField(TEXT("mapping_context_path"), Context->GetPathName());
    return CreateSuccess(Data);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleListEnhancedInputAssets(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = TEXT("/Game");
    Params->TryGetStringField(TEXT("path"), Path);
    bool bRecursive = true;
    Params->TryGetBoolField(TEXT("recursive"), bRecursive);

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    FARFilter ActionFilter;
    ActionFilter.PackagePaths.Add(FName(*Path));
    ActionFilter.ClassPaths.Add(UInputAction::StaticClass()->GetClassPathName());
    ActionFilter.bRecursivePaths = bRecursive;

    FARFilter ContextFilter;
    ContextFilter.PackagePaths.Add(FName(*Path));
    ContextFilter.ClassPaths.Add(UInputMappingContext::StaticClass()->GetClassPathName());
    ContextFilter.bRecursivePaths = bRecursive;

    TArray<FAssetData> ActionAssets;
    TArray<FAssetData> ContextAssets;
    AssetRegistry.GetAssets(ActionFilter, ActionAssets);
    AssetRegistry.GetAssets(ContextFilter, ContextAssets);

    TArray<TSharedPtr<FJsonValue>> Actions;
    for (const FAssetData& Asset : ActionAssets)
    {
        TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("asset_path"), Asset.GetObjectPathString());
        Object->SetStringField(TEXT("package_name"), Asset.PackageName.ToString());
        Object->SetStringField(TEXT("name"), Asset.AssetName.ToString());
        Actions.Add(MakeShared<FJsonValueObject>(Object));
    }

    TArray<TSharedPtr<FJsonValue>> Contexts;
    for (const FAssetData& Asset : ContextAssets)
    {
        TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("asset_path"), Asset.GetObjectPathString());
        Object->SetStringField(TEXT("package_name"), Asset.PackageName.ToString());
        Object->SetStringField(TEXT("name"), Asset.AssetName.ToString());
        Contexts.Add(MakeShared<FJsonValueObject>(Object));
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetArrayField(TEXT("input_actions"), Actions);
    Data->SetArrayField(TEXT("input_mapping_contexts"), Contexts);
    Data->SetNumberField(TEXT("input_action_count"), Actions.Num());
    Data->SetNumberField(TEXT("input_mapping_context_count"), Contexts.Num());
    return CreateSuccess(Data);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleGetEnhancedInputDebugInfo(const TSharedPtr<FJsonObject>& Params)
{
    int32 PlayerIndex = 0;
    double PlayerIndexNumber = 0.0;
    if (Params->TryGetNumberField(TEXT("player_index"), PlayerIndexNumber))
    {
        PlayerIndex = static_cast<int32>(PlayerIndexNumber);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("runtime_world_active"), GetRuntimeWorld() != nullptr);
    UClass* DefaultPlayerInputClass = UInputSettings::GetDefaultPlayerInputClass();
    UClass* DefaultInputComponentClass = UInputSettings::GetDefaultInputComponentClass();
    Data->SetStringField(TEXT("default_player_input_class"), DefaultPlayerInputClass ? DefaultPlayerInputClass->GetPathName() : FString());
    Data->SetStringField(TEXT("default_input_component_class"), DefaultInputComponentClass ? DefaultInputComponentClass->GetPathName() : FString());

    FString Error;
    ULocalPlayer* LocalPlayer = nullptr;
    UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedInputSubsystem(PlayerIndex, Error, &LocalPlayer);
    if (!Subsystem)
    {
        Data->SetStringField(TEXT("runtime_warning"), Error);
        return CreateSuccess(Data);
    }

    Data->SetNumberField(TEXT("player_index"), PlayerIndex);
    Data->SetStringField(TEXT("input_mode"), Subsystem->GetInputMode().ToStringSimple());
    Data->SetBoolField(TEXT("user_settings_available"), Subsystem->GetUserSettings() != nullptr);
    if (UEnhancedInputUserSettings* Settings = Subsystem->GetUserSettings())
    {
        Data->SetStringField(TEXT("active_key_profile_id"), Settings->GetActiveKeyProfileId());
        TArray<TSharedPtr<FJsonValue>> RegisteredContexts;
        for (const UInputMappingContext* Context : Settings->GetRegisteredInputMappingContexts())
        {
            RegisteredContexts.Add(MakeShared<FJsonValueString>(Context ? Context->GetPathName() : FString()));
        }
        Data->SetArrayField(TEXT("registered_mapping_contexts"), RegisteredContexts);
    }

    TArray<TSharedPtr<FJsonValue>> PlayerMappableMappings;
    for (const FEnhancedActionKeyMapping& Mapping : Subsystem->GetAllPlayerMappableActionKeyMappings())
    {
        PlayerMappableMappings.Add(MakeShared<FJsonValueObject>(MappingToJson(Mapping, PlayerMappableMappings.Num())));
    }
    Data->SetArrayField(TEXT("player_mappable_mappings"), PlayerMappableMappings);
    Data->SetNumberField(TEXT("player_mappable_mapping_count"), PlayerMappableMappings.Num());

    TArray<FString> ContextPaths = ReadStringArrayField(Params, TEXT("mapping_context_paths"));
    FString SingleContextPath;
    if (Params->TryGetStringField(TEXT("mapping_context_path"), SingleContextPath))
    {
        ContextPaths.Add(SingleContextPath);
    }
    TArray<TSharedPtr<FJsonValue>> ContextStates;
    for (const FString& ContextPath : ContextPaths)
    {
        UInputMappingContext* Context = LoadEnhancedInputAsset<UInputMappingContext>(ContextPath, Error);
        TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("mapping_context_path"), ContextPath);
        if (Context)
        {
            int32 Priority = -1;
            Object->SetBoolField(TEXT("applied"), Subsystem->HasMappingContext(Context, Priority));
            Object->SetNumberField(TEXT("priority"), Priority);
            Object->SetNumberField(TEXT("mapping_count"), Context->GetMappings().Num());
        }
        else
        {
            Object->SetBoolField(TEXT("applied"), false);
            Object->SetStringField(TEXT("error"), Error);
        }
        ContextStates.Add(MakeShared<FJsonValueObject>(Object));
    }
    Data->SetArrayField(TEXT("mapping_context_states"), ContextStates);
    return CreateSuccess(Data);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleAddRuntimeMappingContext(const TSharedPtr<FJsonObject>& Params)
{
    FString ContextPath;
    if (!Params->TryGetStringField(TEXT("mapping_context_path"), ContextPath) && !Params->TryGetStringField(TEXT("input_mapping_context_path"), ContextPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing mapping_context_path"));
    }

    int32 PlayerIndex = 0;
    double NumberValue = 0.0;
    if (Params->TryGetNumberField(TEXT("player_index"), NumberValue))
    {
        PlayerIndex = static_cast<int32>(NumberValue);
    }
    int32 Priority = 0;
    if (Params->TryGetNumberField(TEXT("priority"), NumberValue))
    {
        Priority = static_cast<int32>(NumberValue);
    }

    FString Error;
    UInputMappingContext* Context = LoadEnhancedInputAsset<UInputMappingContext>(ContextPath, Error);
    if (!Context)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }
    UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedInputSubsystem(PlayerIndex, Error);
    if (!Subsystem)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    const FModifyContextOptions Options = ReadModifyContextOptions(Params);
    Subsystem->AddMappingContext(Context, Priority, Options);
    if (Options.bForceImmediately)
    {
        Subsystem->RequestRebuildControlMappings(Options, EInputMappingRebuildType::RebuildWithFlush);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("mapping_context_path"), Context->GetPathName());
    Data->SetNumberField(TEXT("player_index"), PlayerIndex);
    Data->SetNumberField(TEXT("priority"), Priority);
    Data->SetBoolField(TEXT("force_immediately"), Options.bForceImmediately);
    return CreateSuccess(Data);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleRemoveRuntimeMappingContext(const TSharedPtr<FJsonObject>& Params)
{
    FString ContextPath;
    if (!Params->TryGetStringField(TEXT("mapping_context_path"), ContextPath) && !Params->TryGetStringField(TEXT("input_mapping_context_path"), ContextPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing mapping_context_path"));
    }

    int32 PlayerIndex = 0;
    double NumberValue = 0.0;
    if (Params->TryGetNumberField(TEXT("player_index"), NumberValue))
    {
        PlayerIndex = static_cast<int32>(NumberValue);
    }

    FString Error;
    UInputMappingContext* Context = LoadEnhancedInputAsset<UInputMappingContext>(ContextPath, Error);
    if (!Context)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }
    UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedInputSubsystem(PlayerIndex, Error);
    if (!Subsystem)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    const FModifyContextOptions Options = ReadModifyContextOptions(Params);
    Subsystem->RemoveMappingContext(Context, Options);
    if (Options.bForceImmediately)
    {
        Subsystem->RequestRebuildControlMappings(Options, EInputMappingRebuildType::RebuildWithFlush);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("mapping_context_path"), Context->GetPathName());
    Data->SetNumberField(TEXT("player_index"), PlayerIndex);
    Data->SetBoolField(TEXT("force_immediately"), Options.bForceImmediately);
    return CreateSuccess(Data);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleSetupEnhancedInputBinding(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing blueprint_path"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    FString TargetType;
    Params->TryGetStringField(TEXT("target_type"), TargetType);
    FString TargetWarning;
    if (Blueprint->GeneratedClass)
    {
        if (TargetType.Equals(TEXT("player_controller"), ESearchCase::IgnoreCase) && !Blueprint->GeneratedClass->IsChildOf(APlayerController::StaticClass()))
        {
            TargetWarning = TEXT("Blueprint is not currently a PlayerController subclass; generated nodes are valid only if the Blueprint supports input events.");
        }
        if (TargetType.Equals(TEXT("character"), ESearchCase::IgnoreCase) && !Blueprint->GeneratedClass->IsChildOf(ACharacter::StaticClass()))
        {
            TargetWarning = TEXT("Blueprint is not currently a Character subclass; generated nodes are valid only if the Blueprint supports input events.");
        }
    }

    UEdGraph* Graph = FEpicUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!Graph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to find or create EventGraph"));
    }

    TArray<TSharedPtr<FJsonObject>> BindingSpecs;
    const TArray<TSharedPtr<FJsonValue>>* Bindings = nullptr;
    if (Params->TryGetArrayField(TEXT("bindings"), Bindings) && Bindings)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Bindings)
        {
            if (TSharedPtr<FJsonObject> Object = Value->AsObject())
            {
                BindingSpecs.Add(Object);
            }
        }
    }
    else
    {
        FString ActionPath;
        if (Params->TryGetStringField(TEXT("input_action_path"), ActionPath) || Params->TryGetStringField(TEXT("action_path"), ActionPath))
        {
            TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
            Object->SetStringField(TEXT("input_action_path"), ActionPath);
            FString TriggerEvent;
            if (Params->TryGetStringField(TEXT("trigger_event"), TriggerEvent))
            {
                Object->SetStringField(TEXT("trigger_event"), TriggerEvent);
            }
            FString HandlerFunctionName;
            if (Params->TryGetStringField(TEXT("handler_function_name"), HandlerFunctionName))
            {
                Object->SetStringField(TEXT("handler_function_name"), HandlerFunctionName);
            }
            BindingSpecs.Add(Object);
        }
    }

    if (BindingSpecs.Num() == 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No bindings provided. Pass input_action_path or bindings array."));
    }

    double PosXNumber = 200.0;
    double PosYNumber = 200.0;
    Params->TryGetNumberField(TEXT("pos_x"), PosXNumber);
    Params->TryGetNumberField(TEXT("pos_y"), PosYNumber);

    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    Graph->Modify();
    Blueprint->Modify();

    TArray<TSharedPtr<FJsonValue>> CreatedNodes;
    FString Error;
    for (int32 BindingIndex = 0; BindingIndex < BindingSpecs.Num(); ++BindingIndex)
    {
        const TSharedPtr<FJsonObject>& Binding = BindingSpecs[BindingIndex];
        FString ActionPath;
        if (!Binding->TryGetStringField(TEXT("input_action_path"), ActionPath) && !Binding->TryGetStringField(TEXT("action_path"), ActionPath))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Binding entry missing input_action_path"));
        }

        UInputAction* Action = LoadEnhancedInputAsset<UInputAction>(ActionPath, Error);
        if (!Action)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
        }

        UK2Node_EnhancedInputAction* InputNode = nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            UK2Node_EnhancedInputAction* Existing = Cast<UK2Node_EnhancedInputAction>(Node);
            if (Existing && Existing->InputAction == Action)
            {
                InputNode = Existing;
                break;
            }
        }

        if (!InputNode)
        {
            InputNode = NewObject<UK2Node_EnhancedInputAction>(Graph);
            InputNode->InputAction = Action;
            InputNode->NodePosX = static_cast<int32>(PosXNumber);
            InputNode->NodePosY = static_cast<int32>(PosYNumber + BindingIndex * 260.0);
            Graph->AddNode(InputNode, true, false);
            InputNode->CreateNewGuid();
            InputNode->PostPlacedNewNode();
            InputNode->AllocateDefaultPins();
        }

        FString TriggerEventName = TEXT("Triggered");
        Binding->TryGetStringField(TEXT("trigger_event"), TriggerEventName);
        UEdGraphPin* EventPin = InputNode->FindPin(FName(*TriggerEventToPinName(ParseTriggerEvent(TriggerEventName))), EGPD_Output);

        FString HandlerFunctionName;
        bool bConnectedHandler = false;
        if (Binding->TryGetStringField(TEXT("handler_function_name"), HandlerFunctionName) && !HandlerFunctionName.IsEmpty())
        {
            UFunction* HandlerFunction = nullptr;
            if (Blueprint->SkeletonGeneratedClass)
            {
                HandlerFunction = Blueprint->SkeletonGeneratedClass->FindFunctionByName(FName(*HandlerFunctionName));
            }
            if (!HandlerFunction && Blueprint->GeneratedClass)
            {
                HandlerFunction = Blueprint->GeneratedClass->FindFunctionByName(FName(*HandlerFunctionName));
            }
            if (HandlerFunction && EventPin)
            {
                UK2Node_CallFunction* CallNode = FEpicUnrealMCPCommonUtils::CreateFunctionCallNode(
                    Graph,
                    HandlerFunction,
                    FVector2D(InputNode->NodePosX + 440.0, InputNode->NodePosY));
                if (CallNode && CallNode->GetExecPin())
                {
                    Schema->TryCreateConnection(EventPin, CallNode->GetExecPin());
                    bConnectedHandler = true;
                }
            }
        }

        TSharedPtr<FJsonObject> NodeInfo = MakeShared<FJsonObject>();
        NodeInfo->SetStringField(TEXT("input_action_path"), Action->GetPathName());
        NodeInfo->SetStringField(TEXT("node_id"), InputNode->NodeGuid.ToString());
        NodeInfo->SetStringField(TEXT("trigger_event"), TriggerEventToPinName(ParseTriggerEvent(TriggerEventName)));
        NodeInfo->SetBoolField(TEXT("connected_handler"), bConnectedHandler);
        if (!HandlerFunctionName.IsEmpty())
        {
            NodeInfo->SetStringField(TEXT("handler_function_name"), HandlerFunctionName);
        }
        CreatedNodes.Add(MakeShared<FJsonValueObject>(NodeInfo));
    }

    Graph->NotifyGraphChanged();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    bool bCompile = true;
    Params->TryGetBoolField(TEXT("compile"), bCompile);
    if (bCompile)
    {
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
    }
    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    if (bSave && !SaveAsset(Blueprint, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
    Data->SetStringField(TEXT("graph_name"), Graph->GetName());
    Data->SetArrayField(TEXT("binding_nodes"), CreatedNodes);
    if (!TargetWarning.IsEmpty())
    {
        Data->SetStringField(TEXT("warning"), TargetWarning);
    }
    return CreateSuccess(Data);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleSetupRebindUI(const TSharedPtr<FJsonObject>& Params)
{
    UEnhancedInputDeveloperSettings* Settings = GetMutableDefault<UEnhancedInputDeveloperSettings>();
    if (!Settings)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Enhanced Input developer settings are unavailable. Ensure the EnhancedInput plugin is enabled."));
    }

    bool bEnableUserSettings = true;
    Params->TryGetBoolField(TEXT("enable_user_settings"), bEnableUserSettings);
    Settings->bEnableUserSettings = bEnableUserSettings;
    Settings->TryUpdateDefaultConfigFile();

    TArray<FString> ContextPaths = ReadStringArrayField(Params, TEXT("mapping_context_paths"));
    FString SingleContextPath;
    if (Params->TryGetStringField(TEXT("mapping_context_path"), SingleContextPath))
    {
        ContextPaths.Add(SingleContextPath);
    }

    bool bMarkAllMappings = true;
    Params->TryGetBoolField(TEXT("mark_all_mappings_player_mappable"), bMarkAllMappings);

    TArray<TSharedPtr<FJsonValue>> RegisteredContexts;
    FString Error;
    for (const FString& ContextPath : ContextPaths)
    {
        UInputMappingContext* Context = LoadEnhancedInputAsset<UInputMappingContext>(ContextPath, Error);
        if (!Context)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
        }

        if (bMarkAllMappings)
        {
            Context->Modify();
            for (int32 Index = 0; Index < Context->GetMappings().Num(); ++Index)
            {
                FEnhancedActionKeyMapping& Mapping = Context->GetMapping(Index);
                TSharedPtr<FJsonObject> MappableParams = MakeShared<FJsonObject>();
                MappableParams->SetBoolField(TEXT("player_mappable"), true);
                MappableParams->SetStringField(TEXT("mapping_name"), Mapping.Action ? Mapping.Action->GetName() : Mapping.Key.GetFName().ToString());
                MappableParams->SetStringField(TEXT("display_name"), Mapping.Action ? Mapping.Action->GetName() : Mapping.Key.GetFName().ToString());
                MappableParams->SetStringField(TEXT("display_category"), TEXT("Input"));
                ApplyPlayerMappableSettings(Mapping, Context, MappableParams);
            }
            UEnhancedInputLibrary::RequestRebuildControlMappingsUsingContext(Context, true);
            if (!SaveAsset(Context, Error))
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
            }
        }

        RegisteredContexts.Add(MakeShared<FJsonValueString>(Context->GetPathName()));
    }

    int32 PlayerIndex = 0;
    double PlayerIndexNumber = 0.0;
    if (Params->TryGetNumberField(TEXT("player_index"), PlayerIndexNumber))
    {
        PlayerIndex = static_cast<int32>(PlayerIndexNumber);
    }

    TArray<TSharedPtr<FJsonValue>> RuntimeRegisteredContexts;
    ULocalPlayer* LocalPlayer = nullptr;
    UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedInputSubsystem(PlayerIndex, Error, &LocalPlayer);
    if (Subsystem && LocalPlayer)
    {
        UEnhancedInputUserSettings* UserSettings = Subsystem->GetUserSettings();
        if (!UserSettings)
        {
            UserSettings = UEnhancedInputUserSettings::LoadOrCreateSettings(LocalPlayer);
        }
        if (UserSettings)
        {
            FString ProfileId;
            if (Params->TryGetStringField(TEXT("profile_id"), ProfileId) && !ProfileId.IsEmpty())
            {
                UserSettings->SetActiveKeyProfile(ProfileId);
            }
            for (const FString& ContextPath : ContextPaths)
            {
                UInputMappingContext* Context = LoadEnhancedInputAsset<UInputMappingContext>(ContextPath, Error);
                if (Context && UserSettings->RegisterInputMappingContext(Context))
                {
                    RuntimeRegisteredContexts.Add(MakeShared<FJsonValueString>(Context->GetPathName()));
                }
            }
            bool bSaveSettings = true;
            Params->TryGetBoolField(TEXT("save_settings"), bSaveSettings);
            if (bSaveSettings)
            {
                UserSettings->SaveSettings();
            }
        }
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("enhanced_input_user_settings_enabled"), Settings->bEnableUserSettings);
    Data->SetArrayField(TEXT("configured_mapping_contexts"), RegisteredContexts);
    Data->SetArrayField(TEXT("runtime_registered_mapping_contexts"), RuntimeRegisteredContexts);
    if (!Subsystem)
    {
        Data->SetStringField(TEXT("runtime_warning"), Error);
    }
    return CreateSuccess(Data);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleRebindEnhancedInputKey(const TSharedPtr<FJsonObject>& Params)
{
    FString MappingName;
    if (!Params->TryGetStringField(TEXT("mapping_name"), MappingName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing mapping_name"));
    }

    FString KeyName;
    if (!Params->TryGetStringField(TEXT("key"), KeyName) && !Params->TryGetStringField(TEXT("key_name"), KeyName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing key"));
    }

    int32 PlayerIndex = 0;
    double NumberValue = 0.0;
    if (Params->TryGetNumberField(TEXT("player_index"), NumberValue))
    {
        PlayerIndex = static_cast<int32>(NumberValue);
    }

    FString Error;
    FKey Key;
    if (!TryGetKey(KeyName, Key, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    ULocalPlayer* LocalPlayer = nullptr;
    UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedInputSubsystem(PlayerIndex, Error, &LocalPlayer);
    if (!Subsystem || !LocalPlayer)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    UEnhancedInputUserSettings* UserSettings = Subsystem->GetUserSettings();
    if (!UserSettings)
    {
        UserSettings = UEnhancedInputUserSettings::LoadOrCreateSettings(LocalPlayer);
    }
    if (!UserSettings)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Enhanced Input user settings are unavailable. Run setup_rebind_ui and ensure bEnableUserSettings is true."));
    }

    TArray<TSharedPtr<FJsonValue>> RuntimeRegisteredContexts;
    TArray<UInputMappingContext*> ExplicitContexts;
    TArray<FString> ContextPaths = ReadStringArrayField(Params, TEXT("mapping_context_paths"));
    FString SingleContextPath;
    if (Params->TryGetStringField(TEXT("mapping_context_path"), SingleContextPath) ||
        Params->TryGetStringField(TEXT("input_mapping_context_path"), SingleContextPath))
    {
        ContextPaths.Add(SingleContextPath);
    }
    for (const FString& ContextPath : ContextPaths)
    {
        UInputMappingContext* Context = LoadEnhancedInputAsset<UInputMappingContext>(ContextPath, Error);
        if (!Context)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
        }
        if (UserSettings->IsMappingContextRegistered(Context))
        {
            UserSettings->UnregisterInputMappingContext(Context);
        }
        UserSettings->RegisterInputMappingContext(Context);
        RuntimeRegisteredContexts.Add(MakeShared<FJsonValueString>(Context->GetPathName()));
        ExplicitContexts.Add(Context);
    }

    FMapPlayerKeyArgs Args;
    Args.NewKey = Key;
    FString SlotName = TEXT("first");
    Params->TryGetStringField(TEXT("slot"), SlotName);
    Args.Slot = ParseKeySlot(SlotName);
    FString HardwareDeviceId;
    if (Params->TryGetStringField(TEXT("hardware_device_id"), HardwareDeviceId) && !HardwareDeviceId.IsEmpty())
    {
        Args.HardwareDeviceId = FName(*HardwareDeviceId);
    }
    FString ProfileId;
    if (Params->TryGetStringField(TEXT("profile_id"), ProfileId))
    {
        Args.ProfileIdString = ProfileId;
    }
    bool bCreateSlot = true;
    Params->TryGetBoolField(TEXT("create_matching_slot_if_needed"), bCreateSlot);
    Args.bCreateMatchingSlotIfNeeded = bCreateSlot;

    TArray<FName> CandidateMappingNames;
    CandidateMappingNames.AddUnique(FName(*MappingName));
    if (!MappingName.StartsWith(TEXT("IA_")))
    {
        CandidateMappingNames.AddUnique(FName(*(TEXT("IA_") + MappingName)));
    }
    for (const UInputMappingContext* Context : ExplicitContexts)
    {
        for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
        {
            const FString RowName = Mapping.GetMappingName().ToString();
            const FString ActionName = Mapping.Action ? Mapping.Action->GetName() : FString();
            FString ActionNameWithoutPrefix = ActionName;
            ActionNameWithoutPrefix.RemoveFromStart(TEXT("IA_"));
            const FString DisplayName = Mapping.GetDisplayName().ToString();
            if (RowName.Equals(MappingName, ESearchCase::IgnoreCase) ||
                ActionName.Equals(MappingName, ESearchCase::IgnoreCase) ||
                ActionNameWithoutPrefix.Equals(MappingName, ESearchCase::IgnoreCase) ||
                DisplayName.Equals(MappingName, ESearchCase::IgnoreCase))
            {
                if (!RowName.IsEmpty())
                {
                    CandidateMappingNames.AddUnique(FName(*RowName));
                }
                if (!ActionName.IsEmpty())
                {
                    CandidateMappingNames.AddUnique(FName(*ActionName));
                }
            }
        }
    }

    FGameplayTagContainer FailureReason;
    FName AppliedMappingName = FName(*MappingName);
    bool bUnmap = false;
    Params->TryGetBoolField(TEXT("unmap"), bUnmap);
    bool bMapped = false;
    for (const FName& CandidateName : CandidateMappingNames)
    {
        Args.MappingName = CandidateName;
        FailureReason.Reset();
        if (bUnmap)
        {
            UserSettings->UnMapPlayerKey(Args, FailureReason);
        }
        else
        {
            UserSettings->MapPlayerKey(Args, FailureReason);
        }
        if (FailureReason.IsEmpty())
        {
            AppliedMappingName = CandidateName;
            bMapped = true;
            break;
        }
    }

    if (!bMapped)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to rebind '%s' to '%s': %s"), *MappingName, *Key.GetFName().ToString(), *FailureReason.ToStringSimple()));
    }

    UserSettings->ApplySettings();
    bool bSaveSettings = true;
    Params->TryGetBoolField(TEXT("save_settings"), bSaveSettings);
    if (bSaveSettings)
    {
        UserSettings->SaveSettings();
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("mapping_name"), AppliedMappingName.ToString());
    Data->SetStringField(TEXT("requested_mapping_name"), MappingName);
    Data->SetObjectField(TEXT("key"), KeyToJson(Key));
    Data->SetStringField(TEXT("slot"), StaticEnum<EPlayerMappableKeySlot>()->GetNameStringByValue(static_cast<int64>(Args.Slot)));
    Data->SetBoolField(TEXT("unmapped"), bUnmap);
    Data->SetBoolField(TEXT("saved"), bSaveSettings);
    Data->SetArrayField(TEXT("runtime_registered_mapping_contexts"), RuntimeRegisteredContexts);
    return CreateSuccess(Data);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleConfigureLocalMultiplayerInput(const TSharedPtr<FJsonObject>& Params)
{
    bool bFilterByPlatformUser = true;
    Params->TryGetBoolField(TEXT("filter_input_by_platform_user"), bFilterByPlatformUser);
    bool bEnableUserSettings = true;
    Params->TryGetBoolField(TEXT("enable_user_settings"), bEnableUserSettings);
    bool bEnableDefaultContexts = true;
    Params->TryGetBoolField(TEXT("enable_default_mapping_contexts"), bEnableDefaultContexts);

    const FString ConfigPath = FConfigCacheIni::NormalizeConfigIniPath(FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("DefaultInput.ini")));
    GConfig->SetBool(DefaultInputSettingsSection, TEXT("bFilterInputByPlatformUser"), bFilterByPlatformUser, *ConfigPath);
    GConfig->SetString(DefaultInputSettingsSection, TEXT("DefaultPlayerInputClass"), TEXT("/Script/EnhancedInput.EnhancedPlayerInput"), *ConfigPath);
    GConfig->SetString(DefaultInputSettingsSection, TEXT("DefaultInputComponentClass"), TEXT("/Script/EnhancedInput.EnhancedInputComponent"), *ConfigPath);
    GConfig->Flush(false, *ConfigPath);

    UEnhancedInputDeveloperSettings* Settings = GetMutableDefault<UEnhancedInputDeveloperSettings>();
    if (!Settings)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Enhanced Input developer settings are unavailable. Ensure the EnhancedInput plugin is enabled."));
    }

    Settings->bEnableUserSettings = bEnableUserSettings;
    Settings->bEnableDefaultMappingContexts = bEnableDefaultContexts;

    bool bReplaceDefaultContexts = false;
    Params->TryGetBoolField(TEXT("replace_default_contexts"), bReplaceDefaultContexts);
    if (bReplaceDefaultContexts)
    {
        Settings->DefaultMappingContexts.Reset();
    }

    int32 Priority = 0;
    double NumberValue = 0.0;
    if (Params->TryGetNumberField(TEXT("priority"), NumberValue))
    {
        Priority = static_cast<int32>(NumberValue);
    }

    TArray<FString> ContextPaths = ReadStringArrayField(Params, TEXT("mapping_context_paths"));
    FString SingleContextPath;
    if (Params->TryGetStringField(TEXT("mapping_context_path"), SingleContextPath))
    {
        ContextPaths.Add(SingleContextPath);
    }

    FString Error;
    for (const FString& ContextPath : ContextPaths)
    {
        UInputMappingContext* Context = LoadEnhancedInputAsset<UInputMappingContext>(ContextPath, Error);
        if (!Context)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
        }
        AddDefaultMappingContextSetting(Settings, Context, Priority, true, bEnableUserSettings);
    }
    Settings->TryUpdateDefaultConfigFile();

    int32 DesiredPlayers = 0;
    if (Params->TryGetNumberField(TEXT("create_local_players"), NumberValue) || Params->TryGetNumberField(TEXT("num_players"), NumberValue))
    {
        DesiredPlayers = FMath::Max(0, static_cast<int32>(NumberValue));
    }

    TArray<TSharedPtr<FJsonValue>> RuntimePlayers;
    UWorld* World = GetRuntimeWorld();
    if (World && DesiredPlayers > 0)
    {
        UGameInstance* GameInstance = World->GetGameInstance();
        const int32 ExistingPlayers = GameInstance ? GameInstance->GetNumLocalPlayers() : 0;
        for (int32 PlayerIndex = ExistingPlayers; PlayerIndex < DesiredPlayers; ++PlayerIndex)
        {
            UGameplayStatics::CreatePlayer(World, PlayerIndex, true);
        }

        for (int32 PlayerIndex = 0; PlayerIndex < DesiredPlayers; ++PlayerIndex)
        {
            FString SubsystemError;
            UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedInputSubsystem(PlayerIndex, SubsystemError);
            TSharedPtr<FJsonObject> PlayerObject = MakeShared<FJsonObject>();
            PlayerObject->SetNumberField(TEXT("player_index"), PlayerIndex);
            if (!Subsystem)
            {
                PlayerObject->SetStringField(TEXT("error"), SubsystemError);
            }
            else
            {
                for (const FString& ContextPath : ContextPaths)
                {
                    UInputMappingContext* Context = LoadEnhancedInputAsset<UInputMappingContext>(ContextPath, Error);
                    if (Context)
                    {
                        FModifyContextOptions Options;
                        Options.bForceImmediately = true;
                        Options.bNotifyUserSettings = bEnableUserSettings;
                        Subsystem->AddMappingContext(Context, Priority, Options);
                    }
                }
                PlayerObject->SetBoolField(TEXT("configured"), true);
            }
            RuntimePlayers.Add(MakeShared<FJsonValueObject>(PlayerObject));
        }
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("filter_input_by_platform_user"), bFilterByPlatformUser);
    Data->SetBoolField(TEXT("enable_user_settings"), Settings->bEnableUserSettings);
    Data->SetBoolField(TEXT("enable_default_mapping_contexts"), Settings->bEnableDefaultMappingContexts);
    Data->SetNumberField(TEXT("default_mapping_context_count"), Settings->DefaultMappingContexts.Num());
    Data->SetArrayField(TEXT("runtime_players"), RuntimePlayers);
    if (!World && DesiredPlayers > 0)
    {
        Data->SetStringField(TEXT("runtime_warning"), TEXT("Local player creation requires an active PIE/game world."));
    }
    return CreateSuccess(Data);
}
