#include "Commands/EpicUnrealMCPTransactionCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Commands/EpicUnrealMCPErrorCodes.h"
#include "Editor.h"
#include "ScopedTransaction.h"

FEpicUnrealMCPTransactionCommands::FEpicUnrealMCPTransactionCommands()
	: bTransactionActive(false)
	, ActiveTransactionIndex(-1)
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPTransactionCommands::HandleCommand(
	const FString& CommandType,
	const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("begin_editor_transaction"))
	{
		return HandleBeginTransaction(Params);
	}
	else if (CommandType == TEXT("end_editor_transaction"))
	{
		return HandleEndTransaction(Params);
	}
	else if (CommandType == TEXT("rollback_transaction"))
	{
		return HandleRollbackTransaction(Params);
	}
	else if (CommandType == TEXT("undo"))
	{
		return HandleUndo(Params);
	}
	else if (CommandType == TEXT("redo"))
	{
		return HandleRedo(Params);
	}
	else if (CommandType == TEXT("checkpoint_scene_state"))
	{
		return HandleCheckpointSceneState(Params);
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		MCPErrorCodes::UNKNOWN_COMMAND,
		FString::Printf(TEXT("Unknown transaction command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// begin_editor_transaction
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPTransactionCommands::HandleBeginTransaction(
	const TSharedPtr<FJsonObject>& Params)
{
	if (bTransactionActive)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			MCPErrorCodes::TRANSACTION_ALREADY_ACTIVE,
			FString::Printf(TEXT("Transaction already active: %s"), *ActiveTransactionDescription));
	}

	FString Description = TEXT("MCP Transaction");
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("description"), Description);
	}

	if (!GEditor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			MCPErrorCodes::EDITOR_BUSY, TEXT("GEditor is not available"));
	}

	ActiveTransactionIndex = GEditor->BeginTransaction(TEXT("MCP"), FText::FromString(Description), nullptr);
	bTransactionActive = true;
	ActiveTransactionDescription = Description;

	UE_LOG(LogTemp, Display, TEXT("MCP: BeginTransaction '%s' (index %d)"), *Description, ActiveTransactionIndex);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("description"), Description);
	Data->SetNumberField(TEXT("transaction_index"), ActiveTransactionIndex);
	return Data;
}

// ---------------------------------------------------------------------------
// end_editor_transaction
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPTransactionCommands::HandleEndTransaction(
	const TSharedPtr<FJsonObject>& Params)
{
	if (!bTransactionActive)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			MCPErrorCodes::TRANSACTION_NOT_ACTIVE,
			TEXT("No active transaction to end"));
	}

	if (!GEditor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			MCPErrorCodes::EDITOR_BUSY, TEXT("GEditor is not available"));
	}

	int32 EndedIndex = GEditor->EndTransaction();
	bTransactionActive = false;

	UE_LOG(LogTemp, Display, TEXT("MCP: EndTransaction '%s' (index %d)"), *ActiveTransactionDescription, EndedIndex);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("description"), ActiveTransactionDescription);
	Data->SetNumberField(TEXT("transaction_index"), EndedIndex);
	Data->SetBoolField(TEXT("committed"), true);

	ActiveTransactionDescription.Empty();
	ActiveTransactionIndex = -1;
	return Data;
}

// ---------------------------------------------------------------------------
// rollback_transaction  (end + undo as safe fallback)
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPTransactionCommands::HandleRollbackTransaction(
	const TSharedPtr<FJsonObject>& Params)
{
	if (!bTransactionActive)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			MCPErrorCodes::TRANSACTION_NOT_ACTIVE,
			TEXT("No active transaction to roll back"));
	}

	if (!GEditor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			MCPErrorCodes::EDITOR_BUSY, TEXT("GEditor is not available"));
	}

	FString RolledBackDesc = ActiveTransactionDescription;

	// End the transaction first, then immediately undo it.
	GEditor->EndTransaction();
	bTransactionActive = false;
	ActiveTransactionDescription.Empty();
	ActiveTransactionIndex = -1;

	bool bUndone = GEditor->UndoTransaction();

	UE_LOG(LogTemp, Display, TEXT("MCP: RollbackTransaction '%s' (undo result: %s)"),
		*RolledBackDesc, bUndone ? TEXT("true") : TEXT("false"));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("rolled_back_description"), RolledBackDesc);
	Data->SetBoolField(TEXT("rolled_back"), bUndone);
	return Data;
}

// ---------------------------------------------------------------------------
// undo
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPTransactionCommands::HandleUndo(
	const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			MCPErrorCodes::EDITOR_BUSY, TEXT("GEditor is not available"));
	}

	if (bTransactionActive)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			MCPErrorCodes::TRANSACTION_ALREADY_ACTIVE,
			TEXT("Cannot undo while a transaction is active. End or rollback the transaction first."));
	}

	bool bResult = GEditor->UndoTransaction();

	UE_LOG(LogTemp, Display, TEXT("MCP: Undo (result: %s)"), bResult ? TEXT("true") : TEXT("false"));

	if (!bResult)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			MCPErrorCodes::UNDO_FAILED,
			TEXT("No actions to undo, or undo failed"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetBoolField(TEXT("undo_performed"), true);
	return Data;
}

// ---------------------------------------------------------------------------
// redo
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPTransactionCommands::HandleRedo(
	const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			MCPErrorCodes::EDITOR_BUSY, TEXT("GEditor is not available"));
	}

	if (bTransactionActive)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			MCPErrorCodes::TRANSACTION_ALREADY_ACTIVE,
			TEXT("Cannot redo while a transaction is active."));
	}

	bool bResult = GEditor->RedoTransaction();

	UE_LOG(LogTemp, Display, TEXT("MCP: Redo (result: %s)"), bResult ? TEXT("true") : TEXT("false"));

	if (!bResult)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			MCPErrorCodes::REDO_FAILED,
			TEXT("No actions to redo, or redo failed"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetBoolField(TEXT("redo_performed"), true);
	return Data;
}

// ---------------------------------------------------------------------------
// checkpoint_scene_state
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPTransactionCommands::HandleCheckpointSceneState(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Label = TEXT("MCP Checkpoint");
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("label"), Label);
	}

	if (!GEditor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			MCPErrorCodes::EDITOR_BUSY, TEXT("GEditor is not available"));
	}

	// A scoped transaction creates a labelled undo bookmark.
	{
		FScopedTransaction ScopedTransaction(FText::FromString(Label));
	}

	UE_LOG(LogTemp, Display, TEXT("MCP: Checkpoint '%s'"), *Label);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("checkpoint_label"), Label);
	return Data;
}
