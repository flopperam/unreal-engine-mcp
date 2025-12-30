#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"

class UK2Node;

/**
 * Créateur de nodes de casting Unreal Blueprint
 */
class FCastingNodeCreator
{
public:
	/**
	 * Crée un nœud Dynamic Cast (K2Node_DynamicCast)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y, target_class
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateDynamicCastNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Crée un nœud Class Dynamic Cast (K2Node_ClassDynamicCast)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y, target_class
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateClassDynamicCastNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Crée un nœud Cast Byte To Enum (K2Node_CastByteToEnum)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y, enum_type
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateCastByteToEnumNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);
};
