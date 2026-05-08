#include "TestGameModeCPP.h"

ATestGameModeCPP::ATestGameModeCPP()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ATestGameModeCPP::BeginPlay()
{
    Super::BeginPlay();
}

void ATestGameModeCPP::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}
