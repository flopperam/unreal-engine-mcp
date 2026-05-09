#include "VTHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "VTGameState.h"

void UVTHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HUDPanel"));
	Panel->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.04f, 0.72f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
	PanelSlot->SetPosition(FVector2D(24.0f, 24.0f));
	PanelSlot->SetSize(FVector2D(330.0f, 130.0f));

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HUDStack"));
	Panel->SetContent(Stack);

	RemainingText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RemainingText"));
	RemainingText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.95f, 1.0f, 1.0f)));
	RemainingText->SetText(FText::FromString(TEXT("Cores: 0 / 3")));
	Stack->AddChildToVerticalBox(RemainingText);

	ProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("CoreProgress"));
	ProgressBar->SetPercent(0.0f);
	ProgressBar->SetFillColorAndOpacity(FLinearColor(0.0f, 0.75f, 1.0f, 1.0f));
	if (UVerticalBoxSlot* ProgressSlot = Stack->AddChildToVerticalBox(ProgressBar))
	{
		ProgressSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 8.0f));
	}

	StateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StateText"));
	StateText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.28f, 1.0f)));
	StateText->SetText(FText::FromString(TEXT("Gate locked")));
	Stack->AddChildToVerticalBox(StateText);

	TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimerText"));
	TimerText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.82f, 0.9f, 1.0f)));
	TimerText->SetText(FText::FromString(TEXT("Time: 0.0")));
	Stack->AddChildToVerticalBox(TimerText);
}

void UVTHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (const UWorld* World = GetWorld())
	{
		if (const AVTGameState* VTGameState = World->GetGameState<AVTGameState>())
		{
			SetHUDValues(VTGameState->CollectedCores, VTGameState->TotalCores, VTGameState->bGateUnlocked, VTGameState->GetElapsedTime());
		}
	}
}

void UVTHUDWidget::SetHUDValues(int32 CollectedCores, int32 TotalCores, bool bGateUnlocked, float ElapsedSeconds)
{
	const int32 Remaining = FMath::Max(0, TotalCores - CollectedCores);
	if (RemainingText)
	{
		RemainingText->SetText(FText::Format(
			NSLOCTEXT("VerticalTest", "RemainingCoresFormat", "Cores: {0} / {1}  Remaining: {2}"),
			FText::AsNumber(CollectedCores),
			FText::AsNumber(TotalCores),
			FText::AsNumber(Remaining)));
	}
	if (ProgressBar)
	{
		ProgressBar->SetPercent(TotalCores > 0 ? static_cast<float>(CollectedCores) / static_cast<float>(TotalCores) : 0.0f);
	}
	if (StateText)
	{
		StateText->SetText(bGateUnlocked
			? NSLOCTEXT("VerticalTest", "GateUnlocked", "Gate unlocked")
			: NSLOCTEXT("VerticalTest", "GateLocked", "Gate locked"));
		StateText->SetColorAndOpacity(FSlateColor(bGateUnlocked ? FLinearColor(0.2f, 1.0f, 0.45f, 1.0f) : FLinearColor(1.0f, 0.85f, 0.28f, 1.0f)));
	}
	if (TimerText)
	{
		TimerText->SetText(FText::Format(
			NSLOCTEXT("VerticalTest", "TimerFormat", "Time: {0}"),
			FText::AsNumber(ElapsedSeconds)));
	}
}
