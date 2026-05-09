#include "VTGameState.h"

#include "Kismet/GameplayStatics.h"
#include "VTSaveGame.h"

namespace
{
	const TCHAR* VTRunSlotName = TEXT("VT_ExtractionRoom_Result");
}

AVTGameState::AVTGameState()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AVTGameState::BeginPlay()
{
	Super::BeginPlay();
	StartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	OnCoreCountChanged.Broadcast(CollectedCores, TotalCores);
	OnGateUnlocked.Broadcast(bGateUnlocked);
}

void AVTGameState::ConfigureRun(int32 InTotalCores)
{
	TotalCores = FMath::Max(1, InTotalCores);
	CollectedCores = FMath::Clamp(CollectedCores, 0, TotalCores);
	bGateUnlocked = CollectedCores >= TotalCores;
	RunState = bGateUnlocked ? EVTGameState::GateUnlocked : EVTGameState::Collecting;
	OnCoreCountChanged.Broadcast(CollectedCores, TotalCores);
	OnGateUnlocked.Broadcast(bGateUnlocked);
}

bool AVTGameState::CollectCore()
{
	if (bCleared || CollectedCores >= TotalCores)
	{
		return false;
	}

	++CollectedCores;
	OnCoreCountChanged.Broadcast(CollectedCores, TotalCores);

	if (CollectedCores >= TotalCores)
	{
		bGateUnlocked = true;
		RunState = EVTGameState::GateUnlocked;
		OnGateUnlocked.Broadcast(true);
	}

	return true;
}

bool AVTGameState::CompleteRun()
{
	if (bCleared || !bGateUnlocked)
	{
		return false;
	}

	bCleared = true;
	RunState = EVTGameState::Cleared;
	ClearTimeSeconds = GetElapsedTime();
	SaveRunResult();
	OnRunCleared.Broadcast(ClearTimeSeconds);
	return true;
}

int32 AVTGameState::GetRemainingCores() const
{
	return FMath::Max(0, TotalCores - CollectedCores);
}

float AVTGameState::GetProgress() const
{
	return TotalCores > 0 ? static_cast<float>(CollectedCores) / static_cast<float>(TotalCores) : 0.0f;
}

float AVTGameState::GetElapsedTime() const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : StartTimeSeconds;
	return FMath::Max(0.0f, Now - StartTimeSeconds);
}

bool AVTGameState::SaveRunResult()
{
	UVTSaveGame* SaveGame = Cast<UVTSaveGame>(UGameplayStatics::CreateSaveGameObject(UVTSaveGame::StaticClass()));
	if (!SaveGame)
	{
		return false;
	}

	SaveGame->LastRun.bCleared = bCleared;
	SaveGame->LastRun.CoresCollected = CollectedCores;
	SaveGame->LastRun.TotalCores = TotalCores;
	SaveGame->LastRun.ClearTimeSeconds = ClearTimeSeconds;
	SaveGame->RunsCompleted = bCleared ? 1 : 0;
	SaveGame->BestClearTimeSeconds = bCleared ? ClearTimeSeconds : 0.0f;

	return UGameplayStatics::SaveGameToSlot(SaveGame, VTRunSlotName, 0);
}
