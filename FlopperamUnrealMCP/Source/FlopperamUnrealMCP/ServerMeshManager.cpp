#include "ServerMeshManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

void UServerMeshManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("ServerMeshManager Initialized"));
}

void UServerMeshManager::Deinitialize()
{
	Super::Deinitialize();
}

void UServerMeshManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Server meshing evaluation only happens on the authoritative server
	if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}

	TimeSinceLastEval += DeltaTime;
	if (TimeSinceLastEval >= EvaluationInterval)
	{
		EvaluateServerLoad();
		TimeSinceLastEval = 0.0f;
	}
}

TStatId UServerMeshManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UServerMeshManager, STATGROUP_Tickables);
}

void UServerMeshManager::EvaluateServerLoad()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Map of spatial grid coordinates to actor count
	TMap<FIntVector, int32> SpatialDensityMap;

	// Iterate all actors (in a real scenario, this would be optimized, perhaps via physics grid or Octree)
	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		AActor* Actor = *ActorItr;
		if (Actor && Actor->GetIsReplicated())
		{
			FVector Loc = Actor->GetActorLocation();

			// Quantize location into grid coordinates
			FIntVector GridCoord(
				FMath::FloorToInt(Loc.X / SpatialGridSize),
				FMath::FloorToInt(Loc.Y / SpatialGridSize),
				FMath::FloorToInt(Loc.Z / SpatialGridSize)
			);

			int32& Count = SpatialDensityMap.FindOrAdd(GridCoord);
			Count++;
		}
	}

	// Identify overloaded regions
	for (const auto& Pair : SpatialDensityMap)
	{
		if (Pair.Value > MaxActorsPerRegionThreshold)
		{
			UE_LOG(LogTemp, Warning, TEXT("Server Meshing: High load detected in region (%d, %d, %d) with %d actors. Initiating handoff."),
				Pair.Key.X, Pair.Key.Y, Pair.Key.Z, Pair.Value);

			TriggerMicroServerHandoff(Pair.Key);
		}
	}
}

void UServerMeshManager::TriggerMicroServerHandoff(const FIntVector& RegionCoord)
{
	// SIMULATE DYNAMIC SERVER MESHING MIGRATION
	// In a complete implementation, this would:
	// 1. Request orchestration layer to spin up a new UE server instance
	// 2. Serialize all actors within the RegionCoord bounds
	// 3. Pause replication for these actors on the current server
	// 4. Send state to new server
	// 5. Transfer client connections authority for that specific bounding box
	// 6. Delete local authoritative copies and spawn proxy representations (if cross-server visibility is needed)

	UE_LOG(LogTemp, Log, TEXT("SIMULATION: Migrating spatial region (%d, %d, %d) to a new micro-server process."), RegionCoord.X, RegionCoord.Y, RegionCoord.Z);
}
