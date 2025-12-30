#include "BlueprintGraph/Nodes/NodeCreatorUtils.h"
#include "K2Node.h"
#include "EdGraph/EdGraph.h"
#include "Json.h"

bool FNodeCreatorUtils::InitializeK2Node(UK2Node* Node, UEdGraph* Graph)
{
	// Vérifications basiques
	if (!Node || !Graph)
	{
		return false;
	}

	// 1. Allouer les pins par défaut
	Node->AllocateDefaultPins();

	// 2. Reconstruire le nœud (notifie Unreal des modifications)
	Node->ReconstructNode();

	// 3. Notifier le graph que quelque chose a changé
	Graph->NotifyGraphChanged();

	return true;
}

void FNodeCreatorUtils::ExtractNodePosition(const TSharedPtr<FJsonObject>& Params, double& OutX, double& OutY)
{
	// Initialiser les valeurs par défaut
	OutX = 0.0;
	OutY = 0.0;

	// Vérifier que Params est valide
	if (!Params.IsValid())
	{
		return;
	}

	// Essayer de récupérer pos_x
	if (!Params->TryGetNumberField(TEXT("pos_x"), OutX))
	{
		OutX = 0.0;
	}

	// Essayer de récupérer pos_y
	if (!Params->TryGetNumberField(TEXT("pos_y"), OutY))
	{
		OutY = 0.0;
	}
}
