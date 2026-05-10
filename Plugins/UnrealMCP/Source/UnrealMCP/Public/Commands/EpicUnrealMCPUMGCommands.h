#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler for UI / UMG / Common UI MCP commands.
 *
 * These commands operate on Widget Blueprint assets and runtime widget
 * instances. The bridge dispatches them onto the GameThread before entry.
 */
class UNREALMCP_API FEpicUnrealMCPUMGCommands
{
public:
    FEpicUnrealMCPUMGCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleCreateWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddWidgetToWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveWidgetFromWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWidgetSlotProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWidgetText(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWidgetFont(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWidgetColor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWidgetBrush(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWidgetStyle(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleBindWidgetButtonOnClicked(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateWidgetAnimation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCompileWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleInspectWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddWidgetToViewport(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveWidgetFromParent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleClickWidgetButton(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetUIInputMode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMouseCursorVisible(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateUITemplate(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateWidgetInstance(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleBindWidgetProperty(const TSharedPtr<FJsonObject>& Params);
};
