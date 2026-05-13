#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VTHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS(Blueprintable)
class FLOPPERAMUNREALMCP_API UVTHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Vertical Test|UI")
	void SetHUDValues(int32 CollectedCores, int32 TotalCores, bool bGateUnlocked, float ElapsedSeconds);

protected:
	UPROPERTY(Transient)
	UTextBlock* RemainingText;

	UPROPERTY(Transient)
	UTextBlock* StateText;

	UPROPERTY(Transient)
	UTextBlock* TimerText;

	UPROPERTY(Transient)
	UProgressBar* ProgressBar;
};
