// Header for creating data nodes (Variable Get/Set, MakeArray)

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"

class UK2Node;

/**
 * Créateur de nodes de gestion de données Unreal Blueprint
 */
class FDataNodeCreator
{
public:
	/**
	 * Crée un nœud Variable Get (K2Node_VariableGet)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y, variable_name
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateVariableGetNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Crée un nœud Variable Set (K2Node_VariableSet)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y, variable_name
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateVariableSetNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Crée un nœud Make Array (K2Node_MakeArray)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y, element_type
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateMakeArrayNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);
};
