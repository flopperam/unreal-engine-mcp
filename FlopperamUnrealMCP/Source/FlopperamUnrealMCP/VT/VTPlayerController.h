#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VT/VTPlayerController.generated.h"

class UVTClearScreenWidget;
class UVTHUDWidget;
class UVTPauseMenuWidget;

UCLASS(Blueprintable)
class FLOPPERAMUNREALMCP_API AVTPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Vertical Test|UI")
	void TogglePauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Vertical Test|UI")
	void ShowClearScreen(float ClearTimeSeconds, int32 CoresCollected);

	UFUNCTION(BlueprintCallable, Category = "Vertical Test|UI")
	void RefreshHUD();

protected:
	UPROPERTY(Transient)
	UVTHUDWidget* HUDWidget;

	UPROPERTY(Transient)
	UVTPauseMenuWidget* PauseWidget;

	UPROPERTY(Transient)
	UVTClearScreenWidget* ClearScreenWidget;

	void ShowHUD();
};
