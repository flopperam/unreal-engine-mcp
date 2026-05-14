#pragma once

#include "CoreMinimal.h"
#include "Json.h"

// Forward declarations
class AActor;
class UBlueprint;
class UWorld;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UK2Node_Event;
class UK2Node_CallFunction;
class UK2Node_VariableGet;
class UK2Node_VariableSet;
class UK2Node_InputAction;
class UK2Node_Self;
class UFunction;
class UEpicUnrealMCPBridge;

/**
 * In-memory index for O(1) actor lookup by name and mcp_id.
 * Updated on spawn/delete to avoid GetAllActorsOfClass linear scans.
 */
struct UNREALMCP_API FActorIndex
{
    TMap<FName, TWeakObjectPtr<AActor>> NameIndex;
    TMap<FString, TWeakObjectPtr<AActor>> McpIdIndex;

    void AddActor(AActor* Actor);
    void RemoveActor(AActor* Actor);
    AActor* FindByName(const FName& Name);
    AActor* FindByMcpId(const FString& McpId);
    void RebuildFromWorld(UWorld* World);
    void Clear();
};

/**
 * Common utilities for EpicUnrealMCP commands
 */
class UNREALMCP_API FEpicUnrealMCPCommonUtils
{
public:
    // JSON utilities
    static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Message);
    static TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data = nullptr);
    static void GetIntArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray);
    static void GetFloatArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<float>& OutArray);
    static FVector2D GetVector2DFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName);
    static FVector GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName);
    static bool TryGetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, FVector& OutVector, FString& OutError);
    static FRotator GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName);
    static bool TryGetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, FRotator& OutRotator, FString& OutError);
    
    // Actor utilities
    static TSharedPtr<FJsonValue> ActorToJson(AActor* Actor);
    static TSharedPtr<FJsonObject> ActorToJsonObject(AActor* Actor, bool bDetailed = false);
    
    // Blueprint utilities
    static UBlueprint* FindBlueprint(const FString& BlueprintName);
    static UBlueprint* FindBlueprintByName(const FString& BlueprintName);
    static UEdGraph* FindOrCreateEventGraph(UBlueprint* Blueprint);
    
    // Blueprint node utilities
    static UK2Node_Event* CreateEventNode(UEdGraph* Graph, const FString& EventName, const FVector2D& Position);
    static UK2Node_CallFunction* CreateFunctionCallNode(UEdGraph* Graph, UFunction* Function, const FVector2D& Position);
    static UK2Node_VariableGet* CreateVariableGetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position);
    static UK2Node_VariableSet* CreateVariableSetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position);
    static UK2Node_InputAction* CreateInputActionNode(UEdGraph* Graph, const FString& ActionName, const FVector2D& Position);
    static UK2Node_Self* CreateSelfReferenceNode(UEdGraph* Graph, const FVector2D& Position);
    static bool ConnectGraphNodes(UEdGraph* Graph, UEdGraphNode* SourceNode, const FString& SourcePinName, 
                                UEdGraphNode* TargetNode, const FString& TargetPinName);
    static UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction = EGPD_MAX);
    static UK2Node_Event* FindExistingEventNode(UEdGraph* Graph, const FString& EventName);

    // Property utilities
    static bool SetObjectProperty(UObject* Object, const FString& PropertyName,
                                 const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage);

    // Tag / actor lookup helpers (moved from EpicUnrealMCPEditorCommands.cpp)
    static void ApplyMcpIdAndTags(AActor* Actor, const FString& McpId, const TArray<FString>& ExtraTags);
    static TArray<FString> ReadStringArrayField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName);
    static AActor* FindActorByMcpIdTag(UWorld* World, const FString& McpId);

    // Bridge index accessor (used by command handlers for O(1) actor lookup)
    static FActorIndex& GetActorIndex();
}; 