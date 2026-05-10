#include "Commands/EpicUnrealMCPUMGCommands.h"

#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Animation/WidgetAnimation.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

namespace
{
TMap<FString, TWeakObjectPtr<UUserWidget>> GUMGRuntimeWidgetInstances;

TArray<TSharedPtr<FJsonValue>> MakeNumberArray(std::initializer_list<double> Values)
{
    TArray<TSharedPtr<FJsonValue>> Array;
    for (double Value : Values)
    {
        Array.Add(MakeShared<FJsonValueNumber>(Value));
    }
    return Array;
}

TSharedPtr<FJsonObject> MakeSuccessResponse()
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    return Response;
}

FString NormalizeWidgetBlueprintPath(const FString& RawPath)
{
    FString Path = RawPath;
    Path.TrimStartAndEndInline();
    if (Path.IsEmpty())
    {
        return TEXT("");
    }

    Path = FPackageName::ObjectPathToPackageName(Path);
    if (Path.StartsWith(TEXT("/")))
    {
        return Path;
    }

    return FString::Printf(TEXT("/Game/UI/%s"), *FPaths::GetBaseFilename(Path));
}

bool TryGetWidgetBlueprintPath(const TSharedPtr<FJsonObject>& Params, FString& OutPath, FString& OutError)
{
    FString RawPath;
    if (!Params->TryGetStringField(TEXT("widget_blueprint"), RawPath))
    {
        Params->TryGetStringField(TEXT("blueprint_path"), RawPath);
    }
    if (RawPath.IsEmpty())
    {
        Params->TryGetStringField(TEXT("asset_path"), RawPath);
    }
    if (RawPath.IsEmpty())
    {
        OutError = TEXT("widget_blueprint, blueprint_path, or asset_path is required");
        return false;
    }

    OutPath = NormalizeWidgetBlueprintPath(RawPath);
    return true;
}

UWidgetBlueprint* LoadWidgetBlueprintAsset(const FString& RawPath, FString& OutError)
{
    const FString PackagePath = NormalizeWidgetBlueprintPath(RawPath);
    if (PackagePath.IsEmpty())
    {
        OutError = TEXT("Widget Blueprint path is empty");
        return nullptr;
    }

    UObject* Asset = UEditorAssetLibrary::LoadAsset(PackagePath);
    if (!Asset)
    {
        const FString AssetName = FPackageName::GetShortName(PackagePath);
        Asset = LoadObject<UObject>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackagePath, *AssetName));
    }

    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Asset);
    if (!WidgetBlueprint && !RawPath.StartsWith(TEXT("/")))
    {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        FARFilter Filter;
        Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
        Filter.PackagePaths.Add(FName(TEXT("/Game")));
        Filter.bRecursivePaths = true;

        TArray<FAssetData> Assets;
        AssetRegistryModule.Get().GetAssets(Filter, Assets);
        for (const FAssetData& Candidate : Assets)
        {
            if (Candidate.AssetName.ToString().Equals(RawPath, ESearchCase::IgnoreCase))
            {
                WidgetBlueprint = Cast<UWidgetBlueprint>(Candidate.GetAsset());
                break;
            }
        }
    }

    if (!WidgetBlueprint)
    {
        OutError = FString::Printf(TEXT("Widget Blueprint not found or not a UWidgetBlueprint: %s"), *RawPath);
    }
    return WidgetBlueprint;
}

UWidgetBlueprint* ResolveWidgetBlueprint(const TSharedPtr<FJsonObject>& Params, FString& OutPath, FString& OutError)
{
    if (!TryGetWidgetBlueprintPath(Params, OutPath, OutError))
    {
        return nullptr;
    }
    return LoadWidgetBlueprintAsset(OutPath, OutError);
}

void MarkWidgetBlueprintModified(UWidgetBlueprint* WidgetBlueprint, bool bStructural = true)
{
    if (!WidgetBlueprint)
    {
        return;
    }

    WidgetBlueprint->Modify();
    if (WidgetBlueprint->WidgetTree)
    {
        WidgetBlueprint->WidgetTree->Modify();
    }
    WidgetBlueprint->MarkPackageDirty();

    if (bStructural)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
    }
    else
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
    }
}

FString BlueprintStatusToString(EBlueprintStatus Status)
{
    switch (Status)
    {
    case BS_Unknown: return TEXT("Unknown");
    case BS_Dirty: return TEXT("Dirty");
    case BS_Error: return TEXT("Error");
    case BS_UpToDate: return TEXT("UpToDate");
    case BS_BeingCreated: return TEXT("BeingCreated");
    case BS_UpToDateWithWarnings: return TEXT("UpToDateWithWarnings");
    default: return TEXT("Invalid");
    }
}

bool CompileWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint, TSharedPtr<FJsonObject> Response = nullptr)
{
    if (!WidgetBlueprint)
    {
        return false;
    }

    FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
    const bool bCompiled = WidgetBlueprint->Status != BS_Error && WidgetBlueprint->GeneratedClass != nullptr;
    if (Response)
    {
        Response->SetBoolField(TEXT("compiled_success"), bCompiled);
        Response->SetStringField(TEXT("compile_status"), BlueprintStatusToString(WidgetBlueprint->Status));
        Response->SetNumberField(TEXT("compile_status_code"), static_cast<int32>(WidgetBlueprint->Status));
    }
    return bCompiled;
}

bool TryReadNumberArray(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, int32 ExpectedMinCount, TArray<double>& OutValues)
{
    const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
    if (!Params->TryGetArrayField(FieldName, JsonArray) || !JsonArray || JsonArray->Num() < ExpectedMinCount)
    {
        return false;
    }

    OutValues.Reset();
    for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
    {
        if (!Value.IsValid() || Value->Type != EJson::Number)
        {
            return false;
        }
        OutValues.Add(Value->AsNumber());
    }
    return true;
}

FVector2D ReadVector2D(const TArray<double>& Values, int32 Offset = 0)
{
    return FVector2D(
        Values.IsValidIndex(Offset) ? Values[Offset] : 0.0,
        Values.IsValidIndex(Offset + 1) ? Values[Offset + 1] : 0.0);
}

FMargin ReadMargin(const TArray<double>& Values)
{
    return FMargin(
        Values.IsValidIndex(0) ? Values[0] : 0.0,
        Values.IsValidIndex(1) ? Values[1] : 0.0,
        Values.IsValidIndex(2) ? Values[2] : 0.0,
        Values.IsValidIndex(3) ? Values[3] : 0.0);
}

bool TryReadColorArray(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, FLinearColor& OutColor)
{
    TArray<double> Values;
    if (!TryReadNumberArray(Params, FieldName, 3, Values))
    {
        return false;
    }

    OutColor = FLinearColor(
        Values.IsValidIndex(0) ? Values[0] : 1.0,
        Values.IsValidIndex(1) ? Values[1] : 1.0,
        Values.IsValidIndex(2) ? Values[2] : 1.0,
        Values.IsValidIndex(3) ? Values[3] : 1.0);
    return true;
}

bool TryReadColorField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, FLinearColor& OutColor)
{
    const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
    if (!Object->TryGetArrayField(FieldName, JsonArray) || !JsonArray || JsonArray->Num() < 3)
    {
        return false;
    }
    TArray<double> Values;
    for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
    {
        if (!Value.IsValid() || Value->Type != EJson::Number)
        {
            return false;
        }
        Values.Add(Value->AsNumber());
    }
    OutColor = FLinearColor(
        Values.IsValidIndex(0) ? Values[0] : 1.0,
        Values.IsValidIndex(1) ? Values[1] : 1.0,
        Values.IsValidIndex(2) ? Values[2] : 1.0,
        Values.IsValidIndex(3) ? Values[3] : 1.0);
    return true;
}

EHorizontalAlignment ParseHorizontalAlignment(const FString& Alignment)
{
    if (Alignment.Equals(TEXT("Fill"), ESearchCase::IgnoreCase)) { return HAlign_Fill; }
    if (Alignment.Equals(TEXT("Center"), ESearchCase::IgnoreCase)) { return HAlign_Center; }
    if (Alignment.Equals(TEXT("Right"), ESearchCase::IgnoreCase)) { return HAlign_Right; }
    return HAlign_Left;
}

EVerticalAlignment ParseVerticalAlignment(const FString& Alignment)
{
    if (Alignment.Equals(TEXT("Fill"), ESearchCase::IgnoreCase)) { return VAlign_Fill; }
    if (Alignment.Equals(TEXT("Center"), ESearchCase::IgnoreCase)) { return VAlign_Center; }
    if (Alignment.Equals(TEXT("Bottom"), ESearchCase::IgnoreCase)) { return VAlign_Bottom; }
    return VAlign_Top;
}

FString HorizontalAlignmentToString(EHorizontalAlignment Alignment)
{
    switch (Alignment)
    {
    case HAlign_Fill: return TEXT("Fill");
    case HAlign_Center: return TEXT("Center");
    case HAlign_Right: return TEXT("Right");
    default: return TEXT("Left");
    }
}

FString VerticalAlignmentToString(EVerticalAlignment Alignment)
{
    switch (Alignment)
    {
    case VAlign_Fill: return TEXT("Fill");
    case VAlign_Center: return TEXT("Center");
    case VAlign_Bottom: return TEXT("Bottom");
    default: return TEXT("Top");
    }
}

UClass* TryLoadCommonUIClass(const FString& ClassName)
{
    FModuleManager::Get().LoadModule(TEXT("CommonUI"));
    return LoadClass<UWidget>(nullptr, *FString::Printf(TEXT("/Script/CommonUI.%s"), *ClassName));
}

UClass* ResolveWidgetClass(const FString& WidgetType, FString& OutError)
{
    if (WidgetType.IsEmpty())
    {
        OutError = TEXT("widget_type is required");
        return nullptr;
    }

    UClass* WidgetClass = nullptr;
    if (WidgetType.StartsWith(TEXT("/Script/")))
    {
        WidgetClass = LoadClass<UWidget>(nullptr, *WidgetType);
    }
    else if (WidgetType.Equals(TEXT("CanvasPanel"), ESearchCase::IgnoreCase) || WidgetType.Equals(TEXT("Canvas"), ESearchCase::IgnoreCase))
    {
        WidgetClass = UCanvasPanel::StaticClass();
    }
    else if (WidgetType.Equals(TEXT("VerticalBox"), ESearchCase::IgnoreCase))
    {
        WidgetClass = UVerticalBox::StaticClass();
    }
    else if (WidgetType.Equals(TEXT("HorizontalBox"), ESearchCase::IgnoreCase))
    {
        WidgetClass = UHorizontalBox::StaticClass();
    }
    else if (WidgetType.Equals(TEXT("Overlay"), ESearchCase::IgnoreCase))
    {
        WidgetClass = UOverlay::StaticClass();
    }
    else if (WidgetType.Equals(TEXT("Border"), ESearchCase::IgnoreCase))
    {
        WidgetClass = UBorder::StaticClass();
    }
    else if (WidgetType.Equals(TEXT("Button"), ESearchCase::IgnoreCase))
    {
        WidgetClass = UButton::StaticClass();
    }
    else if (WidgetType.Equals(TEXT("TextBlock"), ESearchCase::IgnoreCase) || WidgetType.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
    {
        WidgetClass = UTextBlock::StaticClass();
    }
    else if (WidgetType.Equals(TEXT("Image"), ESearchCase::IgnoreCase))
    {
        WidgetClass = UImage::StaticClass();
    }
    else if (WidgetType.Equals(TEXT("ProgressBar"), ESearchCase::IgnoreCase) || WidgetType.Equals(TEXT("Progress Bar"), ESearchCase::IgnoreCase))
    {
        WidgetClass = UProgressBar::StaticClass();
    }
    else if (WidgetType.Equals(TEXT("Slider"), ESearchCase::IgnoreCase))
    {
        WidgetClass = USlider::StaticClass();
    }
    else if (WidgetType.Equals(TEXT("CheckBox"), ESearchCase::IgnoreCase) || WidgetType.Equals(TEXT("Check Box"), ESearchCase::IgnoreCase))
    {
        WidgetClass = UCheckBox::StaticClass();
    }
    else if (WidgetType.Equals(TEXT("ComboBoxString"), ESearchCase::IgnoreCase) || WidgetType.Equals(TEXT("ComboBox"), ESearchCase::IgnoreCase) || WidgetType.Equals(TEXT("Combo Box"), ESearchCase::IgnoreCase))
    {
        WidgetClass = UComboBoxString::StaticClass();
    }
    else if (WidgetType.Equals(TEXT("ScrollBox"), ESearchCase::IgnoreCase) || WidgetType.Equals(TEXT("Scroll Box"), ESearchCase::IgnoreCase))
    {
        WidgetClass = UScrollBox::StaticClass();
    }
    else if (WidgetType.Equals(TEXT("UniformGrid"), ESearchCase::IgnoreCase) || WidgetType.Equals(TEXT("UniformGridPanel"), ESearchCase::IgnoreCase) || WidgetType.Equals(TEXT("Uniform Grid"), ESearchCase::IgnoreCase))
    {
        WidgetClass = UUniformGridPanel::StaticClass();
    }
    else if (WidgetType.Equals(TEXT("CommonTextBlock"), ESearchCase::IgnoreCase) || WidgetType.Equals(TEXT("CommonText"), ESearchCase::IgnoreCase))
    {
        WidgetClass = TryLoadCommonUIClass(TEXT("CommonTextBlock"));
    }
    else if (WidgetType.Equals(TEXT("CommonActivatableWidget"), ESearchCase::IgnoreCase))
    {
        WidgetClass = TryLoadCommonUIClass(TEXT("CommonActivatableWidget"));
    }
    else
    {
        WidgetClass = LoadClass<UWidget>(nullptr, *WidgetType);
    }

    if (!WidgetClass)
    {
        OutError = FString::Printf(TEXT("Unsupported widget_type '%s'. Check spelling or pass a full /Script/... widget class path."), *WidgetType);
        return nullptr;
    }
    if (!WidgetClass->IsChildOf(UWidget::StaticClass()))
    {
        OutError = FString::Printf(TEXT("Class '%s' is not a UWidget subclass"), *WidgetClass->GetName());
        return nullptr;
    }
    if (WidgetClass->HasAnyClassFlags(CLASS_Abstract))
    {
        OutError = FString::Printf(TEXT("Widget class '%s' is abstract and cannot be instantiated directly. Create a concrete Blueprint subclass first."), *WidgetClass->GetName());
        return nullptr;
    }
    return WidgetClass;
}

UWidget* FindWidgetByName(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, FString& OutError)
{
    if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
    {
        OutError = TEXT("Widget Blueprint has no WidgetTree");
        return nullptr;
    }
    if (WidgetName.IsEmpty())
    {
        OutError = TEXT("widget_name is required");
        return nullptr;
    }

    UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    if (!Widget)
    {
        OutError = FString::Printf(TEXT("Widget not found in WidgetTree: %s"), *WidgetName);
    }
    return Widget;
}

void AddMarginJson(TSharedPtr<FJsonObject> Object, const FString& FieldName, const FMargin& Margin)
{
    Object->SetArrayField(FieldName, MakeNumberArray({Margin.Left, Margin.Top, Margin.Right, Margin.Bottom}));
}

void AddSlotPropertiesJson(TSharedPtr<FJsonObject> Object, UPanelSlot* Slot)
{
    if (!Slot)
    {
        return;
    }

    Object->SetStringField(TEXT("slot_class"), Slot->GetClass()->GetName());
    TSharedPtr<FJsonObject> SlotProperties = MakeShared<FJsonObject>();

    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
    {
        const FAnchors Anchors = CanvasSlot->GetAnchors();
        SlotProperties->SetArrayField(TEXT("anchors"), MakeNumberArray({
            Anchors.Minimum.X,
            Anchors.Minimum.Y,
            Anchors.Maximum.X,
            Anchors.Maximum.Y
        }));
        const FVector2D Position = CanvasSlot->GetPosition();
        const FVector2D Size = CanvasSlot->GetSize();
        const FVector2D Alignment = CanvasSlot->GetAlignment();
        SlotProperties->SetArrayField(TEXT("position"), MakeNumberArray({Position.X, Position.Y}));
        SlotProperties->SetArrayField(TEXT("size"), MakeNumberArray({Size.X, Size.Y}));
        SlotProperties->SetArrayField(TEXT("alignment"), MakeNumberArray({Alignment.X, Alignment.Y}));
        AddMarginJson(SlotProperties, TEXT("offsets"), CanvasSlot->GetOffsets());
        SlotProperties->SetNumberField(TEXT("z_order"), CanvasSlot->GetZOrder());
        SlotProperties->SetBoolField(TEXT("auto_size"), CanvasSlot->GetAutoSize());
    }
    else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(Slot))
    {
        AddMarginJson(SlotProperties, TEXT("padding"), VerticalSlot->GetPadding());
        SlotProperties->SetStringField(TEXT("horizontal_alignment"), HorizontalAlignmentToString(VerticalSlot->GetHorizontalAlignment()));
        SlotProperties->SetStringField(TEXT("vertical_alignment"), VerticalAlignmentToString(VerticalSlot->GetVerticalAlignment()));
    }
    else if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(Slot))
    {
        AddMarginJson(SlotProperties, TEXT("padding"), HorizontalSlot->GetPadding());
        SlotProperties->SetStringField(TEXT("horizontal_alignment"), HorizontalAlignmentToString(HorizontalSlot->GetHorizontalAlignment()));
        SlotProperties->SetStringField(TEXT("vertical_alignment"), VerticalAlignmentToString(HorizontalSlot->GetVerticalAlignment()));
    }
    else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Slot))
    {
        AddMarginJson(SlotProperties, TEXT("padding"), OverlaySlot->GetPadding());
        SlotProperties->SetStringField(TEXT("horizontal_alignment"), HorizontalAlignmentToString(OverlaySlot->GetHorizontalAlignment()));
        SlotProperties->SetStringField(TEXT("vertical_alignment"), VerticalAlignmentToString(OverlaySlot->GetVerticalAlignment()));
    }
    else if (UBorderSlot* BorderSlot = Cast<UBorderSlot>(Slot))
    {
        AddMarginJson(SlotProperties, TEXT("padding"), BorderSlot->GetPadding());
        SlotProperties->SetStringField(TEXT("horizontal_alignment"), HorizontalAlignmentToString(BorderSlot->GetHorizontalAlignment()));
        SlotProperties->SetStringField(TEXT("vertical_alignment"), VerticalAlignmentToString(BorderSlot->GetVerticalAlignment()));
    }
    else if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Slot))
    {
        AddMarginJson(SlotProperties, TEXT("padding"), ButtonSlot->GetPadding());
        SlotProperties->SetStringField(TEXT("horizontal_alignment"), HorizontalAlignmentToString(ButtonSlot->GetHorizontalAlignment()));
        SlotProperties->SetStringField(TEXT("vertical_alignment"), VerticalAlignmentToString(ButtonSlot->GetVerticalAlignment()));
    }
    else if (UUniformGridSlot* UniformGridSlot = Cast<UUniformGridSlot>(Slot))
    {
        SlotProperties->SetNumberField(TEXT("row"), UniformGridSlot->GetRow());
        SlotProperties->SetNumberField(TEXT("column"), UniformGridSlot->GetColumn());
        SlotProperties->SetStringField(TEXT("horizontal_alignment"), HorizontalAlignmentToString(UniformGridSlot->GetHorizontalAlignment()));
        SlotProperties->SetStringField(TEXT("vertical_alignment"), VerticalAlignmentToString(UniformGridSlot->GetVerticalAlignment()));
    }

    Object->SetObjectField(TEXT("slot_properties"), SlotProperties);
}

TSharedPtr<FJsonObject> RemoveWidgetFromTree(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget, bool bForceGC)
{
    if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree || !Widget)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid Widget Blueprint or widget for removal"));
    }

    TArray<UWidget*> WidgetsToRemove;
    WidgetBlueprint->WidgetTree->ForEachWidget([Widget, &WidgetsToRemove](UWidget* CandidateWidget)
    {
        UWidget* CurrentWidget = CandidateWidget;
        while (CurrentWidget)
        {
            if (CurrentWidget == Widget)
            {
                WidgetsToRemove.Add(CandidateWidget);
                break;
            }
            CurrentWidget = CurrentWidget->GetParent();
        }
    });

    const bool bWasRoot = WidgetBlueprint->WidgetTree->RootWidget == Widget;
    bool bRemoved = true;
    if (bWasRoot)
    {
        WidgetBlueprint->WidgetTree->RootWidget = nullptr;
    }
    else
    {
        bRemoved = WidgetBlueprint->WidgetTree->RemoveWidget(Widget);
    }

    for (UWidget* RemovedWidget : WidgetsToRemove)
    {
        if (!RemovedWidget)
        {
            continue;
        }
        if (RemovedWidget->bIsVariable)
        {
            WidgetBlueprint->OnVariableRemoved(RemovedWidget->GetFName());
            RemovedWidget->bIsVariable = false;
        }
        RemovedWidget->Modify();
        RemovedWidget->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
        RemovedWidget->MarkAsGarbage();
    }

    MarkWidgetBlueprintModified(WidgetBlueprint, true);
    if (bForceGC)
    {
        CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    }

    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetBoolField(TEXT("removed"), bRemoved || bWasRoot);
    Response->SetBoolField(TEXT("removed_root"), bWasRoot);
    Response->SetNumberField(TEXT("removed_widget_count"), WidgetsToRemove.Num());
    return Response;
}

UWorld* GetUMGRuntimeWorld()
{
    if (!GEditor)
    {
        return nullptr;
    }
    if (GEditor->PlayWorld)
    {
        return GEditor->PlayWorld;
    }
    return GEditor->GetEditorWorldContext().World();
}

APlayerController* GetUMGPlayerController(UWorld* World)
{
    return World ? World->GetFirstPlayerController() : nullptr;
}

UUserWidget* FindRuntimeWidgetInstance(const FString& InstanceName)
{
    if (TWeakObjectPtr<UUserWidget>* Found = GUMGRuntimeWidgetInstances.Find(InstanceName))
    {
        if (Found->IsValid())
        {
            return Found->Get();
        }
        GUMGRuntimeWidgetInstances.Remove(InstanceName);
    }
    return nullptr;
}

TSharedPtr<FJsonObject> MakeTemplateParams(const FString& BlueprintPath)
{
    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("widget_blueprint"), BlueprintPath);
    return Params;
}

TSharedPtr<FJsonObject> MakeAddWidgetParams(
    const FString& BlueprintPath,
    const FString& WidgetType,
    const FString& WidgetName,
    const FString& ParentName = FString(),
    bool bIsVariable = false)
{
    TSharedPtr<FJsonObject> Params = MakeTemplateParams(BlueprintPath);
    Params->SetStringField(TEXT("widget_type"), WidgetType);
    Params->SetStringField(TEXT("widget_name"), WidgetName);
    if (!ParentName.IsEmpty())
    {
        Params->SetStringField(TEXT("parent_name"), ParentName);
    }
    Params->SetBoolField(TEXT("is_variable"), bIsVariable);
    Params->SetBoolField(TEXT("replace_existing"), true);
    return Params;
}

TSharedPtr<FJsonObject> MakeSetTextParams(const FString& BlueprintPath, const FString& WidgetName, const FString& Text)
{
    TSharedPtr<FJsonObject> Params = MakeTemplateParams(BlueprintPath);
    Params->SetStringField(TEXT("widget_name"), WidgetName);
    Params->SetStringField(TEXT("text"), Text);
    return Params;
}

TSharedPtr<FJsonObject> MakeSlotParams(
    const FString& BlueprintPath,
    const FString& WidgetName,
    std::initializer_list<double> Anchors,
    std::initializer_list<double> Position,
    std::initializer_list<double> Size,
    std::initializer_list<double> Alignment)
{
    TSharedPtr<FJsonObject> Params = MakeTemplateParams(BlueprintPath);
    Params->SetStringField(TEXT("widget_name"), WidgetName);
    Params->SetArrayField(TEXT("anchors"), MakeNumberArray(Anchors));
    Params->SetArrayField(TEXT("position"), MakeNumberArray(Position));
    Params->SetArrayField(TEXT("size"), MakeNumberArray(Size));
    Params->SetArrayField(TEXT("alignment"), MakeNumberArray(Alignment));
    return Params;
}
}

FEpicUnrealMCPUMGCommands::FEpicUnrealMCPUMGCommands() {}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPUMGCommands::*)(const TSharedPtr<FJsonObject>&);
    static const TMap<FString, Handler> Dispatch = {
        {TEXT("create_widget_blueprint"), &FEpicUnrealMCPUMGCommands::HandleCreateWidgetBlueprint},
        {TEXT("add_widget_to_widget_blueprint"), &FEpicUnrealMCPUMGCommands::HandleAddWidgetToWidgetBlueprint},
        {TEXT("remove_widget_from_widget_blueprint"), &FEpicUnrealMCPUMGCommands::HandleRemoveWidgetFromWidgetBlueprint},
        {TEXT("set_widget_slot_properties"), &FEpicUnrealMCPUMGCommands::HandleSetWidgetSlotProperties},
        {TEXT("set_widget_text"), &FEpicUnrealMCPUMGCommands::HandleSetWidgetText},
        {TEXT("set_widget_font"), &FEpicUnrealMCPUMGCommands::HandleSetWidgetFont},
        {TEXT("set_widget_color"), &FEpicUnrealMCPUMGCommands::HandleSetWidgetColor},
        {TEXT("set_widget_brush"), &FEpicUnrealMCPUMGCommands::HandleSetWidgetBrush},
        {TEXT("set_widget_style"), &FEpicUnrealMCPUMGCommands::HandleSetWidgetStyle},
        {TEXT("bind_widget_button_on_clicked"), &FEpicUnrealMCPUMGCommands::HandleBindWidgetButtonOnClicked},
        {TEXT("bind_widget_property"), &FEpicUnrealMCPUMGCommands::HandleBindWidgetProperty},
        {TEXT("create_widget_animation"), &FEpicUnrealMCPUMGCommands::HandleCreateWidgetAnimation},
        {TEXT("compile_widget_blueprint"), &FEpicUnrealMCPUMGCommands::HandleCompileWidgetBlueprint},
        {TEXT("inspect_widget_blueprint"), &FEpicUnrealMCPUMGCommands::HandleInspectWidgetBlueprint},
        {TEXT("add_widget_to_viewport"), &FEpicUnrealMCPUMGCommands::HandleAddWidgetToViewport},
        {TEXT("remove_widget_from_parent"), &FEpicUnrealMCPUMGCommands::HandleRemoveWidgetFromParent},
        {TEXT("click_widget_button"), &FEpicUnrealMCPUMGCommands::HandleClickWidgetButton},
        {TEXT("set_ui_input_mode"), &FEpicUnrealMCPUMGCommands::HandleSetUIInputMode},
        {TEXT("set_mouse_cursor_visible"), &FEpicUnrealMCPUMGCommands::HandleSetMouseCursorVisible},
        {TEXT("create_ui_template"), &FEpicUnrealMCPUMGCommands::HandleCreateUITemplate},
        {TEXT("create_widget_instance"), &FEpicUnrealMCPUMGCommands::HandleCreateWidgetInstance},
    };

    const Handler* FoundHandler = Dispatch.Find(CommandType);
    if (!FoundHandler)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown UMG command: %s"), *CommandType));
    }
    return (this->*(*FoundHandler))(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleCreateWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString RawPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), RawPath))
    {
        Params->TryGetStringField(TEXT("blueprint_path"), RawPath);
    }
    if (RawPath.IsEmpty())
    {
        Params->TryGetStringField(TEXT("widget_blueprint"), RawPath);
    }
    if (RawPath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path is required"));
    }

    const FString PackagePath = NormalizeWidgetBlueprintPath(RawPath);
    const FString AssetName = FPackageName::GetShortName(PackagePath);
    const FString PackageFolder = FPackageName::GetLongPackagePath(PackagePath);
    if (!UEditorAssetLibrary::DoesDirectoryExist(PackageFolder))
    {
        UEditorAssetLibrary::MakeDirectory(PackageFolder);
    }

    if (UEditorAssetLibrary::DoesAssetExist(PackagePath))
    {
        UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(PackagePath);
        UWidgetBlueprint* ExistingWidgetBlueprint = Cast<UWidgetBlueprint>(ExistingAsset);
        if (!ExistingWidgetBlueprint)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Asset already exists and is not a Widget Blueprint: %s"), *PackagePath));
        }
        TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
        Response->SetBoolField(TEXT("already_exists"), true);
        Response->SetStringField(TEXT("asset_path"), PackagePath);
        Response->SetStringField(TEXT("name"), AssetName);
        Response->SetStringField(TEXT("parent_class"), ExistingWidgetBlueprint->ParentClass ? ExistingWidgetBlueprint->ParentClass->GetPathName() : TEXT(""));
        return Response;
    }

    FString ParentClassName;
    Params->TryGetStringField(TEXT("parent_class"), ParentClassName);
    bool bCommonUI = false;
    Params->TryGetBoolField(TEXT("common_ui"), bCommonUI);

    UClass* ParentClass = UUserWidget::StaticClass();
    if (bCommonUI && ParentClassName.IsEmpty())
    {
        ParentClassName = TEXT("CommonActivatableWidget");
    }
    if (!ParentClassName.IsEmpty())
    {
        if (ParentClassName.Equals(TEXT("UserWidget"), ESearchCase::IgnoreCase))
        {
            ParentClass = UUserWidget::StaticClass();
        }
        else if (ParentClassName.Equals(TEXT("CommonActivatableWidget"), ESearchCase::IgnoreCase))
        {
            FModuleManager::Get().LoadModule(TEXT("CommonUI"));
            ParentClass = LoadClass<UUserWidget>(nullptr, TEXT("/Script/CommonUI.CommonActivatableWidget"));
            if (!ParentClass)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("CommonUI parent requested, but /Script/CommonUI.CommonActivatableWidget could not be loaded. Enable the CommonUI plugin and restart the editor."));
            }
        }
        else if (ParentClassName.StartsWith(TEXT("/Script/")))
        {
            ParentClass = LoadClass<UUserWidget>(nullptr, *ParentClassName);
        }
        else
        {
            ParentClass = LoadClass<UUserWidget>(nullptr, *FString::Printf(TEXT("/Script/UMG.%s"), *ParentClassName));
        }
    }

    if (!ParentClass || !ParentClass->IsChildOf(UUserWidget::StaticClass()))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Invalid UserWidget parent class: %s"), *ParentClassName));
    }

    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create package: %s"), *PackagePath));
    }

    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
        ParentClass,
        Package,
        FName(*AssetName),
        BPTYPE_Normal,
        UWidgetBlueprint::StaticClass(),
        UWidgetBlueprintGeneratedClass::StaticClass(),
        FName(TEXT("MCP_UMG"))));

    if (!WidgetBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("FKismetEditorUtilities::CreateBlueprint failed for Widget Blueprint"));
    }

    WidgetBlueprint->bCanCallInitializedWithoutPlayerContext = true;
    FAssetRegistryModule::AssetCreated(WidgetBlueprint);
    WidgetBlueprint->MarkPackageDirty();

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
        UEditorAssetLibrary::SaveLoadedAsset(WidgetBlueprint);
    }

    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("asset_path"), PackagePath);
    Response->SetStringField(TEXT("name"), AssetName);
    Response->SetStringField(TEXT("parent_class"), ParentClass->GetPathName());
    Response->SetBoolField(TEXT("common_ui"), ParentClass->GetName().Contains(TEXT("Common")));
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleAddWidgetToWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    FString Error;
    UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(Params, BlueprintPath, Error);
    if (!WidgetBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }
    if (!WidgetBlueprint->WidgetTree)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Widget Blueprint has no WidgetTree"));
    }

    FString WidgetType;
    if (!Params->TryGetStringField(TEXT("widget_type"), WidgetType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("widget_type is required"));
    }
    FString WidgetName;
    if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) || WidgetName.IsEmpty())
    {
        WidgetName = WidgetType;
    }

    bool bReplaceExisting = false;
    Params->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);
    if (UWidget* ExistingWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName)))
    {
        if (!bReplaceExisting)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Widget '%s' already exists. Pass replace_existing=true to remove and recreate it safely."), *WidgetName));
        }
        TSharedPtr<FJsonObject> RemovalResult = RemoveWidgetFromTree(WidgetBlueprint, ExistingWidget, true);
        if (!RemovalResult->GetBoolField(TEXT("success")))
        {
            return RemovalResult;
        }
    }

    UClass* WidgetClass = ResolveWidgetClass(WidgetType, Error);
    if (!WidgetClass)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    UWidget* NewWidget = WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
    if (!NewWidget)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to construct widget '%s' of type '%s'"), *WidgetName, *WidgetType));
    }

#if WITH_EDITORONLY_DATA
    NewWidget->WidgetGeneratedBy = WidgetBlueprint;
#endif
    NewWidget->Modify();

    bool bIsVariable = false;
    Params->TryGetBoolField(TEXT("is_variable"), bIsVariable);
    NewWidget->bIsVariable = bIsVariable;
    if (bIsVariable)
    {
        WidgetBlueprint->OnVariableAdded(NewWidget->GetFName());
    }

    FString ParentName;
    Params->TryGetStringField(TEXT("parent_name"), ParentName);
    if (WidgetBlueprint->WidgetTree->RootWidget == nullptr && ParentName.IsEmpty())
    {
        WidgetBlueprint->WidgetTree->RootWidget = NewWidget;
    }
    else
    {
        UWidget* ParentWidget = nullptr;
        if (ParentName.IsEmpty() || ParentName.Equals(TEXT("Root"), ESearchCase::IgnoreCase))
        {
            ParentWidget = WidgetBlueprint->WidgetTree->RootWidget;
        }
        else
        {
            ParentWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*ParentName));
        }

        UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
        if (!ParentPanel)
        {
            NewWidget->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Parent widget '%s' is not a panel/content widget that can accept children"), *ParentName));
        }

        ParentPanel->Modify();
        UPanelSlot* NewSlot = ParentPanel->AddChild(NewWidget);
        if (!NewSlot)
        {
            NewWidget->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Failed to add widget '%s' to parent '%s'. The parent may already contain its maximum number of children."), *WidgetName, *ParentPanel->GetName()));
        }
        NewSlot->Modify();
    }

    FString Text;
    if (Params->TryGetStringField(TEXT("text"), Text))
    {
        if (UTextBlock* TextBlock = Cast<UTextBlock>(NewWidget))
        {
            TextBlock->SetText(FText::FromString(Text));
        }
    }
    double Percent = 0.0;
    if (Params->TryGetNumberField(TEXT("percent"), Percent))
    {
        if (UProgressBar* ProgressBar = Cast<UProgressBar>(NewWidget))
        {
            ProgressBar->SetPercent(static_cast<float>(Percent));
        }
    }
    double Value = 0.0;
    if (Params->TryGetNumberField(TEXT("value"), Value))
    {
        if (USlider* Slider = Cast<USlider>(NewWidget))
        {
            Slider->SetValue(static_cast<float>(Value));
        }
    }
    bool bChecked = false;
    if (Params->TryGetBoolField(TEXT("checked"), bChecked))
    {
        if (UCheckBox* CheckBox = Cast<UCheckBox>(NewWidget))
        {
            CheckBox->SetIsChecked(bChecked);
        }
    }
    const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
    if (Params->TryGetArrayField(TEXT("options"), Options) && Options)
    {
        if (UComboBoxString* ComboBox = Cast<UComboBoxString>(NewWidget))
        {
            ComboBox->ClearOptions();
            for (const TSharedPtr<FJsonValue>& Option : *Options)
            {
                ComboBox->AddOption(Option->AsString());
            }
        }
    }

    MarkWidgetBlueprintModified(WidgetBlueprint, true);

    bool bCompileAfter = false;
    Params->TryGetBoolField(TEXT("compile_after"), bCompileAfter);
    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("asset_path"), BlueprintPath);
    Response->SetStringField(TEXT("widget_name"), NewWidget->GetName());
    Response->SetStringField(TEXT("widget_type"), WidgetClass->GetName());
    Response->SetBoolField(TEXT("is_variable"), NewWidget->bIsVariable);
    Response->SetStringField(TEXT("parent_name"), NewWidget->GetParent() ? NewWidget->GetParent()->GetName() : TEXT(""));
    if (NewWidget->Slot)
    {
        Response->SetStringField(TEXT("slot_class"), NewWidget->Slot->GetClass()->GetName());
    }
    if (WidgetBlueprint->WidgetTree->RootWidget)
    {
        Response->SetStringField(TEXT("root_widget"), WidgetBlueprint->WidgetTree->RootWidget->GetName());
    }
    if (bCompileAfter)
    {
        CompileWidgetBlueprint(WidgetBlueprint, Response);
    }
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleRemoveWidgetFromWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    FString Error;
    UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(Params, BlueprintPath, Error);
    if (!WidgetBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    bool bRemoveRoot = false;
    Params->TryGetBoolField(TEXT("remove_root"), bRemoveRoot);

    UWidget* Widget = nullptr;
    FString WidgetName;
    if (Params->TryGetStringField(TEXT("widget_name"), WidgetName) && !WidgetName.IsEmpty())
    {
        Widget = FindWidgetByName(WidgetBlueprint, WidgetName, Error);
    }
    else if (bRemoveRoot && WidgetBlueprint->WidgetTree)
    {
        Widget = WidgetBlueprint->WidgetTree->RootWidget;
        WidgetName = Widget ? Widget->GetName() : TEXT("");
    }
    if (!Widget)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error.IsEmpty() ? TEXT("widget_name is required unless remove_root=true") : Error);
    }

    bool bForceGC = true;
    Params->TryGetBoolField(TEXT("force_gc"), bForceGC);
    TSharedPtr<FJsonObject> Response = RemoveWidgetFromTree(WidgetBlueprint, Widget, bForceGC);
    Response->SetStringField(TEXT("asset_path"), BlueprintPath);
    Response->SetStringField(TEXT("widget_name"), WidgetName);
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleSetWidgetSlotProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    FString Error;
    UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(Params, BlueprintPath, Error);
    if (!WidgetBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString WidgetName;
    Params->TryGetStringField(TEXT("widget_name"), WidgetName);
    UWidget* Widget = FindWidgetByName(WidgetBlueprint, WidgetName, Error);
    if (!Widget)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }
    UPanelSlot* Slot = Widget->Slot;
    if (!Slot)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget '%s' has no parent slot. Root widgets do not expose slot properties."), *WidgetName));
    }

    Slot->Modify();
    TArray<double> Values;

    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
    {
        if (TryReadNumberArray(Params, TEXT("anchors"), 4, Values))
        {
            CanvasSlot->SetAnchors(FAnchors(Values[0], Values[1], Values[2], Values[3]));
        }
        if (TryReadNumberArray(Params, TEXT("position"), 2, Values))
        {
            CanvasSlot->SetPosition(ReadVector2D(Values));
        }
        if (TryReadNumberArray(Params, TEXT("size"), 2, Values))
        {
            CanvasSlot->SetSize(ReadVector2D(Values));
        }
        if (TryReadNumberArray(Params, TEXT("alignment"), 2, Values))
        {
            CanvasSlot->SetAlignment(ReadVector2D(Values));
        }
        bool bAutoSize = false;
        if (Params->TryGetBoolField(TEXT("auto_size"), bAutoSize))
        {
            CanvasSlot->SetAutoSize(bAutoSize);
        }
        double ZOrder = 0.0;
        if (Params->TryGetNumberField(TEXT("z_order"), ZOrder))
        {
            CanvasSlot->SetZOrder(static_cast<int32>(ZOrder));
        }
    }

    FString HAlign;
    FString VAlign;
    Params->TryGetStringField(TEXT("horizontal_alignment"), HAlign);
    Params->TryGetStringField(TEXT("vertical_alignment"), VAlign);
    const bool bHasPadding = TryReadNumberArray(Params, TEXT("padding"), 4, Values);

    auto ApplyBoxAlignment = [&](auto* BoxSlot)
    {
        if (bHasPadding)
        {
            BoxSlot->SetPadding(ReadMargin(Values));
        }
        if (!HAlign.IsEmpty())
        {
            BoxSlot->SetHorizontalAlignment(ParseHorizontalAlignment(HAlign));
        }
        if (!VAlign.IsEmpty())
        {
            BoxSlot->SetVerticalAlignment(ParseVerticalAlignment(VAlign));
        }
    };

    if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(Slot))
    {
        ApplyBoxAlignment(VerticalSlot);
    }
    else if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(Slot))
    {
        ApplyBoxAlignment(HorizontalSlot);
    }
    else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Slot))
    {
        ApplyBoxAlignment(OverlaySlot);
    }
    else if (UBorderSlot* BorderSlot = Cast<UBorderSlot>(Slot))
    {
        ApplyBoxAlignment(BorderSlot);
    }
    else if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Slot))
    {
        ApplyBoxAlignment(ButtonSlot);
    }
    else if (UUniformGridSlot* UniformGridSlot = Cast<UUniformGridSlot>(Slot))
    {
        double NumberValue = 0.0;
        if (Params->TryGetNumberField(TEXT("row"), NumberValue))
        {
            UniformGridSlot->SetRow(static_cast<int32>(NumberValue));
        }
        if (Params->TryGetNumberField(TEXT("column"), NumberValue))
        {
            UniformGridSlot->SetColumn(static_cast<int32>(NumberValue));
        }
        if (!HAlign.IsEmpty())
        {
            UniformGridSlot->SetHorizontalAlignment(ParseHorizontalAlignment(HAlign));
        }
        if (!VAlign.IsEmpty())
        {
            UniformGridSlot->SetVerticalAlignment(ParseVerticalAlignment(VAlign));
        }
    }

    MarkWidgetBlueprintModified(WidgetBlueprint, false);
    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("asset_path"), BlueprintPath);
    Response->SetStringField(TEXT("widget_name"), WidgetName);
    AddSlotPropertiesJson(Response, Slot);
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleSetWidgetText(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    FString Error;
    UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(Params, BlueprintPath, Error);
    if (!WidgetBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString WidgetName;
    Params->TryGetStringField(TEXT("widget_name"), WidgetName);
    UWidget* Widget = FindWidgetByName(WidgetBlueprint, WidgetName, Error);
    if (!Widget)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString Text;
    if (!Params->TryGetStringField(TEXT("text"), Text))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("text is required"));
    }

    if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
    {
        TextBlock->Modify();
        TextBlock->SetText(FText::FromString(Text));
    }
    else if (UComboBoxString* ComboBox = Cast<UComboBoxString>(Widget))
    {
        ComboBox->Modify();
        ComboBox->SetSelectedOption(Text);
    }
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Widget '%s' does not support text assignment"), *WidgetName));
    }

    MarkWidgetBlueprintModified(WidgetBlueprint, false);
    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("asset_path"), BlueprintPath);
    Response->SetStringField(TEXT("widget_name"), WidgetName);
    Response->SetStringField(TEXT("text"), Text);
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleSetWidgetFont(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    FString Error;
    UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(Params, BlueprintPath, Error);
    if (!WidgetBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString WidgetName;
    Params->TryGetStringField(TEXT("widget_name"), WidgetName);
    UWidget* Widget = FindWidgetByName(WidgetBlueprint, WidgetName, Error);
    UTextBlock* TextBlock = Cast<UTextBlock>(Widget);
    if (!TextBlock)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error.IsEmpty() ? TEXT("set_widget_font requires a TextBlock/CommonTextBlock widget") : Error);
    }

    FSlateFontInfo FontInfo = TextBlock->GetFont();
    double Size = 0.0;
    if (Params->TryGetNumberField(TEXT("font_size"), Size) || Params->TryGetNumberField(TEXT("size"), Size))
    {
        FontInfo.Size = static_cast<int32>(Size);
    }
    FString Typeface;
    if (Params->TryGetStringField(TEXT("typeface"), Typeface))
    {
        FontInfo.TypefaceFontName = FName(*Typeface);
    }

    TextBlock->Modify();
    TextBlock->SetFont(FontInfo);
    MarkWidgetBlueprintModified(WidgetBlueprint, false);

    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("asset_path"), BlueprintPath);
    Response->SetStringField(TEXT("widget_name"), WidgetName);
    Response->SetNumberField(TEXT("font_size"), FontInfo.Size);
    Response->SetStringField(TEXT("typeface"), FontInfo.TypefaceFontName.ToString());
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleSetWidgetColor(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    FString Error;
    UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(Params, BlueprintPath, Error);
    if (!WidgetBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString WidgetName;
    Params->TryGetStringField(TEXT("widget_name"), WidgetName);
    UWidget* Widget = FindWidgetByName(WidgetBlueprint, WidgetName, Error);
    if (!Widget)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FLinearColor Color;
    if (!TryReadColorArray(Params, TEXT("color"), Color))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("color must be an array [r, g, b, a]"));
    }

    Widget->Modify();
    if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
    {
        TextBlock->SetColorAndOpacity(FSlateColor(Color));
    }
    else if (UImage* Image = Cast<UImage>(Widget))
    {
        Image->SetColorAndOpacity(Color);
    }
    else if (UProgressBar* ProgressBar = Cast<UProgressBar>(Widget))
    {
        ProgressBar->SetFillColorAndOpacity(Color);
    }
    else if (UBorder* Border = Cast<UBorder>(Widget))
    {
        Border->SetBrushColor(Color);
    }
    else if (UButton* Button = Cast<UButton>(Widget))
    {
        Button->SetColorAndOpacity(Color);
    }
    else
    {
        Widget->SetRenderOpacity(Color.A);
    }

    MarkWidgetBlueprintModified(WidgetBlueprint, false);
    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("asset_path"), BlueprintPath);
    Response->SetStringField(TEXT("widget_name"), WidgetName);
    Response->SetArrayField(TEXT("color"), MakeNumberArray({Color.R, Color.G, Color.B, Color.A}));
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleSetWidgetBrush(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    FString Error;
    UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(Params, BlueprintPath, Error);
    if (!WidgetBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString WidgetName;
    Params->TryGetStringField(TEXT("widget_name"), WidgetName);
    UWidget* Widget = FindWidgetByName(WidgetBlueprint, WidgetName, Error);
    if (!Widget)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString ResourcePath;
    Params->TryGetStringField(TEXT("resource_path"), ResourcePath);
    if (ResourcePath.IsEmpty())
    {
        Params->TryGetStringField(TEXT("asset_path_resource"), ResourcePath);
    }

    UObject* Resource = nullptr;
    if (!ResourcePath.IsEmpty())
    {
        Resource = StaticLoadObject(UObject::StaticClass(), nullptr, *ResourcePath);
        if (!Resource)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Brush resource not found: %s"), *ResourcePath));
        }
    }

    TArray<double> SizeValues;
    const bool bHasSize = TryReadNumberArray(Params, TEXT("image_size"), 2, SizeValues) || TryReadNumberArray(Params, TEXT("size"), 2, SizeValues);
    FLinearColor Tint = FLinearColor::White;
    const bool bHasTint = TryReadColorArray(Params, TEXT("tint"), Tint) || TryReadColorArray(Params, TEXT("color"), Tint);

    Widget->Modify();
    if (UImage* Image = Cast<UImage>(Widget))
    {
        if (Resource)
        {
            Image->SetBrushResourceObject(Resource);
        }
        if (bHasSize)
        {
            Image->SetDesiredSizeOverride(ReadVector2D(SizeValues));
        }
        if (bHasTint)
        {
            Image->SetBrushTintColor(FSlateColor(Tint));
        }
    }
    else if (UBorder* Border = Cast<UBorder>(Widget))
    {
        if (UTexture2D* Texture = Cast<UTexture2D>(Resource))
        {
            Border->SetBrushFromTexture(Texture);
        }
        else if (UMaterialInterface* Material = Cast<UMaterialInterface>(Resource))
        {
            Border->SetBrushFromMaterial(Material);
        }
        if (bHasTint)
        {
            Border->SetBrushColor(Tint);
        }
    }
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget '%s' does not expose a supported brush API"), *WidgetName));
    }

    MarkWidgetBlueprintModified(WidgetBlueprint, false);
    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("asset_path"), BlueprintPath);
    Response->SetStringField(TEXT("widget_name"), WidgetName);
    if (Resource)
    {
        Response->SetStringField(TEXT("resource_path"), Resource->GetPathName());
    }
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleSetWidgetStyle(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    FString Error;
    UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(Params, BlueprintPath, Error);
    if (!WidgetBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString WidgetName;
    Params->TryGetStringField(TEXT("widget_name"), WidgetName);
    UWidget* Widget = FindWidgetByName(WidgetBlueprint, WidgetName, Error);
    UButton* Button = Cast<UButton>(Widget);
    if (!Button)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error.IsEmpty() ? TEXT("set_widget_style currently supports Button widgets") : Error);
    }

    const TSharedPtr<FJsonObject>* StyleObject = nullptr;
    if (!Params->TryGetObjectField(TEXT("style"), StyleObject) || !StyleObject || !StyleObject->IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("style object is required"));
    }

    Button->Modify();
    FButtonStyle ButtonStyle = Button->GetStyle();
    FLinearColor Color;
    if (TryReadColorField(*StyleObject, TEXT("normal_color"), Color))
    {
        ButtonStyle.Normal.TintColor = FSlateColor(Color);
    }
    if (TryReadColorField(*StyleObject, TEXT("hovered_color"), Color))
    {
        ButtonStyle.Hovered.TintColor = FSlateColor(Color);
    }
    if (TryReadColorField(*StyleObject, TEXT("pressed_color"), Color))
    {
        ButtonStyle.Pressed.TintColor = FSlateColor(Color);
    }
    if (TryReadColorField(*StyleObject, TEXT("background_color"), Color))
    {
        Button->SetBackgroundColor(Color);
    }
    if (TryReadColorField(*StyleObject, TEXT("color_and_opacity"), Color))
    {
        Button->SetColorAndOpacity(Color);
    }
    Button->SetStyle(ButtonStyle);

    MarkWidgetBlueprintModified(WidgetBlueprint, false);
    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("asset_path"), BlueprintPath);
    Response->SetStringField(TEXT("widget_name"), WidgetName);
    Response->SetStringField(TEXT("style_target"), TEXT("Button"));
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleBindWidgetButtonOnClicked(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    FString Error;
    UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(Params, BlueprintPath, Error);
    if (!WidgetBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString ButtonName;
    if (!Params->TryGetStringField(TEXT("button_name"), ButtonName))
    {
        Params->TryGetStringField(TEXT("widget_name"), ButtonName);
    }
    UWidget* Widget = FindWidgetByName(WidgetBlueprint, ButtonName, Error);
    UButton* Button = Cast<UButton>(Widget);
    if (!Button)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error.IsEmpty() ? TEXT("button_name must identify a UButton widget") : Error);
    }

    if (!Button->bIsVariable)
    {
        Button->Modify();
        Button->bIsVariable = true;
        WidgetBlueprint->OnVariableAdded(Button->GetFName());
        MarkWidgetBlueprintModified(WidgetBlueprint, true);
    }

    CompileWidgetBlueprint(WidgetBlueprint);

    FObjectProperty* ComponentProperty = FindFProperty<FObjectProperty>(WidgetBlueprint->SkeletonGeneratedClass, FName(*ButtonName));
    if (!ComponentProperty && WidgetBlueprint->GeneratedClass)
    {
        ComponentProperty = FindFProperty<FObjectProperty>(WidgetBlueprint->GeneratedClass, FName(*ButtonName));
    }
    if (!ComponentProperty)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Button '%s' is not available as a generated object property. Ensure Is Variable is true and the Widget Blueprint compiles."), *ButtonName));
    }

    const UK2Node_ComponentBoundEvent* ExistingEvent = FKismetEditorUtilities::FindBoundEventForComponent(
        WidgetBlueprint,
        FName(TEXT("OnClicked")),
        ComponentProperty->GetFName());

    if (!ExistingEvent)
    {
        FKismetEditorUtilities::CreateNewBoundEventForClass(UButton::StaticClass(), FName(TEXT("OnClicked")), WidgetBlueprint, ComponentProperty);
        ExistingEvent = FKismetEditorUtilities::FindBoundEventForComponent(
            WidgetBlueprint,
            FName(TEXT("OnClicked")),
            ComponentProperty->GetFName());
    }

    UK2Node_ComponentBoundEvent* EventNode = const_cast<UK2Node_ComponentBoundEvent*>(ExistingEvent);
    if (!EventNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create UK2Node_ComponentBoundEvent for Button.OnClicked"));
    }

    FString PrintString;
    if (!Params->TryGetStringField(TEXT("print_string"), PrintString))
    {
        PrintString = TEXT("Button Clicked");
    }

    UK2Node_CallFunction* PrintNode = nullptr;
    if (!PrintString.IsEmpty())
    {
        UEdGraph* Graph = EventNode->GetGraph();
        UFunction* PrintFunction = UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString));
        PrintNode = FEpicUnrealMCPCommonUtils::CreateFunctionCallNode(
            Graph,
            PrintFunction,
            FVector2D(EventNode->NodePosX + 360.0, EventNode->NodePosY));

        if (PrintNode)
        {
            if (UEdGraphPin* InStringPin = PrintNode->FindPin(TEXT("InString")))
            {
                InStringPin->DefaultValue = PrintString;
            }
            if (UEdGraphPin* PrintToLogPin = PrintNode->FindPin(TEXT("bPrintToLog")))
            {
                PrintToLogPin->DefaultValue = TEXT("true");
            }
            if (UEdGraphPin* SourceExec = EventNode->FindPin(UEdGraphSchema_K2::PN_Then))
            {
                if (UEdGraphPin* TargetExec = PrintNode->FindPin(UEdGraphSchema_K2::PN_Execute))
                {
                    SourceExec->MakeLinkTo(TargetExec);
                }
            }
        }
    }

    MarkWidgetBlueprintModified(WidgetBlueprint, true);
    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("asset_path"), BlueprintPath);
    Response->SetStringField(TEXT("button_name"), ButtonName);
    Response->SetStringField(TEXT("delegate_name"), TEXT("OnClicked"));
    Response->SetStringField(TEXT("event_node_class"), EventNode->GetClass()->GetName());
    Response->SetStringField(TEXT("event_node_guid"), EventNode->NodeGuid.ToString());
    if (PrintNode)
    {
        Response->SetStringField(TEXT("print_node_guid"), PrintNode->NodeGuid.ToString());
        Response->SetStringField(TEXT("print_string"), PrintString);
    }
    CompileWidgetBlueprint(WidgetBlueprint, Response);
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleCreateWidgetAnimation(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    FString Error;
    UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(Params, BlueprintPath, Error);
    if (!WidgetBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString AnimationName;
    if (!Params->TryGetStringField(TEXT("animation_name"), AnimationName))
    {
        Params->TryGetStringField(TEXT("name"), AnimationName);
    }
    if (AnimationName.IsEmpty())
    {
        AnimationName = TEXT("MCP_WidgetAnimation");
    }

    for (UWidgetAnimation* ExistingAnimation : WidgetBlueprint->Animations)
    {
        if (ExistingAnimation && ExistingAnimation->GetName().Equals(AnimationName, ESearchCase::IgnoreCase))
        {
            TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
            Response->SetBoolField(TEXT("already_exists"), true);
            Response->SetStringField(TEXT("asset_path"), BlueprintPath);
            Response->SetStringField(TEXT("animation_name"), ExistingAnimation->GetName());
            return Response;
        }
    }

    UWidgetAnimation* Animation = NewObject<UWidgetAnimation>(WidgetBlueprint, FName(*AnimationName), RF_Transactional);
    if (!Animation)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to allocate UWidgetAnimation"));
    }
    Animation->SetDisplayLabel(AnimationName);
    Animation->MovieScene = NewObject<UMovieScene>(Animation, NAME_None, RF_Transactional);
    WidgetBlueprint->Animations.Add(Animation);
    WidgetBlueprint->OnVariableAdded(Animation->GetFName());
    MarkWidgetBlueprintModified(WidgetBlueprint, true);

    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("asset_path"), BlueprintPath);
    Response->SetStringField(TEXT("animation_name"), Animation->GetName());
    Response->SetBoolField(TEXT("has_movie_scene"), Animation->MovieScene != nullptr);
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleCompileWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    FString Error;
    UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(Params, BlueprintPath, Error);
    if (!WidgetBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("asset_path"), BlueprintPath);
    CompileWidgetBlueprint(WidgetBlueprint, Response);
    if (!Response->GetBoolField(TEXT("compiled_success")))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Widget Blueprint compilation failed. Inspect compile_status and Blueprint compiler output log."));
    }
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleInspectWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    FString Error;
    UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(Params, BlueprintPath, Error);
    if (!WidgetBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("asset_path"), BlueprintPath);
    Response->SetStringField(TEXT("name"), WidgetBlueprint->GetName());
    Response->SetStringField(TEXT("parent_class"), WidgetBlueprint->ParentClass ? WidgetBlueprint->ParentClass->GetPathName() : TEXT(""));
    Response->SetStringField(TEXT("compile_status"), BlueprintStatusToString(WidgetBlueprint->Status));
    Response->SetBoolField(TEXT("compiled_success"), WidgetBlueprint->Status != BS_Error);

    TArray<TSharedPtr<FJsonValue>> WidgetsJson;
    TArray<TSharedPtr<FJsonValue>> NamesJson;
    if (WidgetBlueprint->WidgetTree)
    {
        if (WidgetBlueprint->WidgetTree->RootWidget)
        {
            Response->SetStringField(TEXT("root_widget"), WidgetBlueprint->WidgetTree->RootWidget->GetName());
        }
        WidgetBlueprint->WidgetTree->ForEachWidget([&WidgetsJson, &NamesJson](UWidget* Widget)
        {
            if (!Widget)
            {
                return;
            }
            TSharedPtr<FJsonObject> WidgetJson = MakeShared<FJsonObject>();
            WidgetJson->SetStringField(TEXT("name"), Widget->GetName());
            WidgetJson->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
            WidgetJson->SetBoolField(TEXT("is_variable"), Widget->bIsVariable);
            WidgetJson->SetStringField(TEXT("parent_name"), Widget->GetParent() ? Widget->GetParent()->GetName() : TEXT(""));
            AddSlotPropertiesJson(WidgetJson, Widget->Slot);
            WidgetsJson.Add(MakeShared<FJsonValueObject>(WidgetJson));
            NamesJson.Add(MakeShared<FJsonValueString>(Widget->GetName()));
        });
    }
    Response->SetArrayField(TEXT("widgets"), WidgetsJson);
    Response->SetArrayField(TEXT("all_widget_names"), NamesJson);

    TArray<TSharedPtr<FJsonValue>> EventNodesJson;
    TArray<UK2Node_ComponentBoundEvent*> BoundEventNodes;
    FBlueprintEditorUtils::GetAllNodesOfClass(WidgetBlueprint, BoundEventNodes);
    for (UK2Node_ComponentBoundEvent* Node : BoundEventNodes)
    {
        if (!Node)
        {
            continue;
        }
        TSharedPtr<FJsonObject> NodeJson = MakeShared<FJsonObject>();
        NodeJson->SetStringField(TEXT("class"), Node->GetClass()->GetName());
        NodeJson->SetStringField(TEXT("delegate_name"), Node->DelegatePropertyName.ToString());
        NodeJson->SetStringField(TEXT("component_name"), Node->GetComponentPropertyName().ToString());
        NodeJson->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString());
        EventNodesJson.Add(MakeShared<FJsonValueObject>(NodeJson));
    }
    Response->SetArrayField(TEXT("bound_event_nodes"), EventNodesJson);
    Response->SetNumberField(TEXT("animation_count"), WidgetBlueprint->Animations.Num());
    Response->SetNumberField(TEXT("binding_count"), WidgetBlueprint->Bindings.Num());
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleAddWidgetToViewport(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    FString Error;
    UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(Params, BlueprintPath, Error);
    if (!WidgetBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }
    CompileWidgetBlueprint(WidgetBlueprint);

    UClass* WidgetClass = WidgetBlueprint->GeneratedClass;
    if (!WidgetClass || !WidgetClass->IsChildOf(UUserWidget::StaticClass()))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Widget Blueprint does not have a valid generated UUserWidget class"));
    }

    UWorld* World = GetUMGRuntimeWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor or PIE world available for CreateWidget"));
    }

    FString InstanceName;
    if (!Params->TryGetStringField(TEXT("instance_name"), InstanceName) || InstanceName.IsEmpty())
    {
        InstanceName = WidgetBlueprint->GetName();
    }
    double ZOrder = 0.0;
    Params->TryGetNumberField(TEXT("z_order"), ZOrder);

    APlayerController* PlayerController = GetUMGPlayerController(World);
    UUserWidget* Instance = PlayerController
        ? CreateWidget<UUserWidget>(PlayerController, WidgetClass, FName(*InstanceName))
        : CreateWidget<UUserWidget>(World, WidgetClass, FName(*InstanceName));

    if (!Instance)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("CreateWidget returned null"));
    }

    Instance->AddToViewport(static_cast<int32>(ZOrder));
    GUMGRuntimeWidgetInstances.Add(InstanceName, Instance);

    bool bShowMouseCursor = false;
    if (Params->TryGetBoolField(TEXT("show_mouse_cursor"), bShowMouseCursor) && PlayerController)
    {
        PlayerController->bShowMouseCursor = bShowMouseCursor;
    }

    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("asset_path"), BlueprintPath);
    Response->SetStringField(TEXT("instance_name"), InstanceName);
    Response->SetStringField(TEXT("world_name"), World->GetName());
    Response->SetBoolField(TEXT("pie_world"), GEditor && GEditor->PlayWorld == World);
    Response->SetNumberField(TEXT("z_order"), static_cast<int32>(ZOrder));
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleRemoveWidgetFromParent(const TSharedPtr<FJsonObject>& Params)
{
    FString InstanceName;
    if (!Params->TryGetStringField(TEXT("instance_name"), InstanceName) || InstanceName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("instance_name is required"));
    }

    UUserWidget* Instance = FindRuntimeWidgetInstance(InstanceName);
    if (!Instance)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Runtime widget instance not found: %s"), *InstanceName));
    }

    Instance->RemoveFromParent();
    GUMGRuntimeWidgetInstances.Remove(InstanceName);

    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("instance_name"), InstanceName);
    Response->SetBoolField(TEXT("removed_from_parent"), true);
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleClickWidgetButton(const TSharedPtr<FJsonObject>& Params)
{
    FString InstanceName;
    if (!Params->TryGetStringField(TEXT("instance_name"), InstanceName) || InstanceName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("instance_name is required"));
    }
    FString ButtonName;
    if (!Params->TryGetStringField(TEXT("button_name"), ButtonName) || ButtonName.IsEmpty())
    {
        Params->TryGetStringField(TEXT("widget_name"), ButtonName);
    }
    if (ButtonName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("button_name is required"));
    }

    UUserWidget* Instance = FindRuntimeWidgetInstance(InstanceName);
    if (!Instance || !Instance->WidgetTree)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Runtime widget instance not found or has no WidgetTree: %s"), *InstanceName));
    }

    UButton* Button = Cast<UButton>(Instance->WidgetTree->FindWidget(FName(*ButtonName)));
    if (!Button)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Button '%s' not found in runtime widget instance '%s'"), *ButtonName, *InstanceName));
    }

    Button->OnClicked.Broadcast();

    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("instance_name"), InstanceName);
    Response->SetStringField(TEXT("button_name"), ButtonName);
    Response->SetBoolField(TEXT("clicked"), true);
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleSetUIInputMode(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetUMGRuntimeWorld();
    APlayerController* PlayerController = GetUMGPlayerController(World);
    if (!PlayerController)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No PlayerController available. Start PIE before setting runtime input mode."));
    }

    FString Mode;
    if (!Params->TryGetStringField(TEXT("input_mode"), Mode))
    {
        Params->TryGetStringField(TEXT("mode"), Mode);
    }
    if (Mode.Equals(TEXT("ui_only"), ESearchCase::IgnoreCase) || Mode.Equals(TEXT("UIOnly"), ESearchCase::IgnoreCase))
    {
        FInputModeUIOnly InputMode;
        PlayerController->SetInputMode(InputMode);
        Mode = TEXT("ui_only");
    }
    else if (Mode.Equals(TEXT("game_and_ui"), ESearchCase::IgnoreCase) || Mode.Equals(TEXT("GameAndUI"), ESearchCase::IgnoreCase))
    {
        FInputModeGameAndUI InputMode;
        PlayerController->SetInputMode(InputMode);
        Mode = TEXT("game_and_ui");
    }
    else
    {
        FInputModeGameOnly InputMode;
        PlayerController->SetInputMode(InputMode);
        Mode = TEXT("game_only");
    }

    bool bShowMouseCursor = false;
    if (Params->TryGetBoolField(TEXT("show_mouse_cursor"), bShowMouseCursor))
    {
        PlayerController->bShowMouseCursor = bShowMouseCursor;
    }

    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("input_mode"), Mode);
    Response->SetBoolField(TEXT("show_mouse_cursor"), PlayerController->bShowMouseCursor);
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleSetMouseCursorVisible(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetUMGRuntimeWorld();
    APlayerController* PlayerController = GetUMGPlayerController(World);
    if (!PlayerController)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No PlayerController available. Start PIE before setting mouse cursor visibility."));
    }

    bool bVisible = true;
    Params->TryGetBoolField(TEXT("visible"), bVisible);
    PlayerController->bShowMouseCursor = bVisible;

    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetBoolField(TEXT("visible"), bVisible);
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleCreateUITemplate(const TSharedPtr<FJsonObject>& Params)
{
    FString TemplateName;
    if (!Params->TryGetStringField(TEXT("template_name"), TemplateName))
    {
        Params->TryGetStringField(TEXT("template"), TemplateName);
    }
    if (TemplateName.IsEmpty())
    {
        TemplateName = TEXT("main_menu");
    }

    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        AssetPath = FString::Printf(TEXT("/Game/UI/WBP_%s"), *TemplateName.Replace(TEXT(" "), TEXT("_")));
    }
    AssetPath = NormalizeWidgetBlueprintPath(AssetPath);

    TSharedPtr<FJsonObject> CreateParams = MakeShared<FJsonObject>();
    CreateParams->SetStringField(TEXT("asset_path"), AssetPath);
    if (Params->HasField(TEXT("common_ui")))
    {
        bool bCommonUI = false;
        Params->TryGetBoolField(TEXT("common_ui"), bCommonUI);
        CreateParams->SetBoolField(TEXT("common_ui"), bCommonUI);
    }
    TSharedPtr<FJsonObject> CreateResult = HandleCreateWidgetBlueprint(CreateParams);
    if (!CreateResult->GetBoolField(TEXT("success")))
    {
        return CreateResult;
    }

    HandleAddWidgetToWidgetBlueprint(MakeAddWidgetParams(AssetPath, TEXT("CanvasPanel"), TEXT("RootCanvas")));

    const bool bMainMenu = TemplateName.Equals(TEXT("main_menu"), ESearchCase::IgnoreCase) || TemplateName.Equals(TEXT("main menu"), ESearchCase::IgnoreCase) || TemplateName.Equals(TEXT("title"), ESearchCase::IgnoreCase);
    if (bMainMenu)
    {
        HandleAddWidgetToWidgetBlueprint(MakeAddWidgetParams(AssetPath, TEXT("Button"), TEXT("StartButton"), TEXT("RootCanvas"), true));
        HandleAddWidgetToWidgetBlueprint(MakeAddWidgetParams(AssetPath, TEXT("TextBlock"), TEXT("StartText"), TEXT("StartButton"), false));
        HandleSetWidgetText(MakeSetTextParams(AssetPath, TEXT("StartText"), TEXT("Start")));
        HandleSetWidgetSlotProperties(MakeSlotParams(AssetPath, TEXT("StartButton"), {0.5, 0.5, 0.5, 0.5}, {0.0, 0.0}, {240.0, 72.0}, {0.5, 0.5}));
        TSharedPtr<FJsonObject> BindParams = MakeTemplateParams(AssetPath);
        BindParams->SetStringField(TEXT("button_name"), TEXT("StartButton"));
        BindParams->SetStringField(TEXT("print_string"), TEXT("Start Button Clicked"));
        HandleBindWidgetButtonOnClicked(BindParams);
    }
    else if (TemplateName.Equals(TEXT("settings_menu"), ESearchCase::IgnoreCase) || TemplateName.Equals(TEXT("settings menu"), ESearchCase::IgnoreCase))
    {
        HandleAddWidgetToWidgetBlueprint(MakeAddWidgetParams(AssetPath, TEXT("VerticalBox"), TEXT("SettingsList"), TEXT("RootCanvas"), false));
        HandleAddWidgetToWidgetBlueprint(MakeAddWidgetParams(AssetPath, TEXT("Slider"), TEXT("VolumeSlider"), TEXT("SettingsList"), true));
        HandleAddWidgetToWidgetBlueprint(MakeAddWidgetParams(AssetPath, TEXT("CheckBox"), TEXT("FullscreenCheckBox"), TEXT("SettingsList"), true));
        HandleAddWidgetToWidgetBlueprint(MakeAddWidgetParams(AssetPath, TEXT("ComboBoxString"), TEXT("QualityComboBox"), TEXT("SettingsList"), true));
    }
    else if (TemplateName.Equals(TEXT("dialogue_ui"), ESearchCase::IgnoreCase) || TemplateName.Equals(TEXT("dialogue ui"), ESearchCase::IgnoreCase))
    {
        HandleAddWidgetToWidgetBlueprint(MakeAddWidgetParams(AssetPath, TEXT("Border"), TEXT("DialoguePanel"), TEXT("RootCanvas"), false));
        HandleAddWidgetToWidgetBlueprint(MakeAddWidgetParams(AssetPath, TEXT("TextBlock"), TEXT("DialogueText"), TEXT("DialoguePanel"), true));
        HandleSetWidgetText(MakeSetTextParams(AssetPath, TEXT("DialogueText"), TEXT("Dialogue")));
    }
    else if (TemplateName.Equals(TEXT("inventory_ui"), ESearchCase::IgnoreCase) || TemplateName.Equals(TEXT("inventory ui"), ESearchCase::IgnoreCase))
    {
        HandleAddWidgetToWidgetBlueprint(MakeAddWidgetParams(AssetPath, TEXT("ScrollBox"), TEXT("InventoryScrollBox"), TEXT("RootCanvas"), true));
        HandleAddWidgetToWidgetBlueprint(MakeAddWidgetParams(AssetPath, TEXT("UniformGridPanel"), TEXT("InventoryGrid"), TEXT("InventoryScrollBox"), true));
    }
    else
    {
        HandleAddWidgetToWidgetBlueprint(MakeAddWidgetParams(AssetPath, TEXT("VerticalBox"), TEXT("MenuList"), TEXT("RootCanvas"), false));
        HandleAddWidgetToWidgetBlueprint(MakeAddWidgetParams(AssetPath, TEXT("TextBlock"), TEXT("TitleText"), TEXT("MenuList"), true));
        HandleSetWidgetText(MakeSetTextParams(AssetPath, TEXT("TitleText"), TemplateName));
        HandleAddWidgetToWidgetBlueprint(MakeAddWidgetParams(AssetPath, TEXT("Button"), TEXT("PrimaryButton"), TEXT("MenuList"), true));
    }

    TSharedPtr<FJsonObject> CompileParams = MakeTemplateParams(AssetPath);
    TSharedPtr<FJsonObject> CompileResult = HandleCompileWidgetBlueprint(CompileParams);
    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("asset_path"), AssetPath);
    Response->SetStringField(TEXT("template_name"), TemplateName);
    Response->SetBoolField(TEXT("compiled_success"), CompileResult->GetBoolField(TEXT("compiled_success")));
    Response->SetStringField(TEXT("compile_status"), CompileResult->GetStringField(TEXT("compile_status")));
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleCreateWidgetInstance(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    FString Error;
    UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(Params, BlueprintPath, Error);
    if (!WidgetBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }
    CompileWidgetBlueprint(WidgetBlueprint);

    UClass* WidgetClass = WidgetBlueprint->GeneratedClass;
    if (!WidgetClass || !WidgetClass->IsChildOf(UUserWidget::StaticClass()))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Widget Blueprint does not have a valid generated UUserWidget class"));
    }

    UWorld* World = GetUMGRuntimeWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor or PIE world available for CreateWidget"));
    }

    FString InstanceName;
    if (!Params->TryGetStringField(TEXT("instance_name"), InstanceName) || InstanceName.IsEmpty())
    {
        InstanceName = WidgetBlueprint->GetName();
    }

    APlayerController* PlayerController = GetUMGPlayerController(World);
    UUserWidget* Instance = PlayerController
        ? CreateWidget<UUserWidget>(PlayerController, WidgetClass, FName(*InstanceName))
        : CreateWidget<UUserWidget>(World, WidgetClass, FName(*InstanceName));

    if (!Instance)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("CreateWidget returned null"));
    }

    GUMGRuntimeWidgetInstances.Add(InstanceName, Instance);

    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("asset_path"), BlueprintPath);
    Response->SetStringField(TEXT("instance_name"), InstanceName);
    Response->SetStringField(TEXT("world_name"), World->GetName());
    Response->SetBoolField(TEXT("pie_world"), GEditor && GEditor->PlayWorld == World);
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUMGCommands::HandleBindWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    FString Error;
    UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(Params, BlueprintPath, Error);
    if (!WidgetBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString WidgetName;
    Params->TryGetStringField(TEXT("widget_name"), WidgetName);
    UWidget* Widget = FindWidgetByName(WidgetBlueprint, WidgetName, Error);
    if (!Widget)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName) || PropertyName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("property_name is required. Examples: Text for TextBlock, Percent for ProgressBar."));
    }

    FString FunctionName;
    Params->TryGetStringField(TEXT("function_name"), FunctionName);
    FString SourceProperty;
    Params->TryGetStringField(TEXT("source_property"), SourceProperty);
    FString BindingKind;
    Params->TryGetStringField(TEXT("binding_kind"), BindingKind);

    if (FunctionName.IsEmpty() && SourceProperty.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("function_name or source_property is required for a UMG property binding"));
    }

    FProperty* TargetProperty = Widget->GetClass()->FindPropertyByName(FName(*PropertyName));
    if (!TargetProperty)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Widget '%s' does not expose bindable property '%s'"), *WidgetName, *PropertyName));
    }

    if (!Widget->bIsVariable)
    {
        Widget->bIsVariable = true;
        WidgetBlueprint->OnVariableAdded(Widget->GetFName());
    }

    FDelegateEditorBinding NewBinding;
    NewBinding.ObjectName = WidgetName;
    NewBinding.PropertyName = FName(*PropertyName);
    if (!FunctionName.IsEmpty())
    {
        NewBinding.FunctionName = FName(*FunctionName);
        NewBinding.Kind = EBindingKind::Function;
    }
    else
    {
        NewBinding.SourceProperty = FName(*SourceProperty);
        NewBinding.Kind = EBindingKind::Property;
    }

    WidgetBlueprint->Bindings.RemoveAll([&NewBinding](const FDelegateEditorBinding& Existing)
    {
        return Existing.ObjectName == NewBinding.ObjectName && Existing.PropertyName == NewBinding.PropertyName;
    });
    WidgetBlueprint->Bindings.Add(NewBinding);

    MarkWidgetBlueprintModified(WidgetBlueprint, true);
    TSharedPtr<FJsonObject> Response = MakeSuccessResponse();
    Response->SetStringField(TEXT("asset_path"), BlueprintPath);
    Response->SetStringField(TEXT("widget_name"), WidgetName);
    Response->SetStringField(TEXT("property_name"), PropertyName);
    Response->SetStringField(TEXT("binding_kind"), NewBinding.Kind == EBindingKind::Function ? TEXT("Function") : TEXT("Property"));
    if (!FunctionName.IsEmpty())
    {
        Response->SetStringField(TEXT("function_name"), FunctionName);
    }
    if (!SourceProperty.IsEmpty())
    {
        Response->SetStringField(TEXT("source_property"), SourceProperty);
    }
    CompileWidgetBlueprint(WidgetBlueprint, Response);
    return Response;
}
