#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VTInteractable.generated.h"

UINTERFACE(BlueprintType)
class FLOPPERAMUNREALMCP_API UVTInteractable : public UInterface
{
	GENERATED_BODY()
};

class FLOPPERAMUNREALMCP_API IVTInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Vertical Test")
	void Interact(AActor* Interactor);
};
