#include "VTPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "VTClearScreenWidget.h"
#include "VTGameMode.h"
#include "VTGameState.h"
#include "VTHUDWidget.h"
#include "VTPauseMenuWidget.h"

void AVTPlayerController::BeginPlay()
{
	Super::BeginPlay();
	ShowHUD();
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}

void AVTPlayerController::ShowHUD()
{
	if (HUDWidget)
	{
		return;
	}

	TSubclassOf<UVTHUDWidget> WidgetClass = UVTHUDWidget::StaticClass();
	if (const AVTGameMode* VTGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AVTGameMode>() : nullptr)
	{
		if (VTGameMode->HUDWidgetClass)
		{
			WidgetClass = VTGameMode->HUDWidgetClass;
		}
	}

	HUDWidget = CreateWidget<UVTHUDWidget>(this, WidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(0);
		RefreshHUD();
	}
}

void AVTPlayerController::RefreshHUD()
{
	if (!HUDWidget)
	{
		return;
	}

	if (const AVTGameState* VTGameState = GetWorld() ? GetWorld()->GetGameState<AVTGameState>() : nullptr)
	{
		HUDWidget->SetHUDValues(VTGameState->CollectedCores, VTGameState->TotalCores, VTGameState->bGateUnlocked, VTGameState->GetElapsedTime());
	}
}

void AVTPlayerController::TogglePauseMenu()
{
	const bool bIsPaused = IsPaused();
	if (bIsPaused)
	{
		if (PauseWidget)
		{
			PauseWidget->RemoveFromParent();
			PauseWidget = nullptr;
		}
		SetPause(false);
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
		return;
	}

	TSubclassOf<UVTPauseMenuWidget> WidgetClass = UVTPauseMenuWidget::StaticClass();
	if (const AVTGameMode* VTGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AVTGameMode>() : nullptr)
	{
		if (VTGameMode->PauseWidgetClass)
		{
			WidgetClass = VTGameMode->PauseWidgetClass;
		}
	}

	PauseWidget = CreateWidget<UVTPauseMenuWidget>(this, WidgetClass);
	if (PauseWidget)
	{
		PauseWidget->AddToViewport(20);
	}
	SetPause(true);
	bShowMouseCursor = true;
	SetInputMode(FInputModeUIOnly());
}

void AVTPlayerController::ShowClearScreen(float ClearTimeSeconds, int32 CoresCollected)
{
	if (!ClearScreenWidget)
	{
		TSubclassOf<UVTClearScreenWidget> WidgetClass = UVTClearScreenWidget::StaticClass();
		if (const AVTGameMode* VTGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AVTGameMode>() : nullptr)
		{
			if (VTGameMode->ClearScreenWidgetClass)
			{
				WidgetClass = VTGameMode->ClearScreenWidgetClass;
			}
		}

		ClearScreenWidget = CreateWidget<UVTClearScreenWidget>(this, WidgetClass);
		if (ClearScreenWidget)
		{
			ClearScreenWidget->AddToViewport(30);
		}
	}

	if (ClearScreenWidget)
	{
		ClearScreenWidget->SetClearResult(ClearTimeSeconds, CoresCollected);
	}

	bShowMouseCursor = true;
	SetInputMode(FInputModeUIOnly());
}
