// Header for creating control flow nodes (Branch, Switch, Comparison, ExecutionSequence)

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"

class UK2Node;

/**
 * Créateur de nodes de contrôle de flux Unreal Blueprint
 */
class FControlFlowNodeCreator
{
public:
	/**
	 * Crée un nœud Branch (K2Node_IfThenElse)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateBranchNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Crée un nœud Comparison (K2Node_PromotableOperator)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant:
	 *                 - pos_x, pos_y: position
	 *                 - pin_type: type des pins (int, float, string, bool, vector, name, text)
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateComparisonNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Crée un nœud Switch (K2Node_Switch)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateSwitchNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Crée un nœud Switch sur Enum (K2Node_SwitchEnum)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y, enum_type
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateSwitchEnumNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Crée un nœud Switch sur Integer (K2Node_SwitchInteger)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateSwitchIntegerNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Crée un nœud Execution Sequence (K2Node_ExecutionSequence)
	 * @param Graph - Le graphe dans lequel ajouter le nœud
	 * @param Params - Paramètres JSON contenant pos_x, pos_y
	 * @return Le nœud créé ou nullptr en cas d'erreur
	 */
	static UK2Node* CreateExecutionSequenceNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);
};
