#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ServerMeshManager.generated.h"

/**
 * Subsystem responsible for simulating dynamic server meshing.
 * It monitors spatial density and server load (e.g., number of actors in a region)
 * and can trigger simulated "micro-server" migrations when thresholds are exceeded.
 */
UCLASS()
class FLOPPERAMUNREALMCP_API UServerMeshManager : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// Checks regional density and triggers migration if necessary
	UFUNCTION(BlueprintCallable, Category = "Network|ServerMeshing")
	void EvaluateServerLoad();

protected:
	UPROPERTY(EditAnywhere, Category = "Meshing Configuration")
	int32 MaxActorsPerRegionThreshold = 50;

	UPROPERTY(EditAnywhere, Category = "Meshing Configuration")
	float SpatialGridSize = 5000.0f; // 50 meters per grid cell

	UPROPERTY(EditAnywhere, Category = "Meshing Configuration")
	float EvaluationInterval = 5.0f;

	float TimeSinceLastEval = 0.0f;

	// Triggers a simulated handover of a region to a new micro-server
	void TriggerMicroServerHandoff(const FIntVector& RegionCoord);
};
