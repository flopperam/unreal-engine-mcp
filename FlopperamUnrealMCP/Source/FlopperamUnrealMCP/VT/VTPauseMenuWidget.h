#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VTPauseMenuWidget.generated.h"

UCLASS(Blueprintable)
class FLOPPERAMUNREALMCP_API UVTPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

protected:
	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleRestartClicked();

	UFUNCTION()
	void HandleQuitClicked();
};
