#include "AdvancedNetworkCharacter.h"
#include "AdvancedNetworkComponent.h"
#include "DynamicBandwidthManager.h"
#include "Engine/World.h"

AAdvancedNetworkCharacter::AAdvancedNetworkCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);

	NetworkComponent = CreateDefaultSubobject<UAdvancedNetworkComponent>(TEXT("NetworkComponent"));
}

void AAdvancedNetworkCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Register with dynamic bandwidth manager
	if (HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			if (UDynamicBandwidthManager* BandwidthManager = World->GetSubsystem<UDynamicBandwidthManager>())
			{
				BandwidthManager->RegisterActor(this);
			}
		}
	}
}

void AAdvancedNetworkCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister from dynamic bandwidth manager
	if (HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			if (UDynamicBandwidthManager* BandwidthManager = World->GetSubsystem<UDynamicBandwidthManager>())
			{
				BandwidthManager->UnregisterActor(this);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AAdvancedNetworkCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AAdvancedNetworkCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AAdvancedNetworkCharacter::PerformAction(int32 ActionID)
{
	if (NetworkComponent)
	{
		NetworkComponent->Server_RequestAction(ActionID);
	}
}
