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