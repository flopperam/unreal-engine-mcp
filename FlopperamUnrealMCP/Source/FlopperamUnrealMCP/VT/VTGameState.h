#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "VTTypes.h"
#include "VT/VTGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVTCoreCountChangedSignature, int32, CollectedCores, int32, TotalCores);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVTGateUnlockedSignature, bool, bUnlocked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVTRunClearedSignature, float, ClearTimeSeconds);

UCLASS(BlueprintType)
class FLOPPERAMUNREALMCP_API AVTGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AVTGameState();

	virtual void BeginPlay() override;

	UPROPERTY(BlueprintAssignable, Category = "Vertical Test")
	FVTCoreCountChangedSignature OnCoreCountChanged;

	UPROPERTY(BlueprintAssignable, Category = "Vertical Test")
	FVTGateUnlockedSignature OnGateUnlocked;

	UPROPERTY(BlueprintAssignable, Category = "Vertical Test")
	FVTRunClearedSignature OnRunCleared;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Vertical Test")
	EVTGameState RunState = EVTGameState::Collecting;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Vertical Test")
	int32 CollectedCores = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Vertical Test")
	int32 TotalCores = 3;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Vertical Test")
	bool bGateUnlocked = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Vertical Test")
	bool bCleared = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Vertical Test")
	float StartTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Vertical Test")
	float ClearTimeSeconds = 0.0f;

	UFUNCTION(BlueprintCallable, Category = "Vertical Test")
	void ConfigureRun(int32 InTotalCores);

	UFUNCTION(BlueprintCallable, Category = "Vertical Test")
	bool CollectCore();

	UFUNCTION(BlueprintCallable, Category = "Vertical Test")
	bool CompleteRun();

	UFUNCTION(BlueprintPure, Category = "Vertical Test")
	int32 GetRemainingCores() const;

	UFUNCTION(BlueprintPure, Category = "Vertical Test")
	float GetProgress() const;

	UFUNCTION(BlueprintPure, Category = "Vertical Test")
	float GetElapsedTime() const;

	UFUNCTION(BlueprintCallable, Category = "Vertical Test")
	bool SaveRunResult();
};
