// Connects two Blueprint nodes via their pins
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Forward declarations
class UK2Node;
class UEdGraph;
class UEdGraphPin;
enum EEdGraphPinDirection : int;

/**
 * Utility class for connecting Blueprint nodes
 */
class UNREALMCP_API FBPConnector
{
public:
    /**
     * Connects two Blueprint nodes via their pins
     * @param Params JSON containing blueprint_name, source_node_id, source_pin_name, target_node_id, target_pin_name
     * @return JSON with success and connection details
     */
    static TSharedPtr<FJsonObject> ConnectNodes(const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * Finds a node by its ID in the graph
     */
    static UK2Node* FindNodeById(UEdGraph* Graph, const FString& NodeId);

    /**
     * Finds a pin by its name in a node
     */
    static UEdGraphPin* FindPinByName(UK2Node* Node, const FString& PinName, EEdGraphPinDirection Direction);

    /**
     * Checks compatibility between two pins
     */
    static bool ArePinsCompatible(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin);
};