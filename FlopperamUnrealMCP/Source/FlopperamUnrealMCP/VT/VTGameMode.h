#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VTGameMode.generated.h"

class UVTClearScreenWidget;
class UVTHUDWidget;
class UVTPauseMenuWidget;

UCLASS(Blueprintable)
class FLOPPERAMUNREALMCP_API AVTGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVTGameMode();

	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vertical Test")
	int32 ExpectedCoreCount = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vertical Test|UI")
	TSubclassOf<UVTHUDWidget> HUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vertical Test|UI")
	TSubclassOf<UVTPauseMenuWidget> PauseWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vertical Test|UI")
	TSubclassOf<UVTClearScreenWidget> ClearScreenWidgetClass;
};
