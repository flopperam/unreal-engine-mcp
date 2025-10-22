// BPVariables.h
// Created by: Zoscran
// Date: 2025-10-09
// Description: Gestion des variables Blueprint

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Forward declarations
struct FEdGraphPinType;
struct FBPVariableDescription;

/**
 * Classe utilitaire pour créer et gérer des variables Blueprint
 */
class UNREALMCP_API FBPVariables
{
public:
    /**
     * Crée une nouvelle variable dans un Blueprint
     * @param Params JSON contenant blueprint_name, variable_name, variable_type, default_value, is_public, tooltip, category
     * @return JSON avec success et détails de la variable
     */
    static TSharedPtr<FJsonObject> CreateVariable(const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * Convertit un string de type en FEdGraphPinType
     * Types supportés : bool, int, float, string, vector, rotator
     */
    static FEdGraphPinType GetPinTypeFromString(const FString& TypeString);

    /**
     * Définit la valeur par défaut d'une variable
     */
    static void SetDefaultValue(FBPVariableDescription& Variable, const TSharedPtr<FJsonValue>& Value);
};