#include "AdvancedNetworkComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

UAdvancedNetworkComponent::UAdvancedNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	NetworkState = 0;
}

void UAdvancedNetworkComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UAdvancedNetworkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UAdvancedNetworkComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAdvancedNetworkComponent, NetworkState);
}

void UAdvancedNetworkComponent::OnRep_NetworkState()
{
	UE_LOG(LogTemp, Log, TEXT("NetworkState replicated: %d"), NetworkState);
}

bool UAdvancedNetworkComponent::Server_RequestAction_Validate(int32 ActionID)
{
	// Validation logic here (e.g., check if ActionID is within bounds)
	return true;
}

void UAdvancedNetworkComponent::Server_RequestAction_Implementation(int32 ActionID)
{
	if (!HasAuthority()) return;

	UE_LOG(LogTemp, Log, TEXT("Server processing action: %d"), ActionID);

	// Simulate success condition
	bool bSuccess = (ActionID > 0);

	if (bSuccess)
	{
		NetworkState = ActionID; // Replicate to clients
	}

	Client_ConfirmAction(ActionID, bSuccess);
}

void UAdvancedNetworkComponent::Client_ConfirmAction_Implementation(int32 ActionID, bool bSuccess)
{
	UE_LOG(LogTemp, Log, TEXT("Client confirmed action %d: %s"), ActionID, bSuccess ? TEXT("Success") : TEXT("Failure"));
}

void UAdvancedNetworkComponent::Multicast_PlayEffect_Implementation(FVector Location)
{
	UE_LOG(LogTemp, Log, TEXT("Multicast playing effect at: %s"), *Location.ToString());
	// Spawn particle system or sound here
}

bool UAdvancedNetworkComponent::HasAuthority() const
{
	AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}
