#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VTHazard.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class FLOPPERAMUNREALMCP_API AVTHazard : public AActor
{
	GENERATED_BODY()

public:
	AVTHazard();

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vertical Test")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vertical Test")
	UBoxComponent* TriggerComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vertical Test")
	float LaunchStrength = 450.0f;

protected:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
