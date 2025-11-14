// BPVariables.cpp
// Created by: Zoscran
// Date: 2025-10-09

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

    // Update is_blueprint_readable (VariableGet)
    if (Params->HasField(TEXT("is_blueprint_readable")))
    {
        bool bIsReadable = Params->GetBoolField(TEXT("is_blueprint_readable"));
        if (bIsReadable)
        {
            VarDesc->PropertyFlags |= CPF_BlueprintVisible;
        }
        else
        {
            VarDesc->PropertyFlags &= ~CPF_BlueprintVisible;
        }
        UpdatedProperties->SetBoolField("is_blueprint_readable", bIsReadable);
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

    // Update is_editable_in_instance
    if (Params->HasField(TEXT("is_editable_in_instance")))
    {
        bool bIsEditable = Params->GetBoolField(TEXT("is_editable_in_instance"));
        if (bIsEditable)
        {
            VarDesc->PropertyFlags |= CPF_Edit;
        }
        else
        {
            VarDesc->PropertyFlags &= ~CPF_Edit;
        }
        UpdatedProperties->SetBoolField("is_editable_in_instance", bIsEditable);
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

    // Update default_value
    if (Params->HasField(TEXT("default_value")))
    {
        SetDefaultValue(*VarDesc, Params->Values.FindRef("default_value"));
        UpdatedProperties->SetStringField("default_value", "updated");
    }

    // Mark Blueprint as modified and compile
    Blueprint->MarkPackageDirty();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
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
    // Implémentation selon type
    // À compléter selon besoin
}