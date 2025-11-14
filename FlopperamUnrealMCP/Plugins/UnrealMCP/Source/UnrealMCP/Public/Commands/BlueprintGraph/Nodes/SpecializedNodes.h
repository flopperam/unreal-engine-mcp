// Nom du fichier: SpecializedNodes.h
// Date de création: 2025-11-02
// Auteur: Zoscran
// Description: Header pour création de nodes spécialisés (GetDataTableRow, AddComponentByClass, Self, etc.)

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"

class UK2Node;

/**
 * Créateur de nodes spécialisés Unreal Blueprint
 */
class FSpecializedNodeCreator
{
public:
	/**
	 * Crée un nœud Get Data Table Row (K2Node_GetDataTableRow)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y, data_table
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateGetDataTableRowNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Crée un nœud Add Component By Class (K2Node_AddComponentByClass)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y, component_class
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateAddComponentByClassNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Crée un nœud Self (K2Node_Self)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateSelfNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Crée un nœud Construct Object From Class (K2Node_ConstructObjectFromClass)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y, class_type
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateConstructObjectNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Crée un nœud Knot (K2Node_Knot) - pour réorganiser les connexions
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateKnotNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);
};
