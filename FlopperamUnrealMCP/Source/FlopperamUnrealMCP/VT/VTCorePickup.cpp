#include "VTCorePickup.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "VTCharacter.h"
#include "VTGameState.h"

AVTCorePickup::AVTCorePickup()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerComponent = CreateDefaultSubobject<USphereComponent>(TEXT("PickupTrigger"));
	RootComponent = TriggerComponent;
	TriggerComponent->InitSphereRadius(115.0f);
	TriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerComponent->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerComponent->SetGenerateOverlapEvents(true);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(SphereMesh.Object);
		MeshComponent->SetRelativeScale3D(FVector(0.45f));
	}
}

void AVTCorePickup::BeginPlay()
{
	Super::BeginPlay();
	TriggerComponent->OnComponentBeginOverlap.AddDynamic(this, &AVTCorePickup::HandleOverlap);
	if (ActiveMaterial)
	{
		MeshComponent->SetMaterial(0, ActiveMaterial);
	}
}

bool AVTCorePickup::TryCollect(AActor* Collector)
{
	if (bCollected || !Collector || !Collector->IsA<AVTCharacter>())
	{
		return false;
	}

	AVTGameState* VTGameState = GetWorld() ? GetWorld()->GetGameState<AVTGameState>() : nullptr;
	if (!VTGameState || !VTGameState->CollectCore())
	{
		return false;
	}

	bCollected = true;
	if (CollectedMaterial)
	{
		MeshComponent->SetMaterial(0, CollectedMaterial);
	}
	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	return true;
}

void AVTCorePickup::Interact_Implementation(AActor* Interactor)
{
	TryCollect(Interactor);
}

void AVTCorePickup::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryCollect(OtherActor);
}
