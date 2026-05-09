#include "VTClearScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

void UVTClearScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ClearPanel"));
	Panel->SetBrushColor(FLinearColor(0.0f, 0.05f, 0.08f, 0.88f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetPosition(FVector2D::ZeroVector);
	PanelSlot->SetSize(FVector2D(520.0f, 220.0f));

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ClearStack"));
	Panel->SetContent(Stack);

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ResultText"));
	ResultText->SetJustification(ETextJustify::Center);
	ResultText->SetColorAndOpacity(FSlateColor(FLinearColor(0.25f, 1.0f, 0.65f, 1.0f)));
	ResultText->SetText(NSLOCTEXT("VerticalTest", "ClearInitial", "Extraction Complete"));
	Stack->AddChildToVerticalBox(ResultText);
}

void UVTClearScreenWidget::SetClearResult(float ClearTimeSeconds, int32 CoresCollected)
{
	if (ResultText)
	{
		ResultText->SetText(FText::Format(
			NSLOCTEXT("VerticalTest", "ClearResultFormat", "Extraction Complete\nCores: {0}\nTime: {1}s"),
			FText::AsNumber(CoresCollected),
			FText::AsNumber(ClearTimeSeconds)));
	}
}
