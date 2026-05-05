#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Http.h"
#include "Json.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "Commands/EpicUnrealMCPBlueprintGraphCommands.h"
#include "Commands/EpicUnrealMCPMaterialCommands.h"
#include "Commands/EpicUnrealMCPProjectEditorCommands.h"
#include "Commands/EpicUnrealMCPContentBrowserCommands.h"
#include "Commands/EpicUnrealMCPAssetImportCommands.h"
#include "Commands/EpicUnrealMCPMeshEditingCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "MCPServerRunnable.h"
#include "EpicUnrealMCPBridge.generated.h"

/**
 * Editor subsystem for MCP Bridge
 * Handles communication between external tools and the Unreal Editor
 * through a TCP socket connection. Commands are received as JSON and
 * routed to appropriate command handlers.
 */
#if WITH_EDITOR

UCLASS()
class UNREALMCP_API UEpicUnrealMCPBridge : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UEpicUnrealMCPBridge();
	virtual ~UEpicUnrealMCPBridge();

	// UEditorSubsystem implementation
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Server functions
	void StartServer();
	void StopServer();
	bool IsRunning() const { return bIsRunning; }
	FIPv4Address GetServerAddress() const { return ServerAddress; }
	uint16 GetServerPort() const { return Port; }

	// Lazy actor index initialization
	void EnsureActorIndexInitialized();

	// Command execution
	FString ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

	// Actor index for O(1) lookup by name/mcp_id
	FActorIndex ActorIndex;

private:
	// Server state
	bool bIsRunning;
	FSocket* ListenerSocket;
	FSocket* ConnectionSocket;
	FRunnableThread* ServerThread;
	TUniquePtr<FMCPServerRunnable> Runnable;

	// Server configuration
	FIPv4Address ServerAddress;
	uint16 Port;

	// Command handler instances
	TSharedPtr<FEpicUnrealMCPEditorCommands> EditorCommands;
	TSharedPtr<FEpicUnrealMCPBlueprintCommands> BlueprintCommands;
	TSharedPtr<FEpicUnrealMCPBlueprintGraphCommands> BlueprintGraphCommands;
	TSharedPtr<FEpicUnrealMCPMaterialCommands> MaterialCommands;
	TSharedPtr<FEpicUnrealMCPProjectEditorCommands> ProjectEditorCommands;
	TSharedPtr<FEpicUnrealMCPContentBrowserCommands> ContentBrowserCommands;
	TSharedPtr<FEpicUnrealMCPAssetImportCommands> AssetImportCommands;
	TSharedPtr<FEpicUnrealMCPMeshEditingCommands> MeshEditingCommands;
};

#endif // WITH_EDITOR
