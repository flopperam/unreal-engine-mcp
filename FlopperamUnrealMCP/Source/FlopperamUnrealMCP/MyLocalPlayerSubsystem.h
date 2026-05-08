#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MyLocalPlayerSubsystem.generated.h"

UCLASS()
class FLOPPERAMUNREALMCP_API UMyLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    UFUNCTION(BlueprintCallable, Category = "MyLocalPlayerSubsystem")
    void CustomFunction();
};
