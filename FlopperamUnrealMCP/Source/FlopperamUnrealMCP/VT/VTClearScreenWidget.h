#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VTClearScreenWidget.generated.h"

class UTextBlock;

UCLASS(Blueprintable)
class FLOPPERAMUNREALMCP_API UVTClearScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "Vertical Test|UI")
	void SetClearResult(float ClearTimeSeconds, int32 CoresCollected);

protected:
	UPROPERTY(Transient)
	UTextBlock* ResultText;
};
