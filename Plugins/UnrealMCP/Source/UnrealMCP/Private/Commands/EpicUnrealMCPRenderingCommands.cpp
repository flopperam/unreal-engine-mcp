#include "Commands/EpicUnrealMCPRenderingCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "HAL/IConsoleManager.h"
#include "Editor.h"
#include "ShaderCompiler.h"

FEpicUnrealMCPRenderingCommands::FEpicUnrealMCPRenderingCommands()
{
}

FEpicUnrealMCPRenderingCommands::~FEpicUnrealMCPRenderingCommands()
{
}

bool FEpicUnrealMCPRenderingCommands::IsInPIE() const
{
    return GEditor && GEditor->PlayWorld != nullptr;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::CreateCVarResult(const FString& CVarName, bool bSuccess, const FString& Error)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetStringField(TEXT("cvar"), CVarName);
    if (!Error.IsEmpty())
    {
        Result->SetStringField(TEXT("error"), Error);
    }
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::SetCVarInt(const FString& CVarName, int32 Value)
{
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
    if (!CVar)
    {
        return CreateCVarResult(CVarName, false, FString::Printf(TEXT("CVar not found: %s"), *CVarName));
    }
    CVar->Set(Value);
    return CreateCVarResult(CVarName, true);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::SetCVarFloat(const FString& CVarName, float Value)
{
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
    if (!CVar)
    {
        return CreateCVarResult(CVarName, false, FString::Printf(TEXT("CVar not found: %s"), *CVarName));
    }
    CVar->Set(Value);
    return CreateCVarResult(CVarName, true);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::GetCVarValue(const FString& CVarName)
{
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
    if (!CVar)
    {
        return CreateCVarResult(CVarName, false, FString::Printf(TEXT("CVar not found: %s"), *CVarName));
    }

    TSharedPtr<FJsonObject> Result = CreateCVarResult(CVarName, true);
    Result->SetNumberField(TEXT("value"), CVar->GetFloat());
    Result->SetStringField(TEXT("string_value"), CVar->GetString());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPRenderingCommands::*)(const TSharedPtr<FJsonObject>&);
    static const TMap<FString, Handler> Dispatch = {
        {TEXT("set_global_illumination"), &FEpicUnrealMCPRenderingCommands::HandleSetGlobalIllumination},
        {TEXT("set_lumen_enabled"), &FEpicUnrealMCPRenderingCommands::HandleSetLumenEnabled},
        {TEXT("set_lumen_scene_detail"), &FEpicUnrealMCPRenderingCommands::HandleSetLumenSceneDetail},
        {TEXT("set_lumen_reflection_quality"), &FEpicUnrealMCPRenderingCommands::HandleSetLumenReflectionQuality},
        {TEXT("set_hardware_ray_tracing"), &FEpicUnrealMCPRenderingCommands::HandleSetHardwareRayTracing},
        {TEXT("set_path_tracing"), &FEpicUnrealMCPRenderingCommands::HandleSetPathTracing},
        {TEXT("set_virtual_shadow_maps"), &FEpicUnrealMCPRenderingCommands::HandleSetVirtualShadowMaps},
        {TEXT("set_shadow_quality"), &FEpicUnrealMCPRenderingCommands::HandleSetShadowQuality},
        {TEXT("set_anti_aliasing"), &FEpicUnrealMCPRenderingCommands::HandleSetAntiAliasing},
        {TEXT("set_tsr_settings"), &FEpicUnrealMCPRenderingCommands::HandleSetTSRSettings},
        {TEXT("set_upscaler"), &FEpicUnrealMCPRenderingCommands::HandleSetUpscaler},
        {TEXT("set_nanite_visualization"), &FEpicUnrealMCPRenderingCommands::HandleSetNaniteVisualization},
        {TEXT("get_shader_compile_status"), &FEpicUnrealMCPRenderingCommands::HandleGetShaderCompileStatus},
    };

    const Handler* H = Dispatch.Find(CommandType);
    if (H)
    {
        return (this->*(*H))(Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown rendering command: %s"), *CommandType));
}

// ------------------------------------------------------------------
// Global Illumination
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleSetGlobalIllumination(const TSharedPtr<FJsonObject>& Params)
{
    FString Method;
    if (!Params->TryGetStringField(TEXT("method"), Method))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'method' parameter. Use: Off, Lumen, ScreenSpace, RayTraced"));
    }

    int32 Value = 1;
    if (Method.Equals(TEXT("Off"), ESearchCase::IgnoreCase)) Value = 0;
    else if (Method.Equals(TEXT("Lumen"), ESearchCase::IgnoreCase)) Value = 1;
    else if (Method.Equals(TEXT("ScreenSpace"), ESearchCase::IgnoreCase)) Value = 2;
    else if (Method.Equals(TEXT("RayTraced"), ESearchCase::IgnoreCase)) Value = 3;
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown GI method: %s"), *Method));
    }

    return SetCVarInt(TEXT("r.DynamicGlobalIlluminationMethod"), Value);
}

// ------------------------------------------------------------------
// Lumen
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleSetLumenEnabled(const TSharedPtr<FJsonObject>& Params)
{
    if (IsInPIE())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot change Lumen settings while in PIE. Stop PIE first."));
    }

    bool bEnabled = true;
    if (!Params->TryGetBoolField(TEXT("enabled"), bEnabled))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'enabled' parameter"));
    }

    int32 Value = bEnabled ? 1 : 0;
    SetCVarInt(TEXT("r.Lumen.Reflections.Allow"), Value);
    return SetCVarInt(TEXT("r.Lumen.DiffuseIndirect.Allow"), Value);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleSetLumenSceneDetail(const TSharedPtr<FJsonObject>& Params)
{
    double CardRefreshFraction = 0.0;
    int32 RadiosityIterations = -1;
    Params->TryGetNumberField(TEXT("card_refresh_fraction"), CardRefreshFraction);
    Params->TryGetNumberField(TEXT("radiosity_iterations"), RadiosityIterations);

    if (CardRefreshFraction > 0.0)
    {
        SetCVarFloat(TEXT("r.Lumen.Scene.CardCaptureRefreshFraction"), static_cast<float>(CardRefreshFraction));
    }
    if (RadiosityIterations >= 0)
    {
        SetCVarInt(TEXT("r.Lumen.Scene.Radiosity.PropagationIterations"), RadiosityIterations);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleSetLumenReflectionQuality(const TSharedPtr<FJsonObject>& Params)
{
    int32 MaxBounces = -1;
    int32 ScreenTraceIterations = -1;
    Params->TryGetNumberField(TEXT("max_bounces"), MaxBounces);
    Params->TryGetNumberField(TEXT("screen_trace_iterations"), ScreenTraceIterations);

    if (MaxBounces >= 0)
    {
        SetCVarInt(TEXT("r.Lumen.Reflections.MaxReflectionBounces"), MaxBounces);
    }
    if (ScreenTraceIterations >= 0)
    {
        SetCVarInt(TEXT("r.Lumen.Reflections.ScreenTraces.MaxIterations"), ScreenTraceIterations);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

// ------------------------------------------------------------------
// Hardware Ray Tracing
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleSetHardwareRayTracing(const TSharedPtr<FJsonObject>& Params)
{
    if (IsInPIE())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot change ray tracing settings while in PIE. Stop PIE first."));
    }

    bool bEnabled = true;
    if (!Params->TryGetBoolField(TEXT("enabled"), bEnabled))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'enabled' parameter"));
    }

    int32 Value = bEnabled ? 1 : 0;
    SetCVarInt(TEXT("r.RayTracing.Enable"), Value);
    return SetCVarInt(TEXT("r.Lumen.HardwareRayTracing"), Value);
}

// ------------------------------------------------------------------
// Path Tracing
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleSetPathTracing(const TSharedPtr<FJsonObject>& Params)
{
    bool bEnabled = true;
    Params->TryGetBoolField(TEXT("enabled"), bEnabled);

    int32 MaxBounces = -1;
    Params->TryGetNumberField(TEXT("max_bounces"), MaxBounces);

    SetCVarInt(TEXT("r.PathTracing.Enable"), bEnabled ? 1 : 0);
    if (MaxBounces >= 0)
    {
        SetCVarInt(TEXT("r.PathTracing.MaxBounces"), MaxBounces);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

// ------------------------------------------------------------------
// Virtual Shadow Maps
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleSetVirtualShadowMaps(const TSharedPtr<FJsonObject>& Params)
{
    bool bEnabled = true;
    Params->TryGetBoolField(TEXT("enabled"), bEnabled);

    double ResolutionLodBias = 0.0;
    Params->TryGetNumberField(TEXT("resolution_lod_bias"), ResolutionLodBias);

    SetCVarInt(TEXT("r.Shadow.Virtual.Enable"), bEnabled ? 1 : 0);
    SetCVarFloat(TEXT("r.Shadow.Virtual.ResolutionLodBias"), static_cast<float>(ResolutionLodBias));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

// ------------------------------------------------------------------
// Shadow Quality
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleSetShadowQuality(const TSharedPtr<FJsonObject>& Params)
{
    int32 MaxCascades = -1;
    double DistanceScale = 0.0;
    Params->TryGetNumberField(TEXT("max_cascades"), MaxCascades);
    Params->TryGetNumberField(TEXT("distance_scale"), DistanceScale);

    if (MaxCascades >= 0)
    {
        SetCVarInt(TEXT("r.Shadow.CSM.MaxCascades"), MaxCascades);
    }
    if (DistanceScale > 0.0)
    {
        SetCVarFloat(TEXT("r.Shadow.DistanceScale"), static_cast<float>(DistanceScale));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

// ------------------------------------------------------------------
// Anti-Aliasing
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleSetAntiAliasing(const TSharedPtr<FJsonObject>& Params)
{
    FString Method;
    if (!Params->TryGetStringField(TEXT("method"), Method))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'method' parameter. Use: None, FXAA, TAA, TSR, MSAA"));
    }

    int32 Value = 0;
    if (Method.Equals(TEXT("None"), ESearchCase::IgnoreCase)) Value = 0;
    else if (Method.Equals(TEXT("FXAA"), ESearchCase::IgnoreCase)) Value = 1;
    else if (Method.Equals(TEXT("TAA"), ESearchCase::IgnoreCase)) Value = 2;
    else if (Method.Equals(TEXT("TSR"), ESearchCase::IgnoreCase)) Value = 3;
    else if (Method.Equals(TEXT("MSAA"), ESearchCase::IgnoreCase)) Value = 4;
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown anti-aliasing method: %s"), *Method));
    }

    return SetCVarInt(TEXT("r.AntiAliasingMethod"), Value);
}

// ------------------------------------------------------------------
// TSR
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleSetTSRSettings(const TSharedPtr<FJsonObject>& Params)
{
    FString Algorithm;
    Params->TryGetStringField(TEXT("algorithm"), Algorithm);

    double HistoryScreenPercentage = -1.0;
    Params->TryGetNumberField(TEXT("history_screen_percentage"), HistoryScreenPercentage);

    if (!Algorithm.IsEmpty())
    {
        int32 Value = 0;
        if (Algorithm.Equals(TEXT("Gen4"), ESearchCase::IgnoreCase)) Value = 0;
        else if (Algorithm.Equals(TEXT("Gen5"), ESearchCase::IgnoreCase)) Value = 1;
        else
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown TSR algorithm: %s"), *Algorithm));
        }
        SetCVarInt(TEXT("r.TemporalAA.Algorithm"), Value);
    }

    if (HistoryScreenPercentage >= 0.0)
    {
        SetCVarFloat(TEXT("r.TemporalAA.HistoryScreenPercentage"), static_cast<float>(HistoryScreenPercentage));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

// ------------------------------------------------------------------
// Upscaler (DLSS / FSR / XeSS / NIS)
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleSetUpscaler(const TSharedPtr<FJsonObject>& Params)
{
    FString Upscaler;
    if (!Params->TryGetStringField(TEXT("upscaler"), Upscaler))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'upscaler' parameter. Use: DLSS, FSR, XeSS, NIS"));
    }

    bool bEnabled = true;
    Params->TryGetBoolField(TEXT("enabled"), bEnabled);

    int32 Value = bEnabled ? 1 : 0;

    if (Upscaler.Equals(TEXT("NIS"), ESearchCase::IgnoreCase))
    {
        return SetCVarInt(TEXT("r.NIS.Enable"), Value);
    }
    else if (Upscaler.Equals(TEXT("FSR"), ESearchCase::IgnoreCase) || Upscaler.Equals(TEXT("FidelityFX"), ESearchCase::IgnoreCase))
    {
        return SetCVarInt(TEXT("r.FidelityFX.FSR.Enabled"), Value);
    }
    else if (Upscaler.Equals(TEXT("XeSS"), ESearchCase::IgnoreCase))
    {
        return SetCVarInt(TEXT("r.XeSS.Enabled"), Value);
    }
    else if (Upscaler.Equals(TEXT("DLSS"), ESearchCase::IgnoreCase))
    {
        return CreateCVarResult(TEXT("r.NVIDIA.DLSS.Enable"), false, TEXT("DLSS control via CVar is platform-specific. Use project settings or NVIDIA plugin APIs."));
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown upscaler: %s"), *Upscaler));
}

// ------------------------------------------------------------------
// Nanite Visualization
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleSetNaniteVisualization(const TSharedPtr<FJsonObject>& Params)
{
    FString Mode;
    if (!Params->TryGetStringField(TEXT("mode"), Mode))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mode' parameter. Use: Off, Clusters, Triangles"));
    }

    int32 Value = 0;
    if (Mode.Equals(TEXT("Off"), ESearchCase::IgnoreCase)) Value = 0;
    else if (Mode.Equals(TEXT("Clusters"), ESearchCase::IgnoreCase)) Value = 1;
    else if (Mode.Equals(TEXT("Triangles"), ESearchCase::IgnoreCase)) Value = 2;
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown Nanite visualization mode: %s"), *Mode));
    }

    return SetCVarInt(TEXT("r.Nanite.Visualize"), Value);
}

// ------------------------------------------------------------------
// Shader Compile Status
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleGetShaderCompileStatus(const TSharedPtr<FJsonObject>& Params)
{
    int32 RemainingJobs = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
    bool bIsCompiling = GShaderCompilingManager ? GShaderCompilingManager->IsCompilingShaderMap(0) : false;

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("remaining_jobs"), RemainingJobs);
    Result->SetBoolField(TEXT("is_compiling"), bIsCompiling);
    Result->SetStringField(TEXT("status"), bIsCompiling ? TEXT("compiling") : (RemainingJobs > 0 ? TEXT("queued") : TEXT("idle")));
    return Result;
}
