// Nom du fichier: UtilityNodes.h
// Date de création: 2025-11-02
// Auteur: Zoscran
// Description: Header pour création de nodes utilitaires (Print, CallFunction, Select, SpawnActor)

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"

class UK2Node;

/**
 * Créateur de nodes utilitaires Unreal Blueprint
 */
class FUtilityNodeCreator
{
public:
	/**
	 * Crée un nœud Print (K2Node_CallFunction pour PrintString)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y, message
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreatePrintNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Crée un nœud Call Function (K2Node_CallFunction)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y, function_name, target_object
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateCallFunctionNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Crée un nœud Select (K2Node_Select)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y, pin_type
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateSelectNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Crée un nœud Spawn Actor From Class (K2Node_SpawnActorFromClass)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y, actor_class
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateSpawnActorNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);
};
