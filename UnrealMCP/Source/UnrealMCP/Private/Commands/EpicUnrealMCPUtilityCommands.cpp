#include "Commands/EpicUnrealMCPUtilityCommands.h"
#include "PythonScriptLibrary.h" // For Python execution

TSharedPtr<FJsonObject> FEpicUnrealMCPUtilityCommands::HandleCommand(const FString &CommandType, const TSharedPtr<FJsonObject> &Params)
{
    if (CommandType == TEXT("execute_python_script"))
    {
        return ExecutePythonScript(Params);
    }

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), TEXT("Unknown utility command"));
    return Response;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPUtilityCommands::ExecutePythonScript(const TSharedPtr<FJsonObject> &Params)
{
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    FString Script = Params->GetStringField(TEXT("script"));

    if (Script.IsEmpty())
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Script parameter cannot be empty."));
        return Response;
    }

    FString Output;
    bool bSuccess = UPythonScriptLibrary::ExecutePythonCommand(Script, Output);

    Response->SetBoolField(TEXT("success"), bSuccess);
    Response->SetStringField(TEXT("output"), Output);

    // Security Warning
    UE_LOG(LogTemp, Warning, TEXT("Executed Python script via MCP. This can be a security risk. Ensure the source is trusted."));

    return Response;
}