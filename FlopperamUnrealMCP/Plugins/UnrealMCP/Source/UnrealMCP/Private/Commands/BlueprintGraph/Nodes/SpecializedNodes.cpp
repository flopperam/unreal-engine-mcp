#include "Commands/BlueprintGraph/Nodes/SpecializedNodes.h"
#include "Commands/BlueprintGraph/Nodes/NodeCreatorUtils.h"
#include "K2Node_GetDataTableRow.h"
#include "K2Node_AddComponentByClass.h"
#include "K2Node_Self.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "K2Node_Knot.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Json.h"

namespace
{
	UEdGraph* ResolveMacroGraph(const FString& MacroBlueprintPath, const FString& MacroName)
	{
		if (MacroName.IsEmpty())
		{
			return nullptr;
		}

		UBlueprint* MacroBlueprint = LoadObject<UBlueprint>(nullptr, *MacroBlueprintPath);
		if (!MacroBlueprint)
		{
			return nullptr;
		}

		for (UEdGraph* MacroGraph : MacroBlueprint->MacroGraphs)
		{
			if (MacroGraph && MacroGraph->GetName().Equals(MacroName, ESearchCase::IgnoreCase))
			{
				return MacroGraph;
			}
		}

		return nullptr;
	}

	FString NormalizeStandardMacroName(const FString& MacroName)
	{
		FString CompactMacroName = MacroName;
		CompactMacroName.ReplaceInline(TEXT("_"), TEXT(""));
		CompactMacroName.ReplaceInline(TEXT("-"), TEXT(""));
		CompactMacroName.ReplaceInline(TEXT(" "), TEXT(""));

		if (CompactMacroName.Equals(TEXT("ForEach"), ESearchCase::IgnoreCase) ||
			CompactMacroName.Equals(TEXT("ForEachLoop"), ESearchCase::IgnoreCase))
		{
			return TEXT("ForEachLoop");
		}

		if (CompactMacroName.Equals(TEXT("ForEachWithBreak"), ESearchCase::IgnoreCase) ||
			CompactMacroName.Equals(TEXT("ForEachLoopWithBreak"), ESearchCase::IgnoreCase))
		{
			return TEXT("ForEachLoopWithBreak");
		}

		if (CompactMacroName.Equals(TEXT("ReverseForEach"), ESearchCase::IgnoreCase) ||
			CompactMacroName.Equals(TEXT("ReverseForEachLoop"), ESearchCase::IgnoreCase))
		{
			return TEXT("ReverseForEachLoop");
		}

		if (CompactMacroName.Equals(TEXT("For"), ESearchCase::IgnoreCase) ||
			CompactMacroName.Equals(TEXT("ForLoop"), ESearchCase::IgnoreCase))
		{
			return TEXT("ForLoop");
		}

		if (CompactMacroName.Equals(TEXT("ForWithBreak"), ESearchCase::IgnoreCase) ||
			CompactMacroName.Equals(TEXT("ForLoopWithBreak"), ESearchCase::IgnoreCase))
		{
			return TEXT("ForLoopWithBreak");
		}

		if (CompactMacroName.Equals(TEXT("While"), ESearchCase::IgnoreCase) ||
			CompactMacroName.Equals(TEXT("WhileLoop"), ESearchCase::IgnoreCase))
		{
			return TEXT("WhileLoop");
		}

		return MacroName;
	}
}

UK2Node* FSpecializedNodeCreator::CreateGetDataTableRowNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	UK2Node_GetDataTableRow* DataTableRowNode = NewObject<UK2Node_GetDataTableRow>(Graph);
	if (!DataTableRowNode)
	{
		return nullptr;
	}

	// Note: DataTable property not available in UE5.5 API
	// Parameter ignored - node created without data table reference

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	DataTableRowNode->NodePosX = static_cast<int32>(PosX);
	DataTableRowNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(DataTableRowNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(DataTableRowNode, Graph);

	return DataTableRowNode;
}

UK2Node* FSpecializedNodeCreator::CreateAddComponentByClassNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	UK2Node_AddComponentByClass* AddComponentNode = NewObject<UK2Node_AddComponentByClass>(Graph);
	if (!AddComponentNode)
	{
		return nullptr;
	}

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	AddComponentNode->NodePosX = static_cast<int32>(PosX);
	AddComponentNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(AddComponentNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(AddComponentNode, Graph);

	return AddComponentNode;
}

UK2Node* FSpecializedNodeCreator::CreateSelfNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(Graph);
	if (!SelfNode)
	{
		return nullptr;
	}

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	SelfNode->NodePosX = static_cast<int32>(PosX);
	SelfNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(SelfNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(SelfNode, Graph);

	return SelfNode;
}

UK2Node* FSpecializedNodeCreator::CreateConstructObjectNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	UK2Node_ConstructObjectFromClass* ConstructObjNode = NewObject<UK2Node_ConstructObjectFromClass>(Graph);
	if (!ConstructObjNode)
	{
		return nullptr;
	}

	// Note: TargetClass property not available in UE5.5 API
	// Parameter ignored - node created without class reference

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	ConstructObjNode->NodePosX = static_cast<int32>(PosX);
	ConstructObjNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(ConstructObjNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(ConstructObjNode, Graph);

	return ConstructObjNode;
}

UK2Node* FSpecializedNodeCreator::CreateKnotNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	UK2Node_Knot* KnotNode = NewObject<UK2Node_Knot>(Graph);
	if (!KnotNode)
	{
		return nullptr;
	}

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	KnotNode->NodePosX = static_cast<int32>(PosX);
	KnotNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(KnotNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(KnotNode, Graph);

	return KnotNode;
}

UK2Node* FSpecializedNodeCreator::CreateMacroInstanceNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	FString MacroName;
	if (!Params->TryGetStringField(TEXT("macro_name"), MacroName))
	{
		if (!Params->TryGetStringField(TEXT("target_function"), MacroName))
		{
			if (!Params->TryGetStringField(TEXT("macro_type"), MacroName))
			{
				Params->TryGetStringField(TEXT("standard_macro"), MacroName);
			}
		}
	}
	MacroName = NormalizeStandardMacroName(MacroName);

	FString MacroBlueprintPath = TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros");
	if (!Params->TryGetStringField(TEXT("macro_blueprint"), MacroBlueprintPath))
	{
		Params->TryGetStringField(TEXT("target_blueprint"), MacroBlueprintPath);
	}

	UEdGraph* MacroGraph = ResolveMacroGraph(MacroBlueprintPath, MacroName);
	if (!MacroGraph)
	{
		return nullptr;
	}

	UK2Node_MacroInstance* MacroNode = NewObject<UK2Node_MacroInstance>(Graph);
	if (!MacroNode)
	{
		return nullptr;
	}

	MacroNode->SetMacroGraph(MacroGraph);

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	MacroNode->NodePosX = static_cast<int32>(PosX);
	MacroNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(MacroNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(MacroNode, Graph);

	return MacroNode;
}

UK2Node* FSpecializedNodeCreator::CreateFunctionResultNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	UK2Node_FunctionResult* TemplateResult = nullptr;
	for (UEdGraphNode* ExistingNode : Graph->Nodes)
	{
		if (UK2Node_FunctionResult* ExistingResult = Cast<UK2Node_FunctionResult>(ExistingNode))
		{
			TemplateResult = ExistingResult;
			break;
		}
	}

	if (!TemplateResult)
	{
		return nullptr;
	}

	UK2Node_FunctionResult* ResultNode = NewObject<UK2Node_FunctionResult>(Graph);
	if (!ResultNode)
	{
		return nullptr;
	}

	ResultNode->FunctionReference = TemplateResult->FunctionReference;
	ResultNode->UserDefinedPins = TemplateResult->UserDefinedPins;

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	ResultNode->NodePosX = static_cast<int32>(PosX);
	ResultNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(ResultNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(ResultNode, Graph);

	return ResultNode;
}
