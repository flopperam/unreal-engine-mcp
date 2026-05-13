#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGameplayHelpers.generated.h"

class AVTGameState;

UCLASS()
class FLOPPERAMUNREALMCP_API UVTGameplayHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Vertical Test", meta = (WorldContext = "WorldContextObject"))
	static AVTGameState* GetVTGameState(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Vertical Test")
	static FText FormatCoreCounter(int32 CollectedCores, int32 TotalCores);
};
