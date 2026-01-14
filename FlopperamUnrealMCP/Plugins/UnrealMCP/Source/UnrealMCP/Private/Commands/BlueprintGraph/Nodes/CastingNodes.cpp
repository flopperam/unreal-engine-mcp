#include "Commands/BlueprintGraph/Nodes/CastingNodes.h"
#include "Commands/BlueprintGraph/Nodes/NodeCreatorUtils.h"
#include "Json.h"
#include "K2Node_CastByteToEnum.h"
#include "K2Node_ClassDynamicCast.h"
#include "K2Node_DynamicCast.h"


UK2Node *FCastingNodeCreator::CreateDynamicCastNode(
    UEdGraph *Graph, const TSharedPtr<FJsonObject> &Params) {
  if (!Graph || !Params.IsValid()) {
    return nullptr;
  }

  UK2Node_DynamicCast *DynamicCastNode = NewObject<UK2Node_DynamicCast>(Graph);
  if (!DynamicCastNode) {
    return nullptr;
  }

  // Set target class BEFORE initialization
  FString TargetClass;
  if (Params->TryGetStringField(TEXT("target_class"), TargetClass)) {
    UClass *CastClass = Cast<UClass>(
        StaticFindObject(UClass::StaticClass(), nullptr, *TargetClass));
    if (CastClass) {
      DynamicCastNode->TargetType = CastClass;
    }
  }

  double PosX, PosY;
  FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
  DynamicCastNode->NodePosX = static_cast<int32>(PosX);
  DynamicCastNode->NodePosY = static_cast<int32>(PosY);

  Graph->AddNode(DynamicCastNode, true, false);
  FNodeCreatorUtils::InitializeK2Node(DynamicCastNode, Graph);

  return DynamicCastNode;
}

UK2Node *FCastingNodeCreator::CreateClassDynamicCastNode(
    UEdGraph *Graph, const TSharedPtr<FJsonObject> &Params) {
  if (!Graph || !Params.IsValid()) {
    return nullptr;
  }

  UK2Node_ClassDynamicCast *ClassDynamicCastNode =
      NewObject<UK2Node_ClassDynamicCast>(Graph);
  if (!ClassDynamicCastNode) {
    return nullptr;
  }

  // Set target class BEFORE initialization
  FString TargetClass;
  if (Params->TryGetStringField(TEXT("target_class"), TargetClass)) {
    UClass *CastClass = Cast<UClass>(
        StaticFindObject(UClass::StaticClass(), nullptr, *TargetClass));
    if (CastClass) {
      ClassDynamicCastNode->TargetType = CastClass;
    }
  }

  double PosX, PosY;
  FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
  ClassDynamicCastNode->NodePosX = static_cast<int32>(PosX);
  ClassDynamicCastNode->NodePosY = static_cast<int32>(PosY);

  Graph->AddNode(ClassDynamicCastNode, true, false);
  FNodeCreatorUtils::InitializeK2Node(ClassDynamicCastNode, Graph);

  return ClassDynamicCastNode;
}

UK2Node *FCastingNodeCreator::CreateCastByteToEnumNode(
    UEdGraph *Graph, const TSharedPtr<FJsonObject> &Params) {
  if (!Graph || !Params.IsValid()) {
    return nullptr;
  }

  UK2Node_CastByteToEnum *CastByteNode =
      NewObject<UK2Node_CastByteToEnum>(Graph);
  if (!CastByteNode) {
    return nullptr;
  }

  // Set enum type BEFORE initialization
  FString EnumType;
  if (Params->TryGetStringField(TEXT("enum_type"), EnumType)) {
    UEnum *TargetEnum =
        Cast<UEnum>(StaticFindObject(UEnum::StaticClass(), nullptr, *EnumType));
    if (TargetEnum) {
      CastByteNode->Enum = TargetEnum;
    }
  }

  double PosX, PosY;
  FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
  CastByteNode->NodePosX = static_cast<int32>(PosX);
  CastByteNode->NodePosY = static_cast<int32>(PosY);

  Graph->AddNode(CastByteNode, true, false);
  FNodeCreatorUtils::InitializeK2Node(CastByteNode, Graph);

  return CastByteNode;
}
