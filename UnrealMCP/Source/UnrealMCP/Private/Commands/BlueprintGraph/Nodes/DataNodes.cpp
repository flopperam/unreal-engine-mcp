#include "Commands/BlueprintGraph/Nodes/DataNodes.h"
#include "Commands/BlueprintGraph/Nodes/NodeCreatorUtils.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_SetFieldsInStruct.h"
#include "K2Node_GetArrayItem.h"
#include "K2Node_CallArrayFunction.h"
#include "Engine/Blueprint.h"
#include "Kismet/KismetArrayLibrary.h"
#include "UObject/UnrealType.h"
#include "Json.h"

namespace
{
	UScriptStruct* ResolveScriptStruct(const FString& StructPath)
	{
		if (StructPath.IsEmpty())
		{
			return nullptr;
		}

		if (UScriptStruct* FoundStruct = FindObject<UScriptStruct>(nullptr, *StructPath))
		{
			return FoundStruct;
		}

		return LoadObject<UScriptStruct>(nullptr, *StructPath);
	}

	void AddRequestedFieldName(const UScriptStruct* StructType, const FString& RequestedName, TSet<FName>& FieldNames)
	{
		if (RequestedName.IsEmpty())
		{
			return;
		}

		if (StructType)
		{
			for (TFieldIterator<FProperty> PropIt(StructType, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
			{
				FProperty* Property = *PropIt;
				if (Property && Property->GetName().Equals(RequestedName, ESearchCase::IgnoreCase))
				{
					FieldNames.Add(Property->GetFName());
					return;
				}
			}
		}

		FieldNames.Add(FName(*RequestedName));
	}

	void AddRequestedFieldArray(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, const UScriptStruct* StructType, TSet<FName>& FieldNames)
	{
		const TArray<TSharedPtr<FJsonValue>>* RequestedFields = nullptr;
		if (!Params->TryGetArrayField(FieldName, RequestedFields))
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& FieldValue : *RequestedFields)
		{
			FString RequestedName;
			if (FieldValue.IsValid() && FieldValue->TryGetString(RequestedName))
			{
				AddRequestedFieldName(StructType, RequestedName, FieldNames);
			}
		}
	}

	TSet<FName> GetRequestedStructFields(const TSharedPtr<FJsonObject>& Params, const UScriptStruct* StructType)
	{
		TSet<FName> FieldNames;

		bool bShowAllFields = false;
		bool bBoolValue = false;
		if (Params->TryGetBoolField(TEXT("show_all_pins"), bBoolValue))
		{
			bShowAllFields |= bBoolValue;
		}
		if (Params->TryGetBoolField(TEXT("show_all_fields"), bBoolValue))
		{
			bShowAllFields |= bBoolValue;
		}

		if (bShowAllFields && StructType)
		{
			for (TFieldIterator<FProperty> PropIt(StructType, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
			{
				if (FProperty* Property = *PropIt)
				{
					FieldNames.Add(Property->GetFName());
				}
			}
		}

		FString SingleFieldName;
		if (Params->TryGetStringField(TEXT("field_name"), SingleFieldName))
		{
			AddRequestedFieldName(StructType, SingleFieldName, FieldNames);
		}

		AddRequestedFieldArray(Params, TEXT("field_names"), StructType, FieldNames);
		AddRequestedFieldArray(Params, TEXT("fields"), StructType, FieldNames);
		AddRequestedFieldArray(Params, TEXT("member_names"), StructType, FieldNames);

		return FieldNames;
	}

	void PrimeVisibleStructFields(UK2Node_SetFieldsInStruct* SetFieldsNode, const TSet<FName>& FieldNames)
	{
		if (!SetFieldsNode || FieldNames.IsEmpty())
		{
			return;
		}

		SetFieldsNode->ShowPinForProperties.Reset();
		for (const FName& FieldName : FieldNames)
		{
			FOptionalPinFromProperty& PinRecord = SetFieldsNode->ShowPinForProperties.AddDefaulted_GetRef();
			PinRecord.PropertyName = FieldName;
			PinRecord.bShowPin = true;
			PinRecord.bCanToggleVisibility = true;
		}
	}
}

UK2Node* FDataNodeCreator::CreateVariableGetNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	FString VariableName;
	if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
	{
		return nullptr;
	}

	UK2Node_VariableGet* VarGetNode = NewObject<UK2Node_VariableGet>(Graph);
	if (!VarGetNode)
	{
		return nullptr;
	}

	FString TargetBlueprintPath;
	if (Params->TryGetStringField(TEXT("target_blueprint"), TargetBlueprintPath))
	{
		if (UBlueprint* TargetBlueprint = LoadObject<UBlueprint>(nullptr, *TargetBlueprintPath))
		{
			VarGetNode->VariableReference.SetExternalMember(FName(*VariableName), TargetBlueprint->GeneratedClass);
		}
		else
		{
			VarGetNode->VariableReference.SetSelfMember(FName(*VariableName));
		}
	}
	else
	{
		VarGetNode->VariableReference.SetSelfMember(FName(*VariableName));
	}

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	VarGetNode->NodePosX = static_cast<int32>(PosX);
	VarGetNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(VarGetNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(VarGetNode, Graph);

	return VarGetNode;
}

UK2Node* FDataNodeCreator::CreateVariableSetNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	FString VariableName;
	if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
	{
		return nullptr;
	}

	UK2Node_VariableSet* VarSetNode = NewObject<UK2Node_VariableSet>(Graph);
	if (!VarSetNode)
	{
		return nullptr;
	}

	VarSetNode->VariableReference.SetSelfMember(FName(*VariableName));

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	VarSetNode->NodePosX = static_cast<int32>(PosX);
	VarSetNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(VarSetNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(VarSetNode, Graph);

	return VarSetNode;
}


UK2Node* FDataNodeCreator::CreateMakeArrayNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	UK2Node_MakeArray* MakeArrayNode = NewObject<UK2Node_MakeArray>(Graph);
	if (!MakeArrayNode)
	{
		return nullptr;
	}

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	MakeArrayNode->NodePosX = static_cast<int32>(PosX);
	MakeArrayNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(MakeArrayNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(MakeArrayNode, Graph);

	return MakeArrayNode;
}

UK2Node* FDataNodeCreator::CreateMakeStructNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	FString StructPath;
	if (!Params->TryGetStringField(TEXT("struct_path"), StructPath))
	{
		Params->TryGetStringField(TEXT("target_blueprint"), StructPath);
	}

	UScriptStruct* StructType = ResolveScriptStruct(StructPath);
	if (!StructType)
	{
		return nullptr;
	}

	UK2Node_MakeStruct* MakeStructNode = NewObject<UK2Node_MakeStruct>(Graph);
	if (!MakeStructNode)
	{
		return nullptr;
	}

	MakeStructNode->StructType = StructType;

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	MakeStructNode->NodePosX = static_cast<int32>(PosX);
	MakeStructNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(MakeStructNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(MakeStructNode, Graph);

	return MakeStructNode;
}

UK2Node* FDataNodeCreator::CreateBreakStructNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	FString StructPath;
	if (!Params->TryGetStringField(TEXT("struct_path"), StructPath))
	{
		Params->TryGetStringField(TEXT("target_blueprint"), StructPath);
	}

	UScriptStruct* StructType = ResolveScriptStruct(StructPath);
	if (!StructType)
	{
		return nullptr;
	}

	UK2Node_BreakStruct* BreakStructNode = NewObject<UK2Node_BreakStruct>(Graph);
	if (!BreakStructNode)
	{
		return nullptr;
	}

	BreakStructNode->StructType = StructType;

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	BreakStructNode->NodePosX = static_cast<int32>(PosX);
	BreakStructNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(BreakStructNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(BreakStructNode, Graph);

	return BreakStructNode;
}

UK2Node* FDataNodeCreator::CreateSetFieldsInStructNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	FString StructPath;
	if (!Params->TryGetStringField(TEXT("struct_path"), StructPath))
	{
		Params->TryGetStringField(TEXT("target_blueprint"), StructPath);
	}

	UScriptStruct* StructType = ResolveScriptStruct(StructPath);
	if (!StructType)
	{
		return nullptr;
	}

	UK2Node_SetFieldsInStruct* SetFieldsNode = NewObject<UK2Node_SetFieldsInStruct>(Graph);
	if (!SetFieldsNode)
	{
		return nullptr;
	}

	SetFieldsNode->StructType = StructType;
	PrimeVisibleStructFields(SetFieldsNode, GetRequestedStructFields(Params, StructType));

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	SetFieldsNode->NodePosX = static_cast<int32>(PosX);
	SetFieldsNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(SetFieldsNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(SetFieldsNode, Graph);

	return SetFieldsNode;
}

UK2Node* FDataNodeCreator::CreateArrayGetNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	UK2Node_GetArrayItem* ArrayGetNode = NewObject<UK2Node_GetArrayItem>(Graph);
	if (!ArrayGetNode)
	{
		return nullptr;
	}

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	ArrayGetNode->NodePosX = static_cast<int32>(PosX);
	ArrayGetNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(ArrayGetNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(ArrayGetNode, Graph);

	return ArrayGetNode;
}

UK2Node* FDataNodeCreator::CreateCallArrayFunctionNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	FString TargetFunction;
	if (!Params->TryGetStringField(TEXT("target_function"), TargetFunction))
	{
		return nullptr;
	}

	UFunction* TargetFunc = UKismetArrayLibrary::StaticClass()->FindFunctionByName(FName(*TargetFunction));
	if (!TargetFunc)
	{
		return nullptr;
	}

	UK2Node_CallArrayFunction* CallArrayNode = NewObject<UK2Node_CallArrayFunction>(Graph);
	if (!CallArrayNode)
	{
		return nullptr;
	}

	CallArrayNode->SetFromFunction(TargetFunc);

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	CallArrayNode->NodePosX = static_cast<int32>(PosX);
	CallArrayNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(CallArrayNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(CallArrayNode, Graph);

	return CallArrayNode;
}

