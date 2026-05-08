#include "MyWorldSubsystem.h"

void UMyWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UMyWorldSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

bool UMyWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return true;
}

void UMyWorldSubsystem::CustomFunction()
{
}
