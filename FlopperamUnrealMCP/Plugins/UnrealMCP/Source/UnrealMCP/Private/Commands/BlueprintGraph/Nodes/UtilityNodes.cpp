#include "Commands/BlueprintGraph/Nodes/UtilityNodes.h"
#include "Commands/BlueprintGraph/Nodes/NodeCreatorUtils.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Select.h"
#include "K2Node_SpawnActorFromClass.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetInputLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Json.h"

namespace
{
UClass* ResolveFunctionOwnerClass(const FString& ClassName)
{
	if (ClassName.IsEmpty())
	{
		return nullptr;
	}

	if (UClass* TargetClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, *ClassName)))
	{
		return TargetClass;
	}

	if (UClass* TargetClass = LoadObject<UClass>(nullptr, *ClassName))
	{
		return TargetClass;
	}

	if (UBlueprint* TargetBlueprint = LoadObject<UBlueprint>(nullptr, *ClassName))
	{
		return TargetBlueprint->GeneratedClass;
	}

	if (ClassName.Contains(TEXT("KismetInputLibrary")) || ClassName.Equals(TEXT("KismetInputLibrary"), ESearchCase::IgnoreCase))
	{
		return UKismetInputLibrary::StaticClass();
	}
	if (ClassName.Contains(TEXT("KismetMathLibrary")) || ClassName.Equals(TEXT("KismetMathLibrary"), ESearchCase::IgnoreCase))
	{
		return UKismetMathLibrary::StaticClass();
	}
	if (ClassName.Contains(TEXT("KismetSystemLibrary")) || ClassName.Equals(TEXT("KismetSystemLibrary"), ESearchCase::IgnoreCase))
	{
		return UKismetSystemLibrary::StaticClass();
	}
	if (ClassName.Contains(TEXT("WidgetBlueprintLibrary")) || ClassName.Equals(TEXT("WidgetBlueprintLibrary"), ESearchCase::IgnoreCase))
	{
		return UWidgetBlueprintLibrary::StaticClass();
	}
	if (ClassName.Contains(TEXT("WidgetLayoutLibrary")) || ClassName.Equals(TEXT("WidgetLayoutLibrary"), ESearchCase::IgnoreCase))
	{
		return UWidgetLayoutLibrary::StaticClass();
	}
	if (ClassName.Contains(TEXT("SlateBlueprintLibrary")) || ClassName.Equals(TEXT("SlateBlueprintLibrary"), ESearchCase::IgnoreCase))
	{
		return USlateBlueprintLibrary::StaticClass();
	}
	if (ClassName.Contains(TEXT("GameplayStatics")) || ClassName.Equals(TEXT("GameplayStatics"), ESearchCase::IgnoreCase))
	{
		return UGameplayStatics::StaticClass();
	}

	return nullptr;
}

UFunction* FindCommonBlueprintFunction(const FName FunctionName)
{
	UClass* CommonClasses[] = {
		UKismetSystemLibrary::StaticClass(),
		UKismetMathLibrary::StaticClass(),
		UKismetInputLibrary::StaticClass(),
		UWidgetBlueprintLibrary::StaticClass(),
		UWidgetLayoutLibrary::StaticClass(),
		USlateBlueprintLibrary::StaticClass(),
		UGameplayStatics::StaticClass()
	};

	for (UClass* CommonClass : CommonClasses)
	{
		if (CommonClass)
		{
			if (UFunction* Function = CommonClass->FindFunctionByName(FunctionName))
			{
				return Function;
			}
		}
	}

	return nullptr;
}
}

UK2Node* FUtilityNodeCreator::CreatePrintNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	UK2Node_CallFunction* PrintNode = NewObject<UK2Node_CallFunction>(Graph);
	if (!PrintNode)
	{
		return nullptr;
	}

	UFunction* PrintFunc = UKismetSystemLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString)
	);

	if (!PrintFunc)
	{
		return nullptr;
	}

	// Set function reference BEFORE initialization
	PrintNode->SetFromFunction(PrintFunc);

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	PrintNode->NodePosX = static_cast<int32>(PosX);
	PrintNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(PrintNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(PrintNode, Graph);

	// Set message if provided AFTER initialization
	FString Message;
	if (Params->TryGetStringField(TEXT("message"), Message))
	{
		UEdGraphPin* InStringPin = PrintNode->FindPin(TEXT("InString"));
		if (InStringPin)
		{
			InStringPin->DefaultValue = Message;
		}
	}

	return PrintNode;
}

UK2Node* FUtilityNodeCreator::CreateCallFunctionNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	// Get target function name
	FString TargetFunction;
	if (!Params->TryGetStringField(TEXT("target_function"), TargetFunction))
	{
		return nullptr;
	}

	UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
	if (!CallNode)
	{
		return nullptr;
	}

	// Find the function to call
	UFunction* TargetFunc = nullptr;
	FString ClassName;
	if (Params->TryGetStringField(TEXT("target_class"), ClassName))
	{
		UClass* TargetClass = ResolveFunctionOwnerClass(ClassName);
		if (TargetClass)
		{
			TargetFunc = TargetClass->FindFunctionByName(FName(*TargetFunction));
		}
	}
	else if (Params->TryGetStringField(TEXT("target_blueprint"), ClassName))
	{
		UClass* TargetClass = ResolveFunctionOwnerClass(ClassName);

		if (TargetClass)
		{
			TargetFunc = TargetClass->FindFunctionByName(FName(*TargetFunction));
		}
	}
	else
	{
		// Try common Unreal classes
		TargetFunc = FindCommonBlueprintFunction(FName(*TargetFunction));
	}

	if (!TargetFunc)
	{
		TargetFunc = FindCommonBlueprintFunction(FName(*TargetFunction));
	}

	if (!TargetFunc)
	{
		return nullptr;
	}

	// Set function reference BEFORE initialization
	CallNode->SetFromFunction(TargetFunc);

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	CallNode->NodePosX = static_cast<int32>(PosX);
	CallNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(CallNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(CallNode, Graph);

	return CallNode;
}

UK2Node* FUtilityNodeCreator::CreateSelectNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	UK2Node_Select* SelectNode = NewObject<UK2Node_Select>(Graph);
	if (!SelectNode)
	{
		return nullptr;
	}

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	SelectNode->NodePosX = static_cast<int32>(PosX);
	SelectNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(SelectNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(SelectNode, Graph);

	return SelectNode;
}

UK2Node* FUtilityNodeCreator::CreateSpawnActorNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	UK2Node_SpawnActorFromClass* SpawnActorNode = NewObject<UK2Node_SpawnActorFromClass>(Graph);
	if (!SpawnActorNode)
	{
		return nullptr;
	}

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	SpawnActorNode->NodePosX = static_cast<int32>(PosX);
	SpawnActorNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(SpawnActorNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(SpawnActorNode, Graph);

	return SpawnActorNode;
}

