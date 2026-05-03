#include "Commands/EpicUnrealMCPMaterialCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Engine/Texture.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ScopedTransaction.h"
#include "UObject/FieldIterator.h"

namespace
{
FString NormalizeMaterialObjectPath(const FString& MaterialPath)
{
    if (MaterialPath.Contains(TEXT(".")))
    {
        return MaterialPath;
    }

    const FString AssetName = FPaths::GetBaseFilename(MaterialPath);
    return FString::Printf(TEXT("%s.%s"), *MaterialPath, *AssetName);
}

UMaterial* LoadMaterial(const FString& MaterialPath)
{
    if (MaterialPath.IsEmpty())
    {
        return nullptr;
    }

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *NormalizeMaterialObjectPath(MaterialPath));
    if (Material)
    {
        return Material;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(NormalizeMaterialObjectPath(MaterialPath)));
    return Cast<UMaterial>(AssetData.GetAsset());
}

UMaterialExpression* FindMaterialExpression(UMaterial* Material, const FString& NodeId)
{
    if (!Material)
    {
        return nullptr;
    }

    for (UMaterialExpression* Expr : Material->GetExpressions())
    {
        if (Expr && Expr->GetName() == NodeId)
        {
            return Expr;
        }
    }

    return nullptr;
}

int32 FindOutputIndex(UMaterialExpression* Expression, const FString& PinName)
{
    if (!Expression || PinName.IsEmpty())
    {
        return 0;
    }

    const TArray<FExpressionOutput> Outputs = Expression->GetOutputs();
    for (int32 Index = 0; Index < Outputs.Num(); ++Index)
    {
        const FString OutputName = Outputs[Index].OutputName.ToString();
        if (OutputName.Equals(PinName, ESearchCase::IgnoreCase))
        {
            return Index;
        }
    }

    if (PinName.Equals(TEXT("RGB"), ESearchCase::IgnoreCase) ||
        PinName.Equals(TEXT("Result"), ESearchCase::IgnoreCase) ||
        PinName.Equals(TEXT("Value"), ESearchCase::IgnoreCase))
    {
        return 0;
    }

    return INDEX_NONE;
}

FString GetOutputName(UMaterialExpression* Expression, int32 OutputIndex)
{
    if (!Expression)
    {
        return TEXT("");
    }

    const TArray<FExpressionOutput> Outputs = Expression->GetOutputs();
    if (Outputs.IsValidIndex(OutputIndex))
    {
        return Outputs[OutputIndex].OutputName.ToString();
    }

    return TEXT("");
}

FExpressionInput* FindExpressionInput(UObject* Object, const FString& PinName)
{
    if (!Object || PinName.IsEmpty())
    {
        return nullptr;
    }

    for (TFieldIterator<FStructProperty> It(Object->GetClass()); It; ++It)
    {
        FStructProperty* StructProperty = *It;
        if (StructProperty->Struct &&
            StructProperty->Struct->GetName() == TEXT("ExpressionInput") &&
            StructProperty->GetName().Equals(PinName, ESearchCase::IgnoreCase))
        {
            return StructProperty->ContainerPtrToValuePtr<FExpressionInput>(Object);
        }
    }

    return nullptr;
}

FExpressionInput* FindMaterialRootInput(UMaterial* Material, const FString& PinName)
{
    if (!Material)
    {
        return nullptr;
    }

    if (PinName.Equals(TEXT("BaseColor"), ESearchCase::IgnoreCase)) return Material->GetExpressionInputForProperty(MP_BaseColor);
    if (PinName.Equals(TEXT("Metallic"), ESearchCase::IgnoreCase)) return Material->GetExpressionInputForProperty(MP_Metallic);
    if (PinName.Equals(TEXT("Specular"), ESearchCase::IgnoreCase)) return Material->GetExpressionInputForProperty(MP_Specular);
    if (PinName.Equals(TEXT("Roughness"), ESearchCase::IgnoreCase)) return Material->GetExpressionInputForProperty(MP_Roughness);
    if (PinName.Equals(TEXT("EmissiveColor"), ESearchCase::IgnoreCase)) return Material->GetExpressionInputForProperty(MP_EmissiveColor);
    if (PinName.Equals(TEXT("Opacity"), ESearchCase::IgnoreCase)) return Material->GetExpressionInputForProperty(MP_Opacity);
    if (PinName.Equals(TEXT("OpacityMask"), ESearchCase::IgnoreCase)) return Material->GetExpressionInputForProperty(MP_OpacityMask);
    if (PinName.Equals(TEXT("Normal"), ESearchCase::IgnoreCase)) return Material->GetExpressionInputForProperty(MP_Normal);
    if (PinName.Equals(TEXT("WorldPositionOffset"), ESearchCase::IgnoreCase)) return Material->GetExpressionInputForProperty(MP_WorldPositionOffset);
    if (PinName.Equals(TEXT("AmbientOcclusion"), ESearchCase::IgnoreCase)) return Material->GetExpressionInputForProperty(MP_AmbientOcclusion);

    return nullptr;
}

void ApplyNodeParams(UMaterialExpression* Expression, const TSharedPtr<FJsonObject>& NodeParams)
{
    if (!Expression || !NodeParams.IsValid())
    {
        return;
    }

    if (UMaterialExpressionConstant* Constant = Cast<UMaterialExpressionConstant>(Expression))
    {
        double Value = 0.0;
        if (NodeParams->TryGetNumberField(TEXT("value"), Value))
        {
            Constant->R = static_cast<float>(Value);
        }
    }
    else if (UMaterialExpressionVectorParameter* VectorParameter = Cast<UMaterialExpressionVectorParameter>(Expression))
    {
        FString ParameterName;
        if (NodeParams->TryGetStringField(TEXT("parameter_name"), ParameterName))
        {
            VectorParameter->ParameterName = FName(*ParameterName);
        }

        const TArray<TSharedPtr<FJsonValue>>* ColorArray = nullptr;
        if (NodeParams->TryGetArrayField(TEXT("default_value"), ColorArray) && ColorArray->Num() >= 3)
        {
            const float R = static_cast<float>((*ColorArray)[0]->AsNumber());
            const float G = static_cast<float>((*ColorArray)[1]->AsNumber());
            const float B = static_cast<float>((*ColorArray)[2]->AsNumber());
            const float A = ColorArray->Num() >= 4 ? static_cast<float>((*ColorArray)[3]->AsNumber()) : 1.0f;
            VectorParameter->DefaultValue = FLinearColor(R, G, B, A);
        }
    }
    else if (UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(Expression))
    {
        FString TexturePath;
        if (NodeParams->TryGetStringField(TEXT("texture"), TexturePath))
        {
            TextureSample->Texture = LoadObject<UTexture>(nullptr, *NormalizeMaterialObjectPath(TexturePath));
        }
    }
}
}

FEpicUnrealMCPMaterialCommands::FEpicUnrealMCPMaterialCommands()
{
}

FEpicUnrealMCPMaterialCommands::~FEpicUnrealMCPMaterialCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPMaterialCommands::*)(const TSharedPtr<FJsonObject>&);
    static const TMap<FString, Handler> Dispatch = {
        {TEXT("analyze_material_graph"), &FEpicUnrealMCPMaterialCommands::HandleAnalyzeMaterialGraph},
        {TEXT("add_material_node"), &FEpicUnrealMCPMaterialCommands::HandleAddMaterialNode},
        {TEXT("connect_material_nodes"), &FEpicUnrealMCPMaterialCommands::HandleConnectMaterialNodes},
        {TEXT("create_material"), &FEpicUnrealMCPMaterialCommands::HandleCreateMaterial},
    };

    const Handler* H = Dispatch.Find(CommandType);
    if (H)
    {
        return (this->*(*H))(Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown material command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("name"), MaterialName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString PackagePath = TEXT("/Game/Materials/");
    Params->TryGetStringField(TEXT("package_path"), PackagePath);
    if (!PackagePath.EndsWith(TEXT("/")))
    {
        PackagePath += TEXT("/");
    }

    FString AssetPath = PackagePath + MaterialName;

    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material already exists: %s"), *AssetPath));
    }

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Create Material")));

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UObject* NewAsset = AssetTools.CreateAsset(MaterialName, PackagePath, UMaterial::StaticClass(), Factory);

    if (NewAsset)
    {
        UMaterial* Material = Cast<UMaterial>(NewAsset);
        Material->PreEditChange(nullptr);
        Material->PostEditChange();
        
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("path"), AssetPath);
        return Result;
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleAddMaterialNode(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    FString NodeType;
    if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_type' parameter"));
    }

    UMaterial* Material = LoadMaterial(MaterialPath);
    if (!Material)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Add Material Node")));
    Material->Modify();

    UClass* ExpressionClass = nullptr;
    if (NodeType.Equals(TEXT("Add"), ESearchCase::IgnoreCase)) ExpressionClass = UMaterialExpressionAdd::StaticClass();
    else if (NodeType.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase)) ExpressionClass = UMaterialExpressionMultiply::StaticClass();
    else if (NodeType.Equals(TEXT("Constant"), ESearchCase::IgnoreCase)) ExpressionClass = UMaterialExpressionConstant::StaticClass();
    else if (NodeType.Equals(TEXT("VectorParameter"), ESearchCase::IgnoreCase)) ExpressionClass = UMaterialExpressionVectorParameter::StaticClass();
    else if (NodeType.Equals(TEXT("TextureSample"), ESearchCase::IgnoreCase)) ExpressionClass = UMaterialExpressionTextureSample::StaticClass();
    else
    {
        // Try to find class by name
        FString FullClassName = FString::Printf(TEXT("MaterialExpression%s"), *NodeType);
        ExpressionClass = UClass::TryFindTypeSlow<UClass>(*FullClassName);
    }

    if (!ExpressionClass)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown node type: %s"), *NodeType));
    }

    UMaterialExpression* NewExpression = NewObject<UMaterialExpression>(Material, ExpressionClass);
    Material->GetExpressionCollection().AddExpression(NewExpression);

    // Set position
    double PosX = 0, PosY = 0;
    Params->TryGetNumberField(TEXT("pos_x"), PosX);
    Params->TryGetNumberField(TEXT("pos_y"), PosY);
    NewExpression->MaterialExpressionEditorX = static_cast<int32>(PosX);
    NewExpression->MaterialExpressionEditorY = static_cast<int32>(PosY);

    const TSharedPtr<FJsonObject>* NodeParams = nullptr;
    if (Params->TryGetObjectField(TEXT("node_params"), NodeParams))
    {
        ApplyNodeParams(NewExpression, *NodeParams);
    }

    Material->PostEditChange();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("node_id"), NewExpression->GetName());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleConnectMaterialNodes(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    UMaterial* Material = LoadMaterial(MaterialPath);
    if (!Material)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    FString FromNodeId, FromPin, ToNodeId, ToPin;
    Params->TryGetStringField(TEXT("source_node_id"), FromNodeId);
    Params->TryGetStringField(TEXT("source_pin_name"), FromPin);
    Params->TryGetStringField(TEXT("target_node_id"), ToNodeId);
    Params->TryGetStringField(TEXT("target_pin_name"), ToPin);

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Connect Material Nodes")));
    Material->Modify();

    UMaterialExpression* FromExpr = FindMaterialExpression(Material, FromNodeId);
    if (!FromExpr)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source node not found in material: %s"), *FromNodeId));
    }

    const int32 OutputIndex = FindOutputIndex(FromExpr, FromPin);
    if (OutputIndex == INDEX_NONE)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source pin not found on node '%s': %s"), *FromNodeId, *FromPin));
    }

    FExpressionInput* TargetInput = nullptr;
    FString TargetDescription = ToNodeId;

    if (FExpressionInput* RootInput = FindMaterialRootInput(Material, ToNodeId))
    {
        TargetInput = RootInput;
        TargetDescription = ToNodeId;
    }
    else if (ToNodeId.Equals(TEXT("Material"), ESearchCase::IgnoreCase) ||
             ToNodeId.Equals(TEXT("Result"), ESearchCase::IgnoreCase) ||
             ToNodeId.Equals(TEXT("Root"), ESearchCase::IgnoreCase))
    {
        TargetInput = FindMaterialRootInput(Material, ToPin);
        TargetDescription = ToPin;
    }
    else if (UMaterialExpression* ToExpr = FindMaterialExpression(Material, ToNodeId))
    {
        TargetInput = FindExpressionInput(ToExpr, ToPin);
        TargetDescription = FString::Printf(TEXT("%s.%s"), *ToNodeId, *ToPin);
    }

    if (!TargetInput)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target pin not found: %s"), *TargetDescription));
    }

    TargetInput->Connect(OutputIndex, FromExpr);
    
    Material->PostEditChange();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("source_node_id"), FromNodeId);
    Result->SetStringField(TEXT("target"), TargetDescription);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleAnalyzeMaterialGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    UMaterial* Material = LoadMaterial(MaterialPath);
    if (!Material)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    TSharedPtr<FJsonObject> GraphData = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> NodeArray;
    TArray<TSharedPtr<FJsonValue>> ConnectionArray;

    for (UMaterialExpression* Expr : Material->GetExpressions())
    {
        if (!Expr) continue;

        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
        NodeObj->SetStringField(TEXT("name"), Expr->GetName());
        NodeObj->SetStringField(TEXT("id"), Expr->GetName());
        NodeObj->SetStringField(TEXT("class"), Expr->GetClass()->GetName());
        NodeObj->SetNumberField(TEXT("pos_x"), Expr->MaterialExpressionEditorX);
        NodeObj->SetNumberField(TEXT("pos_y"), Expr->MaterialExpressionEditorY);
        
        NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));

        for (TFieldIterator<FStructProperty> It(Expr->GetClass()); It; ++It)
        {
            FStructProperty* StructProperty = *It;
            if (!StructProperty->Struct || StructProperty->Struct->GetName() != TEXT("ExpressionInput"))
            {
                continue;
            }

            FExpressionInput* Input = StructProperty->ContainerPtrToValuePtr<FExpressionInput>(Expr);
            if (!Input || !Input->Expression)
            {
                continue;
            }

            TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
            ConnObj->SetStringField(TEXT("source_id"), Input->Expression->GetName());
            ConnObj->SetStringField(TEXT("source_pin"), GetOutputName(Input->Expression, Input->OutputIndex));
            ConnObj->SetStringField(TEXT("target_id"), Expr->GetName());
            ConnObj->SetStringField(TEXT("target_pin"), StructProperty->GetName());
            ConnectionArray.Add(MakeShared<FJsonValueObject>(ConnObj));
        }
    }

    const TPair<FString, EMaterialProperty> RootInputs[] = {
        TPair<FString, EMaterialProperty>(TEXT("BaseColor"), MP_BaseColor),
        TPair<FString, EMaterialProperty>(TEXT("Metallic"), MP_Metallic),
        TPair<FString, EMaterialProperty>(TEXT("Specular"), MP_Specular),
        TPair<FString, EMaterialProperty>(TEXT("Roughness"), MP_Roughness),
        TPair<FString, EMaterialProperty>(TEXT("EmissiveColor"), MP_EmissiveColor),
        TPair<FString, EMaterialProperty>(TEXT("Opacity"), MP_Opacity),
        TPair<FString, EMaterialProperty>(TEXT("OpacityMask"), MP_OpacityMask),
        TPair<FString, EMaterialProperty>(TEXT("Normal"), MP_Normal),
        TPair<FString, EMaterialProperty>(TEXT("WorldPositionOffset"), MP_WorldPositionOffset),
        TPair<FString, EMaterialProperty>(TEXT("AmbientOcclusion"), MP_AmbientOcclusion),
    };

    for (const TPair<FString, EMaterialProperty>& RootInput : RootInputs)
    {
        FExpressionInput* RootExpressionInput = Material->GetExpressionInputForProperty(RootInput.Value);
        if (!RootExpressionInput || !RootExpressionInput->Expression)
        {
            continue;
        }

        TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
        ConnObj->SetStringField(TEXT("source_id"), RootExpressionInput->Expression->GetName());
        ConnObj->SetStringField(TEXT("source_pin"), GetOutputName(RootExpressionInput->Expression, RootExpressionInput->OutputIndex));
        ConnObj->SetStringField(TEXT("target_id"), TEXT("Material"));
        ConnObj->SetStringField(TEXT("target_pin"), RootInput.Key);
        ConnectionArray.Add(MakeShared<FJsonValueObject>(ConnObj));
    }

    GraphData->SetArrayField(TEXT("nodes"), NodeArray);
    GraphData->SetArrayField(TEXT("connections"), ConnectionArray);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetObjectField(TEXT("graph_data"), GraphData);
    return Result;
}
