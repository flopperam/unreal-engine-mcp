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
 * Classe utilitaire pour connecter des nœuds Blueprint
 */
class UNREALMCP_API FBPConnector
{
public:
    /**
     * Connecte deux nœuds Blueprint via leurs pins
     * @param Params JSON contenant blueprint_name, source_node_id, source_pin_name, target_node_id, target_pin_name
     * @return JSON avec success et détails de la connexion
     */
    static TSharedPtr<FJsonObject> ConnectNodes(const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * Trouve un nœud par son ID dans le graphe
     */
    static UK2Node* FindNodeById(UEdGraph* Graph, const FString& NodeId);

    /**
     * Trouve une pin par son nom dans un nœud
     */
    static UEdGraphPin* FindPinByName(UK2Node* Node, const FString& PinName, EEdGraphPinDirection Direction);

    /**
     * Vérifie la compatibilité entre deux pins
     */
    static bool ArePinsCompatible(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin);
};