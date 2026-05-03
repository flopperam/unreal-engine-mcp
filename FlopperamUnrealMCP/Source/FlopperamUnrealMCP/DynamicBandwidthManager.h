#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DynamicBandwidthManager.generated.h"

/**
 * Subsystem responsible for dynamically adjusting actor network update frequencies
 * based on spatial distance to players and server load, to optimize bandwidth.
 */
UCLASS()
class FLOPPERAMUNREALMCP_API UDynamicBandwidthManager : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// Registers an actor to be managed by the bandwidth manager
	UFUNCTION(BlueprintCallable, Category = "Network|Bandwidth")
	void RegisterActor(AActor* InActor);

	// Unregisters an actor
	UFUNCTION(BlueprintCallable, Category = "Network|Bandwidth")
	void UnregisterActor(AActor* InActor);

protected:
	// List of actors we are actively managing
	UPROPERTY(Transient)
	TArray<AActor*> ManagedActors;

	// How often to recalculate frequencies (in seconds)
	UPROPERTY(EditAnywhere, Category = "Optimization")
	float UpdateInterval = 1.0f;

	float TimeSinceLastUpdate = 0.0f;

	// Performs the actual calculation and application of update frequencies
	void OptimizeBandwidth();
};
