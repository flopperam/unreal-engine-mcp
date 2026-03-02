#pragma once

#include "CoreMinimal.h"

/**
 * Structured error code constants for MCP protocol responses.
 * Mirror these codes in Python/helpers/error_codes.py.
 */
namespace MCPErrorCodes
{
	// General
	static const FString OK                          = TEXT("OK");
	static const FString UNKNOWN_ERROR               = TEXT("UNKNOWN_ERROR");
	static const FString UNKNOWN_COMMAND             = TEXT("UNKNOWN_COMMAND");

	// Parameter validation
	static const FString INVALID_PARAM               = TEXT("INVALID_PARAM");
	static const FString MISSING_PARAM               = TEXT("MISSING_PARAM");

	// Entity not found
	static const FString NOT_FOUND                   = TEXT("NOT_FOUND");
	static const FString ACTOR_NOT_FOUND             = TEXT("ACTOR_NOT_FOUND");
	static const FString BLUEPRINT_NOT_FOUND         = TEXT("BLUEPRINT_NOT_FOUND");
	static const FString ASSET_NOT_FOUND             = TEXT("ASSET_NOT_FOUND");

	// Transaction errors
	static const FString TRANSACTION_FAILED          = TEXT("TRANSACTION_FAILED");
	static const FString TRANSACTION_NOT_ACTIVE      = TEXT("TRANSACTION_NOT_ACTIVE");
	static const FString TRANSACTION_ALREADY_ACTIVE  = TEXT("TRANSACTION_ALREADY_ACTIVE");
	static const FString UNDO_FAILED                 = TEXT("UNDO_FAILED");
	static const FString REDO_FAILED                 = TEXT("REDO_FAILED");

	// Connection / protocol
	static const FString CONNECTION_ERROR            = TEXT("CONNECTION_ERROR");
	static const FString TIMEOUT                     = TEXT("TIMEOUT");
	static const FString PROTOCOL_ERROR              = TEXT("PROTOCOL_ERROR");

	// Batch execution
	static const FString BATCH_PARTIAL_FAILURE       = TEXT("BATCH_PARTIAL_FAILURE");
	static const FString BATCH_ABORTED               = TEXT("BATCH_ABORTED");

	// Idempotency
	static const FString DUPLICATE_REQUEST           = TEXT("DUPLICATE_REQUEST");

	// Version
	static const FString VERSION_MISMATCH            = TEXT("VERSION_MISMATCH");

	// Editor state
	static const FString EDITOR_BUSY                 = TEXT("EDITOR_BUSY");
	static const FString OPERATION_NOT_SUPPORTED     = TEXT("OPERATION_NOT_SUPPORTED");
}
