#include "VTExitGate.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "VTCharacter.h"
#include "VTGameState.h"
#include "VTPlayerController.h"

AVTExitGate::AVTExitGate()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateMesh"));
	RootComponent = MeshComponent;
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMesh.Object);
		MeshComponent->SetRelativeScale3D(FVector(0.35f, 2.2f, 2.0f));
	}

	TriggerComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("ExitTrigger"));
	TriggerComponent->SetupAttachment(RootComponent);
	TriggerComponent->SetBoxExtent(FVector(170.0f, 260.0f, 220.0f));
	TriggerComponent->SetRelativeLocation(FVector(120.0f, 0.0f, 0.0f));
	TriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerComponent->SetGenerateOverlapEvents(true);
}

void AVTExitGate::BeginPlay()
{
	Super::BeginPlay();
	TriggerComponent->OnComponentBeginOverlap.AddDynamic(this, &AVTExitGate::HandleOverlap);

	if (AVTGameState* VTGameState = GetWorld() ? GetWorld()->GetGameState<AVTGameState>() : nullptr)
	{
		VTGameState->OnGateUnlocked.AddDynamic(this, &AVTExitGate::HandleGateUnlocked);
		SetGateUnlocked(VTGameState->bGateUnlocked);
	}
	else
	{
		SetGateUnlocked(false);
	}
}

void AVTExitGate::SetGateUnlocked(bool bNewUnlocked)
{
	bUnlocked = bNewUnlocked;
	MeshComponent->SetCollisionEnabled(bUnlocked ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	if (UMaterialInterface* Material = bUnlocked ? UnlockedMaterial : LockedMaterial)
	{
		MeshComponent->SetMaterial(0, Material);
	}
}

void AVTExitGate::HandleGateUnlocked(bool bNewUnlocked)
{
	SetGateUnlocked(bNewUnlocked);
}

void AVTExitGate::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bUnlocked || !OtherActor || !OtherActor->IsA<AVTCharacter>())
	{
		return;
	}

	if (AVTGameState* VTGameState = GetWorld() ? GetWorld()->GetGameState<AVTGameState>() : nullptr)
	{
		if (VTGameState->CompleteRun())
		{
			if (AVTPlayerController* PC = Cast<AVTPlayerController>(Cast<APawn>(OtherActor)->GetController()))
			{
				PC->ShowClearScreen(VTGameState->ClearTimeSeconds, VTGameState->CollectedCores);
			}
		}
	}
}
