#include "CognitiveAIController.h"
#include "Async/Async.h"
#include "Engine/World.h"

ACognitiveAIController::ACognitiveAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bIsProcessing = false;
}

void ACognitiveAIController::BeginPlay()
{
	Super::BeginPlay();
}

void ACognitiveAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SynchronizeCognitiveState();
}

void ACognitiveAIController::RequestCognitiveProcessing(const FString& EnvironmentalContext)
{
	if (bIsProcessing)
	{
		UE_LOG(LogTemp, Warning, TEXT("AI is already processing a cognitive task."));
		return;
	}

	bIsProcessing = true;
	RunAsyncLLMTask(EnvironmentalContext);
}

void ACognitiveAIController::RunAsyncLLMTask(const FString& Context)
{
	TWeakObjectPtr<ACognitiveAIController> WeakThis(this);

	// Launch an asynchronous background task to simulate heavy LLM processing
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakThis, Context]()
	{
		// SIMULATE HEAVY COMPUTATION (e.g., local LLM inference)
		FPlatformProcess::Sleep(2.0f); // Simulate a 2-second inference latency

		// Process context and formulate a decision
		FString SimulatedDecision = FString::Printf(TEXT("Processed Context: '%s' -> Action: Investigate"), *Context);

		if (ACognitiveAIController* StrongThis = WeakThis.Get())
		{
			// Safely push the result back to our pending queue
			FScopeLock Lock(&StrongThis->CognitiveMutex);
			StrongThis->PendingDecisions.Add(SimulatedDecision);
		}
	});
}

void ACognitiveAIController::SynchronizeCognitiveState()
{
	// Synchronize async results on the main thread
	TArray<FString> DecisionsToProcess;

	{
		FScopeLock Lock(&CognitiveMutex);
		if (PendingDecisions.Num() > 0)
		{
			DecisionsToProcess = PendingDecisions;
			PendingDecisions.Empty();
			bIsProcessing = false; // Free the AI to think again
		}
	}

	// Broadcast decisions on the main thread so Blueprints and physical states update safely
	for (const FString& Decision : DecisionsToProcess)
	{
		UE_LOG(LogTemp, Log, TEXT("Cognitive AI Decision Synced: %s"), *Decision);
		OnDecisionCompleted.Broadcast(Decision);

		// In a real system, you would apply physics, states, or behavior tree updates here
	}
}
