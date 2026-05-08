#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TestGameModeCPP.generated.h"

UCLASS()
class FLOPPERAMUNREALMCP_API ATestGameModeCPP : public AGameModeBase
{
    GENERATED_BODY()

public:
    ATestGameModeCPP();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
};
