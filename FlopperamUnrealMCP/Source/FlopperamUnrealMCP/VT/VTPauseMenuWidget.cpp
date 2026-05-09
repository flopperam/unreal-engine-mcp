#include "VTPauseMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Kismet/GameplayStatics.h"
#include "VTPlayerController.h"

namespace
{
	UButton* AddPauseButton(UWidgetTree* WidgetTree, UVerticalBox* Stack, const TCHAR* Name, const FText& Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(Name));
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(FString(Name) + TEXT("_Text")));
		Text->SetText(Label);
		Text->SetJustification(ETextJustify::Center);
		Button->SetContent(Text);
		Stack->AddChildToVerticalBox(Button);
		return Button;
	}
}

void UVTPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PausePanel"));
	Panel->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetPosition(FVector2D::ZeroVector);
	PanelSlot->SetSize(FVector2D(360.0f, 240.0f));

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PauseStack"));
	Panel->SetContent(Stack);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PauseTitle"));
	Title->SetText(NSLOCTEXT("VerticalTest", "Paused", "Paused"));
	Title->SetJustification(ETextJustify::Center);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.95f, 1.0f, 1.0f)));
	Stack->AddChildToVerticalBox(Title);

	UButton* ResumeButton = AddPauseButton(WidgetTree, Stack, TEXT("ResumeButton"), NSLOCTEXT("VerticalTest", "Resume", "Resume"));
	UButton* RestartButton = AddPauseButton(WidgetTree, Stack, TEXT("RestartButton"), NSLOCTEXT("VerticalTest", "Restart", "Restart"));
	UButton* QuitButton = AddPauseButton(WidgetTree, Stack, TEXT("QuitButton"), NSLOCTEXT("VerticalTest", "Quit", "Quit"));

	ResumeButton->OnClicked.AddDynamic(this, &UVTPauseMenuWidget::HandleResumeClicked);
	RestartButton->OnClicked.AddDynamic(this, &UVTPauseMenuWidget::HandleRestartClicked);
	QuitButton->OnClicked.AddDynamic(this, &UVTPauseMenuWidget::HandleQuitClicked);
}

void UVTPauseMenuWidget::HandleResumeClicked()
{
	if (AVTPlayerController* PC = GetOwningPlayer<AVTPlayerController>())
	{
		PC->TogglePauseMenu();
	}
}

void UVTPauseMenuWidget::HandleRestartClicked()
{
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGamePaused(World, false);
		UGameplayStatics::OpenLevel(World, FName(*World->GetName()));
	}
}

void UVTPauseMenuWidget::HandleQuitClicked()
{
	HandleResumeClicked();
}
