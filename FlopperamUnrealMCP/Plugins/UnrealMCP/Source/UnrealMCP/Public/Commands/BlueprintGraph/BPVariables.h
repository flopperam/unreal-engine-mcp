// Blueprint variable creation and management utilities

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

    /**
     * Modifie les propriétés d'une variable existante sans la supprimer
     * @param Params JSON contenant blueprint_name, variable_name, et propriétés optionnelles:
     *        is_blueprint_writable, is_public, is_editable_in_instance,
     *        tooltip, category, default_value, is_config, expose_on_spawn,
     *        var_name, var_type, friendly_name, replication
     * @return JSON avec success et propriétés modifiées
     */
    static TSharedPtr<FJsonObject> SetVariableProperties(const TSharedPtr<FJsonObject>& Params);

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