#pragma once

#include "CoreMinimal.h"
#include "VTTypes.generated.h"

UENUM(BlueprintType)
enum class EVTGameState : uint8
{
	Collecting UMETA(DisplayName = "Collecting"),
	GateUnlocked UMETA(DisplayName = "Gate Unlocked"),
	Cleared UMETA(DisplayName = "Cleared")
};

USTRUCT(BlueprintType)
struct FVT_RunResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vertical Test")
	bool bCleared = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vertical Test")
	int32 CoresCollected = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vertical Test")
	int32 TotalCores = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vertical Test")
	float ClearTimeSeconds = 0.0f;
};
