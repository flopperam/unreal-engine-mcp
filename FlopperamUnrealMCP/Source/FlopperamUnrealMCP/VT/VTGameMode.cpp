#include "VTGameMode.h"

#include "EngineUtils.h"
#include "VTCharacter.h"
#include "VTCorePickup.h"
#include "VTClearScreenWidget.h"
#include "VTGameState.h"
#include "VTHUDWidget.h"
#include "VTPauseMenuWidget.h"
#include "VTPlayerController.h"

AVTGameMode::AVTGameMode()
{
	GameStateClass = AVTGameState::StaticClass();
	PlayerControllerClass = AVTPlayerController::StaticClass();
	DefaultPawnClass = AVTCharacter::StaticClass();
	HUDWidgetClass = UVTHUDWidget::StaticClass();
	PauseWidgetClass = UVTPauseMenuWidget::StaticClass();
	ClearScreenWidgetClass = UVTClearScreenWidget::StaticClass();
}

void AVTGameMode::BeginPlay()
{
	Super::BeginPlay();

	int32 CoreCount = 0;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AVTCorePickup> It(World); It; ++It)
		{
			++CoreCount;
		}
	}

	if (AVTGameState* VTGameState = GetGameState<AVTGameState>())
	{
		VTGameState->ConfigureRun(CoreCount > 0 ? CoreCount : ExpectedCoreCount);
	}
}
