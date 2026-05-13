#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VTInteractable.h"
#include "VTCorePickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class USoundBase;

UCLASS(Blueprintable)
class FLOPPERAMUNREALMCP_API AVTCorePickup : public AActor, public IVTInteractable
{
	GENERATED_BODY()

public:
	AVTCorePickup();

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vertical Test")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vertical Test")
	USphereComponent* TriggerComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vertical Test")
	UMaterialInterface* ActiveMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vertical Test")
	UMaterialInterface* CollectedMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vertical Test")
	USoundBase* PickupSound;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Vertical Test")
	bool bCollected = false;

	UFUNCTION(BlueprintCallable, Category = "Vertical Test")
	bool TryCollect(AActor* Collector);

	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
