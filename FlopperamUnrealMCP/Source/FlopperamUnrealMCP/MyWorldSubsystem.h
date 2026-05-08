#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MyWorldSubsystem.generated.h"

UCLASS()
class FLOPPERAMUNREALMCP_API UMyWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    UFUNCTION(BlueprintCallable, Category = "MyWorldSubsystem")
    void CustomFunction();
};
