// =====================================================================
// EpicUnrealMCPEditorCommands
//
// Slim shell after the Phase 2 / Phase 3 refactors.
//
// Phase 2 moved Actor CRUD into FEpicUnrealMCPActorCommands.
// Phase 3 moved NavAI + Spline into FEpicUnrealMCPNavigationCommands.
// Phase 1 had previously moved Procedural / Physics / Validation /
// Cognitive Processing into FEpicUnrealMCPProceduralCommands.
//
// We retain this class as a generic editor hook so that:
//   * the registration pattern in UEpicUnrealMCPBridge remains uniform
//     across all command groups, and
//   * any future generic editor-only command (viewport tweaks, misc
//     UEditorEngine helpers, etc.) has an obvious home that does not
//     belong in a more focused handler class.
//
// Add new commands here only if they do not naturally belong to one of
// the dedicated handler classes; otherwise create a new
// FEpicUnrealMCP<Domain>Commands class so this file does not grow back.
// =====================================================================

#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Editor.h"

FEpicUnrealMCPEditorCommands::FEpicUnrealMCPEditorCommands()
{
}

UWorld* FEpicUnrealMCPEditorCommands::GetEditorWorld() const
{
    if (!GEditor)
    {
        return nullptr;
    }
    return GEditor->GetEditorWorldContext().World();
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPEditorCommands::*)(const TSharedPtr<FJsonObject>&);

    // Intentionally empty after Phase 2 / Phase 3.  Add new entries here
    // only for commands that genuinely belong in the generic editor
    // surface and do not fit any other dedicated handler class.
    static const TMap<FString, Handler> Dispatch = {};

    const Handler* H = Dispatch.Find(CommandType);
    if (H)
    {
        return (this->*(*H))(Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}