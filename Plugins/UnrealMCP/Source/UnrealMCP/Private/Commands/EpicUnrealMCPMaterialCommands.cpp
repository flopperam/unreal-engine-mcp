#include "Commands/EpicUnrealMCPMaterialCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "EngineUtils.h"
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
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Factories/MaterialParameterCollectionFactoryNew.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialInterface.h"
#include "Components/PrimitiveComponent.h"
#include "RenderingThread.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ScopedTransaction.h"
#include "UObject/FieldIterator.h"
#include "Kismet/GameplayStatics.h"

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

UMaterialInstance* LoadMaterialInstance(const FString& InstancePath)
{
    if (InstancePath.IsEmpty())
    {
        return nullptr;
    }

    const FString NormalizedPath = NormalizeMaterialObjectPath(InstancePath);
    UMaterialInstance* Instance = LoadObject<UMaterialInstance>(nullptr, *NormalizedPath);
    if (Instance)
    {
        return Instance;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(NormalizedPath));
    return Cast<UMaterialInstance>(AssetData.GetAsset());
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
        {TEXT("create_material_instance"), &FEpicUnrealMCPMaterialCommands::HandleCreateMaterialInstance},
        {TEXT("create_dynamic_material_instance"), &FEpicUnrealMCPMaterialCommands::HandleCreateDynamicMaterialInstance},
        {TEXT("batch_update_material_parameters"), &FEpicUnrealMCPMaterialCommands::HandleBatchUpdateMaterialParameters},
        {TEXT("set_material_scalar_parameter"), &FEpicUnrealMCPMaterialCommands::HandleSetMaterialScalarParameter},
        {TEXT("set_material_vector_parameter"), &FEpicUnrealMCPMaterialCommands::HandleSetMaterialVectorParameter},
        {TEXT("set_material_texture_parameter"), &FEpicUnrealMCPMaterialCommands::HandleSetMaterialTextureParameter},
        {TEXT("set_material_static_switch_parameter"), &FEpicUnrealMCPMaterialCommands::HandleSetMaterialStaticSwitchParameter},
        {TEXT("create_material_parameter_collection"), &FEpicUnrealMCPMaterialCommands::HandleCreateMaterialParameterCollection},
        {TEXT("edit_material_parameter_collection"), &FEpicUnrealMCPMaterialCommands::HandleEditMaterialParameterCollection},
        {TEXT("create_advanced_material"), &FEpicUnrealMCPMaterialCommands::HandleCreateAdvancedMaterial},
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
        FlushRenderingCommands();
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

    FlushRenderingCommands();
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

    FlushRenderingCommands();
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

// ------------------------------------------------------------------
// Phase 1: Material Instance & Parameter System
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
    FString ParentMaterialPath;
    if (!Params->TryGetStringField(TEXT("parent_material"), ParentMaterialPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parent_material' parameter"));
    }

    FString InstanceName;
    if (!Params->TryGetStringField(TEXT("instance_name"), InstanceName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'instance_name' parameter"));
    }

    FString PackagePath = TEXT("/Game/Materials/");
    Params->TryGetStringField(TEXT("package_path"), PackagePath);
    if (!PackagePath.EndsWith(TEXT("/")))
    {
        PackagePath += TEXT("/");
    }

    FString AssetPath = PackagePath + InstanceName;

    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material instance already exists: %s"), *AssetPath));
    }

    UMaterial* ParentMaterial = LoadMaterial(ParentMaterialPath);
    if (!ParentMaterial)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent material not found: %s"), *ParentMaterialPath));
    }

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Create Material Instance")));

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = ParentMaterial;
    UObject* NewAsset = AssetTools.CreateAsset(InstanceName, PackagePath, UMaterialInstanceConstant::StaticClass(), Factory);

    if (NewAsset)
    {
        UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(NewAsset);
        if (Instance)
        {
            Instance->PreEditChange(nullptr);
            FlushRenderingCommands();
            Instance->PostEditChange();
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("path"), AssetPath);
        Result->SetStringField(TEXT("parent_material"), ParentMaterialPath);
        return Result;
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material instance"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleCreateDynamicMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    int32 MaterialSlot = 0;
    Params->TryGetNumberField(TEXT("material_slot"), MaterialSlot);

    FString SourceMaterialPath;
    Params->TryGetStringField(TEXT("source_material"), SourceMaterialPath);

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active world found"));
    }

    AActor* Actor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            Actor = *It;
            break;
        }
    }

    if (!Actor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UPrimitiveComponent* PrimComp = Actor->FindComponentByClass<UPrimitiveComponent>();
    if (!PrimComp)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Actor has no PrimitiveComponent to apply material"));
    }

    UMaterialInstanceDynamic* MID = nullptr;
    if (!SourceMaterialPath.IsEmpty())
    {
        UMaterial* SourceMaterial = LoadMaterial(SourceMaterialPath);
        if (!SourceMaterial)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source material not found: %s"), *SourceMaterialPath));
        }
        MID = UMaterialInstanceDynamic::Create(SourceMaterial, PrimComp);
    }
    else
    {
        UMaterialInterface* CurrentMaterial = PrimComp->GetMaterial(MaterialSlot);
        if (!CurrentMaterial)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No material found at slot to create dynamic instance from"));
        }
        MID = UMaterialInstanceDynamic::Create(CurrentMaterial, PrimComp);
    }

    if (!MID)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create dynamic material instance"));
    }

    PrimComp->SetMaterial(MaterialSlot, MID);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor_name"), ActorName);
    Result->SetNumberField(TEXT("material_slot"), MaterialSlot);
    Result->SetStringField(TEXT("instance_path"), MID->GetPathName());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::ApplyBatchParameters(
    UMaterialInstance* Instance,
    const TArray<TSharedPtr<FJsonValue>>& Parameters)
{
    if (!Instance)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid material instance"));
    }

    int32 UpdatedCount = 0;
    TArray<FString> Errors;

    for (const TSharedPtr<FJsonValue>& ParamValue : Parameters)
    {
        if (!ParamValue.IsValid())
        {
            continue;
        }

        const TSharedPtr<FJsonObject>* ParamObj = nullptr;
        if (!ParamValue->TryGetObject(ParamObj) || !ParamObj->IsValid())
        {
            Errors.Add(TEXT("Invalid parameter entry (not an object)"));
            continue;
        }

        FString ParamName;
        if (!(*ParamObj)->TryGetStringField(TEXT("name"), ParamName) || ParamName.IsEmpty())
        {
            Errors.Add(TEXT("Parameter missing 'name' field"));
            continue;
        }

        FString ParamType;
        if (!(*ParamObj)->TryGetStringField(TEXT("type"), ParamType) || ParamType.IsEmpty())
        {
            Errors.Add(FString::Printf(TEXT("Parameter '%s' missing 'type' field"), *ParamName));
            continue;
        }

        const FName ParamFName(*ParamName);

        UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Instance);
        if (!MIC)
        {
            Errors.Add(TEXT("Instance is not a Material Instance Constant"));
            continue;
        }

        if (ParamType.Equals(TEXT("scalar"), ESearchCase::IgnoreCase))
        {
            double Value = 0.0;
            if ((*ParamObj)->TryGetNumberField(TEXT("value"), Value))
            {
                MIC->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ParamFName), static_cast<float>(Value));
                ++UpdatedCount;
            }
            else
            {
                Errors.Add(FString::Printf(TEXT("Scalar parameter '%s' missing numeric 'value'"), *ParamName));
            }
        }
        else if (ParamType.Equals(TEXT("vector"), ESearchCase::IgnoreCase))
        {
            const TArray<TSharedPtr<FJsonValue>>* ColorArray = nullptr;
            if ((*ParamObj)->TryGetArrayField(TEXT("value"), ColorArray) && ColorArray->Num() >= 3)
            {
                const float R = static_cast<float>((*ColorArray)[0]->AsNumber());
                const float G = static_cast<float>((*ColorArray)[1]->AsNumber());
                const float B = static_cast<float>((*ColorArray)[2]->AsNumber());
                const float A = ColorArray->Num() >= 4 ? static_cast<float>((*ColorArray)[3]->AsNumber()) : 1.0f;
                MIC->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(ParamFName), FLinearColor(R, G, B, A));
                ++UpdatedCount;
            }
            else
            {
                Errors.Add(FString::Printf(TEXT("Vector parameter '%s' missing [R,G,B,A] array 'value'"), *ParamName));
            }
        }
        else if (ParamType.Equals(TEXT("texture"), ESearchCase::IgnoreCase))
        {
            FString TexturePath;
            if ((*ParamObj)->TryGetStringField(TEXT("value"), TexturePath))
            {
                UTexture* Texture = LoadObject<UTexture>(nullptr, *NormalizeMaterialObjectPath(TexturePath));
                if (!Texture)
                {
                    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
                    const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(NormalizeMaterialObjectPath(TexturePath)));
                    Texture = Cast<UTexture>(AssetData.GetAsset());
                }

                if (Texture)
                {
                    MIC->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(ParamFName), Texture);
                    ++UpdatedCount;
                }
                else
                {
                    Errors.Add(FString::Printf(TEXT("Texture parameter '%s': texture not found '%s'"), *ParamName, *TexturePath));
                }
            }
            else
            {
                Errors.Add(FString::Printf(TEXT("Texture parameter '%s' missing string 'value' (texture path)"), *ParamName));
            }
        }
        else if (ParamType.Equals(TEXT("static_switch"), ESearchCase::IgnoreCase))
        {
            bool bValue = false;
            if ((*ParamObj)->TryGetBoolField(TEXT("value"), bValue))
            {
                Instance->SetStaticSwitchParameterValueEditorOnly(ParamFName, bValue);
                ++UpdatedCount;
            }
            else
            {
                Errors.Add(FString::Printf(TEXT("Static switch parameter '%s' missing boolean 'value'"), *ParamName));
            }
        }
        else
        {
            Errors.Add(FString::Printf(TEXT("Unknown parameter type '%s' for '%s'"), *ParamType, *ParamName));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), Errors.IsEmpty());
    Result->SetNumberField(TEXT("updated_count"), UpdatedCount);
    if (!Errors.IsEmpty())
    {
        Result->SetStringField(TEXT("error"), FString::Join(Errors, TEXT("; ")));
    }
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleBatchUpdateMaterialParameters(const TSharedPtr<FJsonObject>& Params)
{
    FString InstancePath;
    if (!Params->TryGetStringField(TEXT("instance_path"), InstancePath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'instance_path' parameter"));
    }

    UMaterialInstance* Instance = LoadMaterialInstance(InstancePath);
    if (!Instance)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material instance not found: %s"), *InstancePath));
    }

    const TArray<TSharedPtr<FJsonValue>>* Parameters = nullptr;
    if (!Params->TryGetArrayField(TEXT("parameters"), Parameters))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameters' array"));
    }

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Batch Update Material Parameters")));
    Instance->Modify();

    TSharedPtr<FJsonObject> Result = ApplyBatchParameters(Instance, *Parameters);

    if (Result->GetBoolField(TEXT("success")))
    {
        FlushRenderingCommands();
        Instance->PostEditChange();
    }

    Result->SetStringField(TEXT("instance_path"), InstancePath);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleSetMaterialScalarParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString InstancePath;
    if (!Params->TryGetStringField(TEXT("instance_path"), InstancePath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'instance_path' parameter"));
    }

    FString ParamName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParamName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }

    double Value = 0.0;
    if (!Params->TryGetNumberField(TEXT("value"), Value))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
    }

    UMaterialInstance* Instance = LoadMaterialInstance(InstancePath);
    if (!Instance)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material instance not found: %s"), *InstancePath));
    }

    UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Instance);
    if (!MIC)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Instance is not a Material Instance Constant"));
    }

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Scalar Parameter")));
    MIC->Modify();
    MIC->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(FName(*ParamName)), static_cast<float>(Value));
    FlushRenderingCommands();
    MIC->PostEditChange();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("instance_path"), InstancePath);
    Result->SetStringField(TEXT("parameter_name"), ParamName);
    Result->SetNumberField(TEXT("value"), Value);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleSetMaterialVectorParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString InstancePath;
    if (!Params->TryGetStringField(TEXT("instance_path"), InstancePath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'instance_path' parameter"));
    }

    FString ParamName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParamName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }

    const TArray<TSharedPtr<FJsonValue>>* ValueArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("value"), ValueArray) || ValueArray->Num() < 3)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' array with at least [R,G,B]"));
    }

    UMaterialInstance* Instance = LoadMaterialInstance(InstancePath);
    if (!Instance)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material instance not found: %s"), *InstancePath));
    }

    UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Instance);
    if (!MIC)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Instance is not a Material Instance Constant"));
    }

    const float R = static_cast<float>((*ValueArray)[0]->AsNumber());
    const float G = static_cast<float>((*ValueArray)[1]->AsNumber());
    const float B = static_cast<float>((*ValueArray)[2]->AsNumber());
    const float A = ValueArray->Num() >= 4 ? static_cast<float>((*ValueArray)[3]->AsNumber()) : 1.0f;

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Vector Parameter")));
    MIC->Modify();
    MIC->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(FName(*ParamName)), FLinearColor(R, G, B, A));
    FlushRenderingCommands();
    MIC->PostEditChange();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("instance_path"), InstancePath);
    Result->SetStringField(TEXT("parameter_name"), ParamName);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleSetMaterialTextureParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString InstancePath;
    if (!Params->TryGetStringField(TEXT("instance_path"), InstancePath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'instance_path' parameter"));
    }

    FString ParamName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParamName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }

    FString TexturePath;
    if (!Params->TryGetStringField(TEXT("texture_path"), TexturePath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'texture_path' parameter"));
    }

    UMaterialInstance* Instance = LoadMaterialInstance(InstancePath);
    if (!Instance)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material instance not found: %s"), *InstancePath));
    }

    UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Instance);
    if (!MIC)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Instance is not a Material Instance Constant"));
    }

    UTexture* Texture = LoadObject<UTexture>(nullptr, *NormalizeMaterialObjectPath(TexturePath));
    if (!Texture)
    {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(NormalizeMaterialObjectPath(TexturePath)));
        Texture = Cast<UTexture>(AssetData.GetAsset());
    }

    if (!Texture)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Texture not found: %s"), *TexturePath));
    }

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Texture Parameter")));
    MIC->Modify();
    MIC->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(FName(*ParamName)), Texture);
    FlushRenderingCommands();
    MIC->PostEditChange();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("instance_path"), InstancePath);
    Result->SetStringField(TEXT("parameter_name"), ParamName);
    Result->SetStringField(TEXT("texture_path"), TexturePath);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleSetMaterialStaticSwitchParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString InstancePath;
    if (!Params->TryGetStringField(TEXT("instance_path"), InstancePath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'instance_path' parameter"));
    }

    FString ParamName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParamName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }

    bool bValue = false;
    if (!Params->TryGetBoolField(TEXT("value"), bValue))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter (boolean)"));
    }

    UMaterialInstance* Instance = LoadMaterialInstance(InstancePath);
    if (!Instance)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material instance not found: %s"), *InstancePath));
    }

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Static Switch Parameter")));
    Instance->Modify();
    Instance->SetStaticSwitchParameterValueEditorOnly(FName(*ParamName), bValue);
    FlushRenderingCommands();
    Instance->PostEditChange();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("instance_path"), InstancePath);
    Result->SetStringField(TEXT("parameter_name"), ParamName);
    Result->SetBoolField(TEXT("value"), bValue);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleCreateMaterialParameterCollection(const TSharedPtr<FJsonObject>& Params)
{
    FString CollectionName;
    if (!Params->TryGetStringField(TEXT("name"), CollectionName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString PackagePath = TEXT("/Game/Materials/");
    Params->TryGetStringField(TEXT("package_path"), PackagePath);
    if (!PackagePath.EndsWith(TEXT("/")))
    {
        PackagePath += TEXT("/");
    }

    FString AssetPath = PackagePath + CollectionName;

    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material parameter collection already exists: %s"), *AssetPath));
    }

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Create Material Parameter Collection")));

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    UMaterialParameterCollectionFactoryNew* Factory = NewObject<UMaterialParameterCollectionFactoryNew>();
    UObject* NewAsset = AssetTools.CreateAsset(CollectionName, PackagePath, UMaterialParameterCollection::StaticClass(), Factory);

    if (NewAsset)
    {
        UMaterialParameterCollection* Collection = Cast<UMaterialParameterCollection>(NewAsset);
        if (Collection)
        {
            Collection->PreEditChange(nullptr);
            FlushRenderingCommands();
            Collection->PostEditChange();
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("path"), AssetPath);
        return Result;
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material parameter collection"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleEditMaterialParameterCollection(const TSharedPtr<FJsonObject>& Params)
{
    FString CollectionPath;
    if (!Params->TryGetStringField(TEXT("collection_path"), CollectionPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'collection_path' parameter"));
    }

    UMaterialParameterCollection* Collection = LoadObject<UMaterialParameterCollection>(nullptr, *NormalizeMaterialObjectPath(CollectionPath));
    if (!Collection)
    {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(NormalizeMaterialObjectPath(CollectionPath)));
        Collection = Cast<UMaterialParameterCollection>(AssetData.GetAsset());
    }

    if (!Collection)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material parameter collection not found: %s"), *CollectionPath));
    }

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Edit Material Parameter Collection")));
    Collection->Modify();

    int32 AddedScalars = 0;
    int32 AddedVectors = 0;
    int32 RemovedCount = 0;
    TArray<FString> Errors;

    const TArray<TSharedPtr<FJsonValue>>* AddScalars = nullptr;
    if (Params->TryGetArrayField(TEXT("add_scalars"), AddScalars))
    {
        for (const TSharedPtr<FJsonValue>& Val : *AddScalars)
        {
            FString ParamName = Val->AsString();
            if (!ParamName.IsEmpty())
            {
                FCollectionScalarParameter NewParam;
                NewParam.ParameterName = FName(*ParamName);
                NewParam.DefaultValue = 0.0f;
                Collection->ScalarParameters.Add(NewParam);
                ++AddedScalars;
            }
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* AddVectors = nullptr;
    if (Params->TryGetArrayField(TEXT("add_vectors"), AddVectors))
    {
        for (const TSharedPtr<FJsonValue>& Val : *AddVectors)
        {
            FString ParamName = Val->AsString();
            if (!ParamName.IsEmpty())
            {
                FCollectionVectorParameter NewParam;
                NewParam.ParameterName = FName(*ParamName);
                NewParam.DefaultValue = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
                Collection->VectorParameters.Add(NewParam);
                ++AddedVectors;
            }
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* RemoveParams = nullptr;
    if (Params->TryGetArrayField(TEXT("remove_params"), RemoveParams))
    {
        for (const TSharedPtr<FJsonValue>& Val : *RemoveParams)
        {
            FString ParamName = Val->AsString();
            if (!ParamName.IsEmpty())
            {
                const FName ParamFName(*ParamName);
                int32 RemovedFromScalars = Collection->ScalarParameters.RemoveAll(
                    [&ParamFName](const FCollectionScalarParameter& P) { return P.ParameterName == ParamFName; });
                int32 RemovedFromVectors = Collection->VectorParameters.RemoveAll(
                    [&ParamFName](const FCollectionVectorParameter& P) { return P.ParameterName == ParamFName; });
                if (RemovedFromScalars + RemovedFromVectors > 0)
                {
                    RemovedCount += RemovedFromScalars + RemovedFromVectors;
                }
                else
                {
                    Errors.Add(FString::Printf(TEXT("Parameter '%s' not found for removal"), *ParamName));
                }
            }
        }
    }

    FlushRenderingCommands();
    Collection->PostEditChange();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("collection_path"), CollectionPath);
    Result->SetNumberField(TEXT("added_scalars"), AddedScalars);
    Result->SetNumberField(TEXT("added_vectors"), AddedVectors);
    Result->SetNumberField(TEXT("removed_count"), RemovedCount);
    if (!Errors.IsEmpty())
    {
        Result->SetStringField(TEXT("warnings"), FString::Join(Errors, TEXT("; ")));
    }
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleCreateAdvancedMaterial(const TSharedPtr<FJsonObject>& Params)
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

    FString MaterialDomain = TEXT("Surface");
    Params->TryGetStringField(TEXT("material_domain"), MaterialDomain);

    FString AssetPath = PackagePath + MaterialName;

    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material already exists: %s"), *AssetPath));
    }

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Create Advanced Material")));

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UObject* NewAsset = AssetTools.CreateAsset(MaterialName, PackagePath, UMaterial::StaticClass(), Factory);

    if (!NewAsset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material"));
    }

    UMaterial* Material = Cast<UMaterial>(NewAsset);
    if (!Material)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Created asset is not a material"));
    }

    Material->Modify();

    if (MaterialDomain.Equals(TEXT("Surface"), ESearchCase::IgnoreCase))
    {
        Material->MaterialDomain = MD_Surface;
    }
    else if (MaterialDomain.Equals(TEXT("DeferredDecal"), ESearchCase::IgnoreCase))
    {
        Material->MaterialDomain = MD_DeferredDecal;
    }
    else if (MaterialDomain.Equals(TEXT("LightFunction"), ESearchCase::IgnoreCase))
    {
        Material->MaterialDomain = MD_LightFunction;
    }
    else if (MaterialDomain.Equals(TEXT("PostProcess"), ESearchCase::IgnoreCase))
    {
        Material->MaterialDomain = MD_PostProcess;
        Material->BlendableLocation = BL_SceneColorAfterTonemapping;
    }
    else if (MaterialDomain.Equals(TEXT("VirtualTexture"), ESearchCase::IgnoreCase))
    {
        Material->MaterialDomain = MD_RuntimeVirtualTexture;
    }
    else if (MaterialDomain.Equals(TEXT("Landscape"), ESearchCase::IgnoreCase))
    {
        Material->MaterialDomain = MD_Surface;
    }
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown material domain: %s. Valid: Surface, DeferredDecal, LightFunction, PostProcess, VirtualTexture, Landscape"), *MaterialDomain));
    }

    FlushRenderingCommands();
    Material->PostEditChange();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("path"), AssetPath);
    Result->SetStringField(TEXT("material_domain"), MaterialDomain);
    return Result;
}
