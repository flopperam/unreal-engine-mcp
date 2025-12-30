// Utility header for node creation helpers

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"

class UK2Node;

/**
 * Utilitaires partagés pour la création et l'initialisation de K2Nodes
 *
 * Cette classe fournit des fonctions mutualisées pour éviter la duplication
 * de code dans tous les créateurs de nodes (ControlFlowNodes, DataNodes, etc.)
 */
class FNodeCreatorUtils
{
public:
	/**
	 * Initialise complètement un nœud K2Node après sa création
	 *
	 * Effectue les étapes suivantes dans l'ordre :
	 * 1. AllocateDefaultPins() - Crée les pins par défaut
	 * 2. ReconstructNode() - Reconstruit le nœud (notifie Unreal des modifications)
	 * 3. NotifyGraphChanged() - Notifie le graph que quelque chose a changé
	 *
	 * @param Node - Le nœud à initialiser (doit être non-null)
	 * @param Graph - Le graphe contenant le nœud (doit être non-null)
	 *
	 * @return true si l'initialisation a réussi, false sinon
	 *
	 * @note Cette fonction DOIT être appelée après :
	 *       - NewObject<NodeType>(Graph)
	 *       - Graph->AddNode(Node, ...)
	 *       - Toute configuration spécifique au type (SetExternalMember, StructType, etc.)
	 *
	 * @example
	 * UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
	 * Graph->AddNode(Node, true, false);
	 * Node->FunctionReference.SetExternalMember(FunctionName, TargetClass);  // Configuration AVANT
	 * FNodeCreatorUtils::InitializeK2Node(Node, Graph);  // Initialisation APRÈS
	 */
	static bool InitializeK2Node(UK2Node* Node, UEdGraph* Graph);

	/**
	 * Extrait les coordonnées X/Y d'un objet JSON
	 *
	 * @param Params - Objet JSON contenant "pos_x" et "pos_y"
	 * @param OutX - Référence pour stocker la position X (défaut: 0.0)
	 * @param OutY - Référence pour stocker la position Y (défaut: 0.0)
	 *
	 * @note Valide l'existence des champs avant d'essayer de les lire
	 */
	static void ExtractNodePosition(const TSharedPtr<class FJsonObject>& Params, double& OutX, double& OutY);
};
