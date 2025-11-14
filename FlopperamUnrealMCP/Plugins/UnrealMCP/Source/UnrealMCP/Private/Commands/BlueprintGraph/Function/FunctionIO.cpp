#include "Commands/BlueprintGraph/Function/FunctionIO.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "EditorAssetLibrary.h"
#include "EdGraph/EdGraphNode.h"

TSharedPtr<FJsonObject> FFunctionIO::AddFunctionIO(const TSharedPtr<FJsonObject>& Params)
{
	// Validate parameters
	if (!Params.IsValid())
	{
		return CreateErrorResponse(TEXT("Invalid parameters"));
	}

	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString FunctionName;
	if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
	{
		return CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
	}

	FString ParamName;
	if (!Params->TryGetStringField(TEXT("param_name"), ParamName))
	{
		return CreateErrorResponse(TEXT("Missing 'param_name' parameter"));
	}

	FString ParamType;
	if (!Params->TryGetStringField(TEXT("param_type"), ParamType))
	{
		return CreateErrorResponse(TEXT("Missing 'param_type' parameter"));
	}

	// Get direction parameter (input or output)
	FString Direction = TEXT("input");
	Params->TryGetStringField(TEXT("direction"), Direction);

	// Validate direction
	bool bIsInput;
	if (Direction == TEXT("input"))
	{
		bIsInput = true;
	}
	else if (Direction == TEXT("output"))
	{
		bIsInput = false;
	}
	else
	{
		return CreateErrorResponse(TEXT("Invalid direction: must be 'input' or 'output'"));
	}

	// Get optional is_array parameter
	bool bIsArray = false;
	Params->TryGetBoolField(TEXT("is_array"), bIsArray);

	// Validate parameter name
	if (!ValidateParameterName(ParamName))
	{
		return CreateErrorResponse(TEXT("Invalid parameter name: contains spaces or special characters"));
	}

	// Load the Blueprint
	UBlueprint* Blueprint = LoadBlueprint(BlueprintName);
	if (!Blueprint)
	{
		return CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
	}

	// Find the function graph
	UEdGraph* FunctionGraph = FindFunctionGraph(Blueprint, FunctionName);
	if (!FunctionGraph)
	{
		return CreateErrorResponse(FString::Printf(TEXT("Function not found: %s"), *FunctionName));
	}

	// Add the parameter
	if (!AddFunctionParameter(Blueprint, FunctionName, ParamName, ParamType, bIsInput, bIsArray))
	{
		return CreateErrorResponse(FString::Printf(TEXT("Failed to add %s parameter: %s"),
			bIsInput ? TEXT("input") : TEXT("output"), *ParamName));
	}

	// Mark Blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	FunctionGraph->NotifyGraphChanged();

	return CreateSuccessResponse(ParamName, ParamType, bIsInput);
}

TSharedPtr<FJsonObject> FFunctionIO::AddFunctionInput(const TSharedPtr<FJsonObject>& Params)
{
	// Create a copy of params and add direction="input"
	TSharedPtr<FJsonObject> InputParams = MakeShareable(new FJsonObject);

	// Copy all fields from original params
	for (const auto& Field : Params->Values)
	{
		InputParams->SetField(Field.Key, Field.Value);
	}

	// Set direction to input
	InputParams->SetStringField(TEXT("direction"), TEXT("input"));

	// Call the unified function
	return AddFunctionIO(InputParams);
}

TSharedPtr<FJsonObject> FFunctionIO::AddFunctionOutput(const TSharedPtr<FJsonObject>& Params)
{
	// Create a copy of params and add direction="output"
	TSharedPtr<FJsonObject> OutputParams = MakeShareable(new FJsonObject);

	// Copy all fields from original params
	for (const auto& Field : Params->Values)
	{
		OutputParams->SetField(Field.Key, Field.Value);
	}

	// Set direction to output
	OutputParams->SetStringField(TEXT("direction"), TEXT("output"));

	// Call the unified function
	return AddFunctionIO(OutputParams);
}

bool FFunctionIO::AddFunctionParameter(
	UBlueprint* Blueprint,
	const FString& FunctionName,
	const FString& ParamName,
	const FString& ParamType,
	bool bIsInput,
	bool bIsArray)
{
	if (!Blueprint)
	{
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = FindFunctionGraph(Blueprint, FunctionName);
	if (!FunctionGraph)
	{
		return false;
	}

	// Create the property type
	FEdGraphPinType PropertyType = GetPropertyTypeFromString(ParamType);

	UEdGraphPin* NewPin = nullptr;

	if (bIsInput)
	{
		// For INPUT parameters: Find FunctionEntry and create OUTPUT pin
		UK2Node_FunctionEntry* EntryNode = nullptr;
		for (UEdGraphNode* Node : FunctionGraph->Nodes)
		{
			if (Node && Node->IsA<UK2Node_FunctionEntry>())
			{
				EntryNode = Cast<UK2Node_FunctionEntry>(Node);
				break;
			}
		}

		if (!EntryNode)
		{
			UE_LOG(LogTemp, Warning, TEXT("Could not find FunctionEntry node in function graph '%s'"), *FunctionName);
			return false;
		}

		// Create OUTPUT pin on FunctionEntry for input parameter
		NewPin = EntryNode->CreateUserDefinedPin(*ParamName, PropertyType, EGPD_Output);
	}
	else
	{
		// For OUTPUT parameters: Find or create FunctionResult and create OUTPUT pin
		UK2Node_FunctionResult* ResultNode = nullptr;

		// First try to find existing FunctionResult node
		for (UEdGraphNode* Node : FunctionGraph->Nodes)
		{
			if (Node && Node->IsA<UK2Node_FunctionResult>())
			{
				ResultNode = Cast<UK2Node_FunctionResult>(Node);
				break;
			}
		}

		// SOLUTION 2: Let Unreal create the FunctionResult node automatically
		// If no FunctionResult exists, CreateUserDefinedPin should create it automatically
		// when we add a pin to the function signature.
		//
		// COMMENTED OUT - Previous approach that caused double execute pin bug:
		/*
		if (!ResultNode)
		{
			// Create the FunctionResult node manually using NewObject
			ResultNode = NewObject<UK2Node_FunctionResult>(FunctionGraph);
			if (!ResultNode)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to create FunctionResult node for function '%s'"), *FunctionName);
				return false;
			}

			// Initialize the node
			ResultNode->NodePosX = 400;
			ResultNode->NodePosY = 0;
			ResultNode->CreateNewGuid();
			FunctionGraph->AddNode(ResultNode, false, false);
			ResultNode->PostPlacedNewNode();
			ResultNode->AllocateDefaultPins();  // <-- This caused double execute pin!

			UE_LOG(LogTemp, Display, TEXT("FunctionResult node created manually for function '%s'"), *FunctionName);
		}

		// Now add the OUTPUT pin to the FunctionResult
		NewPin = ResultNode->CreateUserDefinedPin(*ParamName, PropertyType, EGPD_Output);
		*/

		// FIX: Create FunctionResult manually but WITHOUT AllocateDefaultPins()
		// AllocateDefaultPins() creates the execute pin, and CreateUserDefinedPin creates another
		// This causes the double execute pin bug. We need to manually create the node and add pins.
		if (ResultNode)
		{
			// FunctionResult already exists, add pin to it
			NewPin = ResultNode->CreateUserDefinedPin(*ParamName, PropertyType, EGPD_Output);
		}
		else
		{
			// Create FunctionResult node manually
			ResultNode = NewObject<UK2Node_FunctionResult>(FunctionGraph);
			if (!ResultNode)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to create FunctionResult node for function '%s'"), *FunctionName);
				return false;
			}

			// Initialize the node
			ResultNode->NodePosX = 400;
			ResultNode->NodePosY = 0;
			ResultNode->CreateNewGuid();
			FunctionGraph->AddNode(ResultNode, false, false);

			// Create the execute input pin manually BEFORE PostPlacedNewNode
			// Use "execute" as the pin name (this is the standard for FunctionResult execute pin)
			UEdGraphPin* ExecutePin = ResultNode->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, FName(TEXT("execute")));
			if (!ExecutePin)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to create execute pin for FunctionResult in function '%s'"), *FunctionName);
				return false;
			}

			// Now call PostPlacedNewNode AFTER manually creating the execute pin
			ResultNode->PostPlacedNewNode();

			// Now add the OUTPUT pin to the FunctionResult (this will be a user-defined pin)
			NewPin = ResultNode->CreateUserDefinedPin(*ParamName, PropertyType, EGPD_Input);
		}
	}

	// For INPUT parameters, we must have a pin created
	// For OUTPUT parameters without FunctionResult, NewPin will be nullptr and that's OK
	if (bIsInput && !NewPin)
	{
		return false;
	}

	// Update the function signature
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	FunctionGraph->NotifyGraphChanged();

	return true;
}

UBlueprint* FFunctionIO::LoadBlueprint(const FString& BlueprintName)
{
	// Try direct load
	UBlueprint* Blueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintName));
	if (Blueprint)
	{
		return Blueprint;
	}

	// Try EditorAssetLibrary
	if (UEditorAssetLibrary::DoesAssetExist(BlueprintName))
	{
		return Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintName));
	}

	return nullptr;
}

FEdGraphPinType FFunctionIO::GetPropertyTypeFromString(const FString& TypeName)
{
	FEdGraphPinType PinType;

	if (TypeName == TEXT("bool"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (TypeName == TEXT("int") || TypeName == TEXT("int32"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (TypeName == TEXT("float"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	}
	else if (TypeName == TEXT("string"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (TypeName == TEXT("vector") || TypeName == TEXT("FVector"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (TypeName == TEXT("rotator") || TypeName == TEXT("FRotator"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else if (TypeName == TEXT("object"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
	}
	else
	{
		// Default to object for unknown types
		PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
	}

	return PinType;
}

UEdGraph* FFunctionIO::FindFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName)
{
	if (!Blueprint || FunctionName.IsEmpty())
	{
		return nullptr;
	}

	// Search in all function graphs
	// Try exact name match first
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph)
		{
			FString GraphName = Graph->GetFName().ToString();
			// Try exact match
			if (GraphName == FunctionName)
			{
				return Graph;
			}
			// Try matching the outer name (function name)
			if (Graph->GetOuter() && Graph->GetOuter()->GetFName().ToString() == FunctionName)
			{
				return Graph;
			}
		}
	}

	// If not found, try to find by searching all graphs more broadly
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName().ToString().Contains(FunctionName))
		{
			return Graph;
		}
	}

	return nullptr;
}

bool FFunctionIO::ValidateParameterName(const FString& ParamName)
{
	if (ParamName.IsEmpty())
	{
		return false;
	}

	// Check for spaces or invalid characters
	for (TCHAR Char : ParamName)
	{
		if (FChar::IsWhitespace(Char) || (!FChar::IsAlnum(Char) && Char != TEXT('_')))
		{
			return false;
		}
	}

	// Must start with letter or underscore
	if (!FChar::IsAlpha(ParamName[0]) && ParamName[0] != TEXT('_'))
	{
		return false;
	}

	return true;
}

TSharedPtr<FJsonObject> FFunctionIO::CreateSuccessResponse(const FString& ParamName, const FString& ParamType, bool bIsInput)
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("param_name"), ParamName);
	Response->SetStringField(TEXT("param_type"), ParamType);
	Response->SetStringField(TEXT("direction"), bIsInput ? TEXT("input") : TEXT("output"));
	return Response;
}

TSharedPtr<FJsonObject> FFunctionIO::CreateErrorResponse(const FString& ErrorMessage)
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error"), ErrorMessage);
	return Response;
}
