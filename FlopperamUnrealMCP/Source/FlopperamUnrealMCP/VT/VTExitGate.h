#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VTExitGate.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class FLOPPERAMUNREALMCP_API AVTExitGate : public AActor
{
	GENERATED_BODY()

public:
	AVTExitGate();

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vertical Test")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vertical Test")
	UBoxComponent* TriggerComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vertical Test")
	UMaterialInterface* LockedMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vertical Test")
	UMaterialInterface* UnlockedMaterial;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Vertical Test")
	bool bUnlocked = false;

	UFUNCTION(BlueprintCallable, Category = "Vertical Test")
	void SetGateUnlocked(bool bNewUnlocked);

protected:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleGateUnlocked(bool bNewUnlocked);
};
