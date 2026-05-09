#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "VTTypes.h"
#include "VT/VTSaveGame.generated.h"

UCLASS(BlueprintType)
class FLOPPERAMUNREALMCP_API UVTSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vertical Test")
	FVT_RunResult LastRun;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vertical Test")
	float BestClearTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vertical Test")
	int32 RunsCompleted = 0;
};
