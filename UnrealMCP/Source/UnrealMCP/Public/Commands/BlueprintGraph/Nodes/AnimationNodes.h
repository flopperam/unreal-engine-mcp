#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"

class UK2Node;

/**
 * Créateur de nodes d'animation Unreal Blueprint
 */
class FAnimationNodeCreator
{
public:
	/**
	 * Crée un nœud Timeline (K2Node_Timeline)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y, timeline_name
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateTimelineNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);
};
