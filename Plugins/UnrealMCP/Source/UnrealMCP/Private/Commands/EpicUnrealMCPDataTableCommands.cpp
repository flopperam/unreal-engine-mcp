#include "Commands/EpicUnrealMCPDataTableCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Engine/DataTable.h"
#include "Engine/EngineTypes.h"
#include "UObject/Package.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"

FEpicUnrealMCPDataTableCommands::FEpicUnrealMCPDataTableCommands()
{
}

FEpicUnrealMCPDataTableCommands::~FEpicUnrealMCPDataTableCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPDataTableCommands::*)(const TSharedPtr<FJsonObject>&);
    static const TMap<FString, Handler> Dispatch = {
        {TEXT("create_data_table"), &FEpicUnrealMCPDataTableCommands::HandleCreateDataTable},
        {TEXT("import_csv_to_data_table"), &FEpicUnrealMCPDataTableCommands::HandleImportCSVToDataTable},
        {TEXT("add_data_table_row"), &FEpicUnrealMCPDataTableCommands::HandleAddDataTableRow},
        {TEXT("delete_data_table_row"), &FEpicUnrealMCPDataTableCommands::HandleDeleteDataTableRow},
        {TEXT("update_data_table_row"), &FEpicUnrealMCPDataTableCommands::HandleUpdateDataTableRow},
        {TEXT("export_data_table_csv"), &FEpicUnrealMCPDataTableCommands::HandleExportDataTableCSV},
        {TEXT("export_data_table_json"), &FEpicUnrealMCPDataTableCommands::HandleExportDataTableJSON},
    };

    const Handler* H = Dispatch.Find(CommandType);
    if (H)
    {
        return (this->*(*H))(Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown data table command: %s"), *CommandType));
}

UScriptStruct* FEpicUnrealMCPDataTableCommands::FindRowStruct(const FString& StructPath, FString& OutError)
{
    // Try loading as an object path first (e.g., /Game/MyStruct.MyStruct)
    UScriptStruct* RowStruct = LoadObject<UScriptStruct>(nullptr, *StructPath);
    if (RowStruct)
    {
        return RowStruct;
    }

    // Try finding by name in memory
    RowStruct = FindObject<UScriptStruct>(nullptr, *StructPath);
    if (RowStruct)
    {
        return RowStruct;
    }

    OutError = FString::Printf(TEXT("Row struct not found: %s"), *StructPath);
    return nullptr;
}

bool FEpicUnrealMCPDataTableCommands::SetStructProperty(void* StructMemory, FProperty* Property, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError)
{
    if (!Property || !JsonValue.IsValid())
    {
        OutError = TEXT("Invalid property or JSON value");
        return false;
    }

    void* PropertyValue = Property->ContainerPtrToValuePtr<void>(StructMemory);

    if (FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
    {
        if (NumericProp->IsInteger())
        {
            NumericProp->SetIntPropertyValue(PropertyValue, static_cast<int64>(JsonValue->AsNumber()));
        }
        else
        {
            NumericProp->SetFloatingPointPropertyValue(PropertyValue, JsonValue->AsNumber());
        }
        return true;
    }
    else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        BoolProp->SetPropertyValue(PropertyValue, JsonValue->AsBool());
        return true;
    }
    else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        StrProp->SetPropertyValue(PropertyValue, JsonValue->AsString());
        return true;
    }
    else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        NameProp->SetPropertyValue(PropertyValue, FName(*JsonValue->AsString()));
        return true;
    }
    else if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
    {
        TextProp->SetPropertyValue(PropertyValue, FText::FromString(JsonValue->AsString()));
        return true;
    }
    else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        if (StructProp->Struct == TBaseStructure<FVector>::Get())
        {
            const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
            if (JsonValue->TryGetArray(Array) && Array->Num() >= 3)
            {
                FVector* Vec = static_cast<FVector*>(PropertyValue);
                Vec->X = static_cast<float>((*Array)[0]->AsNumber());
                Vec->Y = static_cast<float>((*Array)[1]->AsNumber());
                Vec->Z = static_cast<float>((*Array)[2]->AsNumber());
                return true;
            }
            OutError = TEXT("FVector property expects [X, Y, Z] array");
            return false;
        }
        else if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
        {
            const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
            if (JsonValue->TryGetArray(Array) && Array->Num() >= 3)
            {
                FLinearColor* Color = static_cast<FLinearColor*>(PropertyValue);
                Color->R = static_cast<float>((*Array)[0]->AsNumber());
                Color->G = static_cast<float>((*Array)[1]->AsNumber());
                Color->B = static_cast<float>((*Array)[2]->AsNumber());
                if (Array->Num() >= 4)
                {
                    Color->A = static_cast<float>((*Array)[3]->AsNumber());
                }
                return true;
            }
            OutError = TEXT("FLinearColor property expects [R, G, B, A] array");
            return false;
        }
        OutError = FString::Printf(TEXT("Unsupported struct type for property %s"), *Property->GetName());
        return false;
    }

    OutError = FString::Printf(TEXT("Unsupported property type for %s"), *Property->GetName());
    return false;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleCreateDataTable(const TSharedPtr<FJsonObject>& Params)
{
    FString TablePath;
    if (!Params->TryGetStringField(TEXT("table_path"), TablePath) || TablePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'table_path' parameter (e.g. /Game/Data/MyTable)"));
    }

    FString RowStructPath;
    if (!Params->TryGetStringField(TEXT("row_struct_path"), RowStructPath) || RowStructPath.IsEmpty())
    {
        // Default to a common built-in struct if available, otherwise error
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'row_struct_path' parameter"));
    }

    FString Error;
    UScriptStruct* RowStruct = FindRowStruct(RowStructPath, Error);
    if (!RowStruct)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString TableName = FPaths::GetBaseFilename(TablePath);
    UPackage* Package = CreatePackage(*TablePath);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for data table"));
    }

    UDataTable* NewDataTable = NewObject<UDataTable>(Package, FName(*TableName), RF_Public | RF_Standalone);
    if (!NewDataTable)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create DataTable object"));
    }

    NewDataTable->RowStruct = RowStruct;
    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewDataTable);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("table_path"), TablePath);
    Result->SetStringField(TEXT("table_name"), TableName);
    Result->SetStringField(TEXT("row_struct"), RowStruct->GetName());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleImportCSVToDataTable(const TSharedPtr<FJsonObject>& Params)
{
    FString TablePath;
    if (!Params->TryGetStringField(TEXT("table_path"), TablePath) || TablePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'table_path' parameter"));
    }

    FString CSVContent;
    if (!Params->TryGetStringField(TEXT("csv_content"), CSVContent) || CSVContent.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'csv_content' parameter"));
    }

    UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *TablePath);
    if (!DataTable)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("DataTable not found: %s"), *TablePath));
    }

    TArray<FString> Errors = DataTable->CreateTableFromCSVString(CSVContent);
    if (Errors.Num() > 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("CSV import errors: %s"), *FString::Join(Errors, TEXT("; "))));
    }

    DataTable->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("table_path"), TablePath);
    Result->SetNumberField(TEXT("row_count"), DataTable->GetRowMap().Num());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleAddDataTableRow(const TSharedPtr<FJsonObject>& Params)
{
    FString TablePath;
    if (!Params->TryGetStringField(TEXT("table_path"), TablePath) || TablePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'table_path' parameter"));
    }

    FString RowName;
    if (!Params->TryGetStringField(TEXT("row_name"), RowName) || RowName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'row_name' parameter"));
    }

    const TSharedPtr<FJsonObject>* RowDataObj = nullptr;
    if (!Params->TryGetObjectField(TEXT("row_data"), RowDataObj) || !RowDataObj->IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or invalid 'row_data' parameter (JSON object)"));
    }

    UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *TablePath);
    if (!DataTable)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("DataTable not found: %s"), *TablePath));
    }

    UScriptStruct* RowStruct = DataTable->RowStruct;
    if (!RowStruct)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("DataTable has no RowStruct assigned"));
    }

    // Allocate memory for the row struct
    uint8* RowMemory = static_cast<uint8*>(FMemory::Malloc(RowStruct->GetStructureSize()));
    RowStruct->InitializeStruct(RowMemory);

    // Set properties from JSON
    for (const auto& Pair : (*RowDataObj)->Values)
    {
        FProperty* Property = RowStruct->FindPropertyByName(FName(*Pair.Key));
        if (!Property)
        {
            FMemory::Free(RowMemory);
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Property '%s' not found in row struct"), *Pair.Key));
        }

        FString Error;
        if (!SetStructProperty(RowMemory, Property, Pair.Value, Error))
        {
            FMemory::Free(RowMemory);
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
        }
    }

    // Add row to table (this copies the memory)
    DataTable->AddRow(FName(*RowName), RowMemory);

    // Free our temporary allocation
    RowStruct->DestroyStruct(RowMemory);
    FMemory::Free(RowMemory);

    DataTable->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("table_path"), TablePath);
    Result->SetStringField(TEXT("row_name"), RowName);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleDeleteDataTableRow(const TSharedPtr<FJsonObject>& Params)
{
    FString TablePath;
    if (!Params->TryGetStringField(TEXT("table_path"), TablePath) || TablePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'table_path' parameter"));
    }

    FString RowName;
    if (!Params->TryGetStringField(TEXT("row_name"), RowName) || RowName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'row_name' parameter"));
    }

    UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *TablePath);
    if (!DataTable)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("DataTable not found: %s"), *TablePath));
    }

    const TMap<FName, uint8*>& RowMap = DataTable->GetRowMap();
    if (!RowMap.Contains(FName(*RowName)))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Row '%s' not found in DataTable"), *RowName));
    }

    DataTable->RemoveRow(FName(*RowName));
    DataTable->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("table_path"), TablePath);
    Result->SetStringField(TEXT("row_name"), RowName);
    Result->SetNumberField(TEXT("row_count"), DataTable->GetRowMap().Num());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleUpdateDataTableRow(const TSharedPtr<FJsonObject>& Params)
{
    FString TablePath;
    if (!Params->TryGetStringField(TEXT("table_path"), TablePath) || TablePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'table_path' parameter"));
    }

    FString RowName;
    if (!Params->TryGetStringField(TEXT("row_name"), RowName) || RowName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'row_name' parameter"));
    }

    const TSharedPtr<FJsonObject>* RowDataObj = nullptr;
    if (!Params->TryGetObjectField(TEXT("row_data"), RowDataObj) || !RowDataObj->IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or invalid 'row_data' parameter (JSON object)"));
    }

    UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *TablePath);
    if (!DataTable)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("DataTable not found: %s"), *TablePath));
    }

    UScriptStruct* RowStruct = DataTable->RowStruct;
    if (!RowStruct)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("DataTable has no RowStruct assigned"));
    }

    // Remove existing row if present
    const TMap<FName, uint8*>& RowMap = DataTable->GetRowMap();
    const bool bRowExisted = RowMap.Contains(FName(*RowName));
    if (bRowExisted)
    {
        DataTable->RemoveRow(FName(*RowName));
    }

    // Allocate memory for the row struct
    uint8* RowMemory = static_cast<uint8*>(FMemory::Malloc(RowStruct->GetStructureSize()));
    RowStruct->InitializeStruct(RowMemory);

    // Set properties from JSON
    for (const auto& Pair : (*RowDataObj)->Values)
    {
        FProperty* Property = RowStruct->FindPropertyByName(FName(*Pair.Key));
        if (!Property)
        {
            RowStruct->DestroyStruct(RowMemory);
            FMemory::Free(RowMemory);
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Property '%s' not found in row struct"), *Pair.Key));
        }

        FString Error;
        if (!SetStructProperty(RowMemory, Property, Pair.Value, Error))
        {
            RowStruct->DestroyStruct(RowMemory);
            FMemory::Free(RowMemory);
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
        }
    }

    // Add row to table (this copies the memory)
    DataTable->AddRow(FName(*RowName), RowMemory);

    // Free our temporary allocation
    RowStruct->DestroyStruct(RowMemory);
    FMemory::Free(RowMemory);

    DataTable->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("table_path"), TablePath);
    Result->SetStringField(TEXT("row_name"), RowName);
    Result->SetBoolField(TEXT("row_existed"), bRowExisted);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleExportDataTableCSV(const TSharedPtr<FJsonObject>& Params)
{
    FString TablePath;
    if (!Params->TryGetStringField(TEXT("table_path"), TablePath) || TablePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'table_path' parameter"));
    }

    UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *TablePath);
    if (!DataTable)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("DataTable not found: %s"), *TablePath));
    }

    FString CSVContent;
    UDataTable::SaveTableToCSVString(*DataTable, CSVContent);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("table_path"), TablePath);
    Result->SetStringField(TEXT("csv_content"), CSVContent);
    Result->SetNumberField(TEXT("row_count"), DataTable->GetRowMap().Num());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleExportDataTableJSON(const TSharedPtr<FJsonObject>& Params)
{
    FString TablePath;
    if (!Params->TryGetStringField(TEXT("table_path"), TablePath) || TablePath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'table_path' parameter"));
    }

    UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *TablePath);
    if (!DataTable)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("DataTable not found: %s"), *TablePath));
    }

    FString JSONContent;
    UDataTable::SaveTableToJSONString(*DataTable, JSONContent);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("table_path"), TablePath);
    Result->SetStringField(TEXT("json_content"), JSONContent);
    Result->SetNumberField(TEXT("row_count"), DataTable->GetRowMap().Num());
    return Result;
}
