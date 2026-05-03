#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AdvancedNetworkComponent.generated.h"

/**
 * Component to handle advanced RPC dispatching and replication logic
 * for complex multiplayer interactions.
 */
UCLASS( ClassGroup=(Network), meta=(BlueprintSpawnableComponent) )
class FLOPPERAMUNREALMCP_API UAdvancedNetworkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAdvancedNetworkComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Example replicated state
	UPROPERTY(ReplicatedUsing = OnRep_NetworkState)
	int32 NetworkState;

	UFUNCTION()
	void OnRep_NetworkState();

	// Server RPC to request an action
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestAction(int32 ActionID);

	// Client RPC to confirm action
	UFUNCTION(Client, Reliable)
	void Client_ConfirmAction(int32 ActionID, bool bSuccess);

	// Broadcast RPC
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayEffect(FVector Location);

	// Ensures only the authoritative server executes this
	bool HasAuthority() const;
};
