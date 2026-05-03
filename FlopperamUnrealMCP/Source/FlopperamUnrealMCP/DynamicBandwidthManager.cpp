#include "DynamicBandwidthManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Actor.h"

void UDynamicBandwidthManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("DynamicBandwidthManager Initialized"));
}

void UDynamicBandwidthManager::Deinitialize()
{
	ManagedActors.Empty();
	Super::Deinitialize();
}

void UDynamicBandwidthManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Only run on the server
	if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}

	TimeSinceLastUpdate += DeltaTime;
	if (TimeSinceLastUpdate >= UpdateInterval)
	{
		OptimizeBandwidth();
		TimeSinceLastUpdate = 0.0f;
	}
}

TStatId UDynamicBandwidthManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDynamicBandwidthManager, STATGROUP_Tickables);
}

void UDynamicBandwidthManager::RegisterActor(AActor* InActor)
{
	if (InActor && !ManagedActors.Contains(InActor) && InActor->GetIsReplicated())
	{
		ManagedActors.Add(InActor);
	}
}

void UDynamicBandwidthManager::UnregisterActor(AActor* InActor)
{
	if (InActor)
	{
		ManagedActors.Remove(InActor);
	}
}

void UDynamicBandwidthManager::OptimizeBandwidth()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Gather all player pawn locations
	TArray<FVector> PlayerLocations;
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Iterator->Get();
		if (PC && PC->GetPawn())
		{
			PlayerLocations.Add(PC->GetPawn()->GetActorLocation());
		}
	}

	if (PlayerLocations.Num() == 0) return;

	// Remove invalid actors
	ManagedActors.RemoveAll([](AActor* Actor) { return !IsValid(Actor); });

	// Simulated server load factor (0.0 to 1.0)
	// In a real scenario, this would be derived from actual server tick rate or network saturation
	float ServerLoadFactor = 0.5f;

	for (AActor* Actor : ManagedActors)
	{
		float MinDistanceSq = MAX_flt;
		FVector ActorLoc = Actor->GetActorLocation();

		for (const FVector& PlayerLoc : PlayerLocations)
		{
			float DistSq = FVector::DistSquared(ActorLoc, PlayerLoc);
			if (DistSq < MinDistanceSq)
			{
				MinDistanceSq = DistSq;
			}
		}

		float MinDistance = FMath::Sqrt(MinDistanceSq);

		// Logic:
		// Base frequency is high. As distance increases, frequency drops.
		// If server load is high, all frequencies are scaled down.
		float TargetFrequency = 100.0f; // Max updates per second

		if (MinDistance > 10000.0f) // > 100 meters
		{
			TargetFrequency = 2.0f;
		}
		else if (MinDistance > 5000.0f) // > 50 meters
		{
			TargetFrequency = 10.0f;
		}
		else
		{
			TargetFrequency = 60.0f;
		}

		// Apply server load throttling
		TargetFrequency *= (1.0f - (ServerLoadFactor * 0.5f)); // Throttle up to 50% under max load

		// Ensure minimum frequency
		TargetFrequency = FMath::Max(1.0f, TargetFrequency);

		Actor->NetUpdateFrequency = TargetFrequency;
	}
}
