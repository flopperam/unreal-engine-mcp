#include "VTGameplayHelpers.h"

#include "Engine/World.h"
#include "VTGameState.h"

AVTGameState* UVTGameplayHelpers::GetVTGameState(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetGameState<AVTGameState>() : nullptr;
}

FText UVTGameplayHelpers::FormatCoreCounter(int32 CollectedCores, int32 TotalCores)
{
	return FText::Format(
		NSLOCTEXT("VerticalTest", "CoreCounterFormat", "{0} / {1} cores"),
		FText::AsNumber(CollectedCores),
		FText::AsNumber(TotalCores));
}
