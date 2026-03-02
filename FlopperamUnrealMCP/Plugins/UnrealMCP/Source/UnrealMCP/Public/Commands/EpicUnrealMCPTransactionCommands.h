#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Transaction / Undo MCP commands.
 *
 * Wraps Unreal's built-in transaction system:
 *   GEditor->BeginTransaction() / GEditor->EndTransaction()
 *   GEditor->UndoTransaction()  / GEditor->RedoTransaction()
 *
 * Transaction state is tracked so that mismatched begin/end calls
 * produce clear error codes rather than undefined behaviour.
 */
class UNREALMCP_API FEpicUnrealMCPTransactionCommands
{
public:
	FEpicUnrealMCPTransactionCommands();

	TSharedPtr<FJsonObject> HandleCommand(
		const FString& CommandType,
		const TSharedPtr<FJsonObject>& Params);

	/** Returns true if a transaction is currently open. */
	bool IsTransactionActive() const { return bTransactionActive; }

private:
	TSharedPtr<FJsonObject> HandleBeginTransaction(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleEndTransaction(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRollbackTransaction(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleUndo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRedo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCheckpointSceneState(const TSharedPtr<FJsonObject>& Params);

	bool bTransactionActive;
	FString ActiveTransactionDescription;
	int32 ActiveTransactionIndex;
};
