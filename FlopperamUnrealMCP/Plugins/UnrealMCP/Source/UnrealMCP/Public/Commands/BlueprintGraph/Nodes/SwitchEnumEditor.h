// Header for editing SwitchEnum nodes (enum type management)

#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UK2Node;
class UEdGraph;

/**
 * FPinManagementEditor handles pin management for UK2Node_SwitchEnum.
 * This node automatically generates execution output pins for each enum value.
 */
class FSwitchEnumEditor
{
public:
	/**
	 * Set enum type on a SwitchEnum node.
	 * This automatically generates execution output pins for each enum value.
	 *
	 * @param Node The UK2Node_SwitchEnum node to modify
	 * @param Graph The graph containing the node
	 * @param EnumPath Full path to enum type (e.g., "/Script/MyProject.EMyEnum" or "/Game/Enums/EMyEnum")
	 * @return true if successful, false otherwise
	 */
	static bool SetEnumType(UK2Node* Node, UEdGraph* Graph, const FString& EnumPath);

private:
	/**
	 * Find an enum by path
	 */
	static UEnum* FindEnumByPath(const FString& EnumPath);
};
