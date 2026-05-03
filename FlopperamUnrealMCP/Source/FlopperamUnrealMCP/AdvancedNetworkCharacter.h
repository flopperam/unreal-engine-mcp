#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AdvancedNetworkCharacter.generated.h"

class UAdvancedNetworkComponent;

/**
 * Character class integrated with AdvancedNetworkComponent and DynamicBandwidthManager.
 */
UCLASS()
class FLOPPERAMUNREALMCP_API AAdvancedNetworkCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAdvancedNetworkCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network")
	UAdvancedNetworkComponent* NetworkComponent;

	// Request an action from the server
	UFUNCTION(BlueprintCallable, Category = "Network|Actions")
	void PerformAction(int32 ActionID);
};
