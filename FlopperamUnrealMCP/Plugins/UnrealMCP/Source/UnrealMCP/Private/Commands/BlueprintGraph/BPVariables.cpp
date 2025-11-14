#include "Commands/BlueprintGraph/BPVariables.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Engine/Blueprint.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "EditorSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"

TSharedPtr<FJsonObject> FBPVariables::CreateVariable(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    FString BlueprintName = Params->GetStringField(TEXT("blueprint_name"));
    FString VariableName = Params->GetStringField(TEXT("variable_name"));
    FString VariableType = Params->GetStringField(TEXT("variable_type"));

    bool IsPublic = Params->HasField(TEXT("is_public")) ? Params->GetBoolField(TEXT("is_public")) : false;
    FString Tooltip = Params->HasField(TEXT("tooltip")) ? Params->GetStringField(TEXT("tooltip")) : TEXT("");
    FString Category = Params->HasField(TEXT("category")) ? Params->GetStringField(TEXT("category")) : TEXT("Default");

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);

    if (!Blueprint)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Blueprint not found");
        return Result;
    }

    FEdGraphPinType VarType = GetPinTypeFromString(VariableType);
    FName VarName = FName(*VariableName);

    if (FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarName, VarType))
    {
        FBPVariableDescription& Variable = Blueprint->NewVariables.Last();
        Variable.FriendlyName = VariableName;
        Variable.Category = FText::FromString(Category);
        Variable.PropertyFlags = CPF_BlueprintVisible | CPF_BlueprintReadOnly;
        if (IsPublic)
        {
            Variable.PropertyFlags |= CPF_Edit;
        }

        if (!Tooltip.IsEmpty())
        {
            Variable.SetMetaData(FBlueprintMetadata::MD_Tooltip, Tooltip);
        }

        if (Params->HasField(TEXT("default_value")))
        {
            SetDefaultValue(Variable, Params->Values.FindRef("default_value"));
        }

        Blueprint->MarkPackageDirty();

        // Force immediate refresh of the Blueprint editor
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        // Force asset registry update
        if (GEditor)
        {
            // Note: Asset registry notifications removed for UE5.5 compatibility
            // FAssetRegistryModule::AssetRegistryHelpers::GetAssetRegistry().AssetCreated(Blueprint);

            // Broadcast compilation event to refresh all editors
            // GEditor->BroadcastBlueprintCompiled(Blueprint); // Removed for UE5.5 compatibility

            // Additional refresh for property windows
            FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
            PropertyModule.NotifyCustomizationModuleChanged();
        }

        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        Result->SetBoolField("success", true);

        TSharedPtr<FJsonObject> VarInfo = MakeShared<FJsonObject>();
        VarInfo->SetStringField("name", VariableName);
        VarInfo->SetStringField("type", VariableType);
        VarInfo->SetBoolField("is_public", IsPublic);
        VarInfo->SetStringField("category", Category);

        Result->SetObjectField("variable", VarInfo);
    }
    else
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Failed to create variable");
    }

    return Result;
}

TSharedPtr<FJsonObject> FBPVariables::SetVariableProperties(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    FString BlueprintName = Params->GetStringField(TEXT("blueprint_name"));
    FString VariableName = Params->GetStringField(TEXT("variable_name"));

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);

    if (!Blueprint)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
        return Result;
    }

    // Find the variable in the Blueprint
    FBPVariableDescription* VarDesc = nullptr;
    for (FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        if (Var.VarName == FName(*VariableName))
        {
            VarDesc = &Var;
            break;
        }
    }

    if (!VarDesc)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", FString::Printf(TEXT("Variable not found: %s"), *VariableName));
        return Result;
    }

    // Track which properties were updated
    TSharedPtr<FJsonObject> UpdatedProperties = MakeShared<FJsonObject>();

    // Update var_name (rename variable)
    if (Params->HasField(TEXT("var_name")))
    {
        FString NewVarName = Params->GetStringField(TEXT("var_name"));
        VarDesc->VarName = FName(*NewVarName);
        UpdatedProperties->SetStringField("var_name", NewVarName);
    }

    // Update var_type (change variable type)
    if (Params->HasField(TEXT("var_type")))
    {
        FString TypeString = Params->GetStringField(TEXT("var_type"));
        FEdGraphPinType NewType = GetPinTypeFromString(TypeString);
        VarDesc->VarType = NewType;
        UpdatedProperties->SetStringField("var_type", TypeString);
    }

    // Update is_blueprint_writable (Set node)
    if (Params->HasField(TEXT("is_blueprint_writable")))
    {
        bool bIsWritable = Params->GetBoolField(TEXT("is_blueprint_writable"));
        if (bIsWritable)
        {
            VarDesc->PropertyFlags &= ~CPF_BlueprintReadOnly;
        }
        else
        {
            VarDesc->PropertyFlags |= CPF_BlueprintReadOnly;
        }
        UpdatedProperties->SetBoolField("is_blueprint_writable", bIsWritable);
    }

    // Update is_public
    if (Params->HasField(TEXT("is_public")))
    {
        bool bIsPublic = Params->GetBoolField(TEXT("is_public"));
        if (bIsPublic)
        {
            VarDesc->PropertyFlags |= CPF_Edit;
        }
        else
        {
            VarDesc->PropertyFlags &= ~CPF_Edit;
        }
        UpdatedProperties->SetBoolField("is_public", bIsPublic);
    }

    // Update is_editable_in_instance (opposite of CPF_DisableEditOnInstance)
    if (Params->HasField(TEXT("is_editable_in_instance")))
    {
        bool bIsEditable = Params->GetBoolField(TEXT("is_editable_in_instance"));
        if (bIsEditable)
        {
            VarDesc->PropertyFlags &= ~CPF_DisableEditOnInstance;
        }
        else
        {
            VarDesc->PropertyFlags |= CPF_DisableEditOnInstance;
        }
        UpdatedProperties->SetBoolField("is_editable_in_instance", bIsEditable);
    }

    // Update is_config
    if (Params->HasField(TEXT("is_config")))
    {
        bool bIsConfig = Params->GetBoolField(TEXT("is_config"));
        if (bIsConfig)
        {
            VarDesc->PropertyFlags |= CPF_Config;
        }
        else
        {
            VarDesc->PropertyFlags &= ~CPF_Config;
        }
        UpdatedProperties->SetBoolField("is_config", bIsConfig);
    }

    // Update friendly_name
    if (Params->HasField(TEXT("friendly_name")))
    {
        FString FriendlyName = Params->GetStringField(TEXT("friendly_name"));
        VarDesc->FriendlyName = FriendlyName;
        UpdatedProperties->SetStringField("friendly_name", FriendlyName);
    }

    // Update tooltip
    if (Params->HasField(TEXT("tooltip")))
    {
        FString Tooltip = Params->GetStringField(TEXT("tooltip"));
        VarDesc->SetMetaData(FBlueprintMetadata::MD_Tooltip, *Tooltip);
        UpdatedProperties->SetStringField("tooltip", Tooltip);
    }

    // Update category
    if (Params->HasField(TEXT("category")))
    {
        FString Category = Params->GetStringField(TEXT("category"));
        VarDesc->Category = FText::FromString(Category);
        UpdatedProperties->SetStringField("category", Category);
    }

    // Update replication_enabled (Row 15 - CPF_Net flag)
    if (Params->HasField(TEXT("replication_enabled")))
    {
        bool bReplicationEnabled = Params->GetBoolField(TEXT("replication_enabled"));
        if (bReplicationEnabled)
        {
            VarDesc->PropertyFlags |= CPF_Net;
        }
        else
        {
            VarDesc->PropertyFlags &= ~CPF_Net;
        }
        UpdatedProperties->SetBoolField("replication_enabled", bReplicationEnabled);
    }

    // Update replication_condition (Row 16 - ELifetimeCondition)
    if (Params->HasField(TEXT("replication_condition")))
    {
        int32 ReplicationConditionValue = (int32)Params->GetNumberField(TEXT("replication_condition"));
        VarDesc->ReplicationCondition = (ELifetimeCondition)ReplicationConditionValue;
        UpdatedProperties->SetNumberField("replication_condition", ReplicationConditionValue);
    }

    // Update is_private (Row 7 - MD_AllowPrivateAccess metadata)
    if (Params->HasField(TEXT("is_private")))
    {
        bool bIsPrivate = Params->GetBoolField(TEXT("is_private"));
        if (bIsPrivate)
        {
            VarDesc->SetMetaData(TEXT("AllowPrivateAccess"), TEXT("true"));
        }
        else
        {
            VarDesc->RemoveMetaData(TEXT("AllowPrivateAccess"));
        }
        UpdatedProperties->SetBoolField("is_private", bIsPrivate);
    }

    // Update expose_on_spawn (metadata)
    if (Params->HasField(TEXT("expose_on_spawn")))
    {
        bool bExposeOnSpawn = Params->GetBoolField(TEXT("expose_on_spawn"));
        if (bExposeOnSpawn)
        {
            VarDesc->SetMetaData(TEXT("ExposeOnSpawn"), TEXT("true"));
        }
        else
        {
            VarDesc->RemoveMetaData(TEXT("ExposeOnSpawn"));
        }
        UpdatedProperties->SetBoolField("expose_on_spawn", bExposeOnSpawn);
    }

    // Update default_value
    if (Params->HasField(TEXT("default_value")))
    {
        SetDefaultValue(*VarDesc, Params->Values.FindRef("default_value"));
        UpdatedProperties->SetStringField("default_value", "updated");
    }

    // Update expose_to_cinematics (CPF_Interp)
    if (Params->HasField(TEXT("expose_to_cinematics")))
    {
        bool bExposeToCinematics = Params->GetBoolField(TEXT("expose_to_cinematics"));
        if (bExposeToCinematics)
        {
            VarDesc->PropertyFlags |= CPF_Interp;
        }
        else
        {
            VarDesc->PropertyFlags &= ~CPF_Interp;
        }
        UpdatedProperties->SetBoolField("expose_to_cinematics", bExposeToCinematics);
    }

    // Update slider_range_min (MD_UIMin)
    if (Params->HasField(TEXT("slider_range_min")))
    {
        FString SliderMin = Params->GetStringField(TEXT("slider_range_min"));
        VarDesc->SetMetaData(TEXT("UIMin"), *SliderMin);
        UpdatedProperties->SetStringField("slider_range_min", SliderMin);
    }

    // Update slider_range_max (MD_UIMax)
    if (Params->HasField(TEXT("slider_range_max")))
    {
        FString SliderMax = Params->GetStringField(TEXT("slider_range_max"));
        VarDesc->SetMetaData(TEXT("UIMax"), *SliderMax);
        UpdatedProperties->SetStringField("slider_range_max", SliderMax);
    }

    // Update value_range_min (MD_ClampMin)
    if (Params->HasField(TEXT("value_range_min")))
    {
        FString ClampMin = Params->GetStringField(TEXT("value_range_min"));
        VarDesc->SetMetaData(TEXT("ClampMin"), *ClampMin);
        UpdatedProperties->SetStringField("value_range_min", ClampMin);
    }

    // Update value_range_max (MD_ClampMax)
    if (Params->HasField(TEXT("value_range_max")))
    {
        FString ClampMax = Params->GetStringField(TEXT("value_range_max"));
        VarDesc->SetMetaData(TEXT("ClampMax"), *ClampMax);
        UpdatedProperties->SetStringField("value_range_max", ClampMax);
    }

    // Update units (MD_Units)
    if (Params->HasField(TEXT("units")))
    {
        FString Units = Params->GetStringField(TEXT("units"));
        VarDesc->SetMetaData(TEXT("Units"), *Units);
        UpdatedProperties->SetStringField("units", Units);
    }

    // Update bitmask (MD_Bitmask)
    if (Params->HasField(TEXT("bitmask")))
    {
        bool bIsBitmask = Params->GetBoolField(TEXT("bitmask"));
        if (bIsBitmask)
        {
            VarDesc->SetMetaData(TEXT("Bitmask"), TEXT("true"));
        }
        else
        {
            VarDesc->RemoveMetaData(TEXT("Bitmask"));
        }
        UpdatedProperties->SetBoolField("bitmask", bIsBitmask);
    }

    // Update bitmask_enum (MD_BitmaskEnum)
    if (Params->HasField(TEXT("bitmask_enum")))
    {
        FString BitmaskEnum = Params->GetStringField(TEXT("bitmask_enum"));
        VarDesc->SetMetaData(TEXT("BitmaskEnum"), *BitmaskEnum);
        UpdatedProperties->SetStringField("bitmask_enum", BitmaskEnum);
    }

    // Mark Blueprint as modified and compile
    Blueprint->MarkPackageDirty();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    // Force property editor refresh for metadata changes
    // This ensures Details Panel dropdowns (Units, etc.) synchronize with metadata
    if (GEditor)
    {
        FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
        PropertyModule.NotifyCustomizationModuleChanged();
    }

    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    Result->SetBoolField("success", true);
    Result->SetStringField("variable_name", VariableName);
    Result->SetObjectField("properties_updated", UpdatedProperties);
    Result->SetStringField("message", "Variable properties updated successfully");

    return Result;
}

FEdGraphPinType FBPVariables::GetPinTypeFromString(const FString& TypeString)
{
    FEdGraphPinType PinType;

    if (TypeString == "bool")
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    }
    else if (TypeString == "int")
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    }
    else if (TypeString == "float")
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    }
    else if (TypeString == "string")
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    }
    else if (TypeString == "vector")
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }
    else if (TypeString == "rotator")
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
    }
    else
    {
        // Défaut: float
        PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    }

    return PinType;
}

void FBPVariables::SetDefaultValue(FBPVariableDescription& Variable, const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid())
    {
        return;
    }

    FString StringValue;

    // Convert JSON value to string representation for default value
    if (Value->Type == EJson::String)
    {
        StringValue = Value->AsString();
    }
    else if (Value->Type == EJson::Number)
    {
        StringValue = FString::Printf(TEXT("%g"), Value->AsNumber());
    }
    else if (Value->Type == EJson::Boolean)
    {
        StringValue = Value->AsBool() ? TEXT("true") : TEXT("false");
    }
    else if (Value->Type == EJson::Null)
    {
        StringValue = TEXT("");
    }
    else
    {
        // For complex types, convert to empty string
        StringValue = TEXT("");
    }

    // Update Variable.DefaultValue for Blueprint display
    Variable.DefaultValue = StringValue;
}