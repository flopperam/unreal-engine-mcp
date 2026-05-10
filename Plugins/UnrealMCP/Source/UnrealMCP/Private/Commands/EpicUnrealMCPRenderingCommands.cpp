#include "Commands/EpicUnrealMCPRenderingCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "HAL/IConsoleManager.h"
#include "Editor.h"
#include "ShaderCompiler.h"
#include "Engine/PostProcessVolume.h"
#include "Camera/CameraActor.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Kismet/GameplayStatics.h"

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
        {TEXT("set_post_process_volume"), &FEpicUnrealMCPRenderingCommands::HandleSetPostProcessVolume},
        {TEXT("spawn_camera_actor"), &FEpicUnrealMCPRenderingCommands::HandleSpawnCameraActor},
        {TEXT("spawn_cine_camera_actor"), &FEpicUnrealMCPRenderingCommands::HandleSpawnCineCameraActor},
        {TEXT("set_camera_properties"), &FEpicUnrealMCPRenderingCommands::HandleSetCameraProperties},
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

// ------------------------------------------------------------------
// Post Process Volume
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleSetPostProcessVolume(const TSharedPtr<FJsonObject>& Params)
{
    FString VolumeName;
    if (!Params->TryGetStringField(TEXT("volume_name"), VolumeName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'volume_name' parameter"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
    }

    APostProcessVolume* TargetVolume = nullptr;
    for (TActorIterator<APostProcessVolume> It(World); It; ++It)
    {
        if (It->GetName() == VolumeName)
        {
            TargetVolume = *It;
            break;
        }
    }

    if (!TargetVolume)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("PostProcessVolume not found: %s"), *VolumeName));
    }

    FPostProcessSettings& Settings = TargetVolume->Settings;

    FString ExposureMethod;
    if (Params->TryGetStringField(TEXT("exposure_method"), ExposureMethod))
    {
        if (ExposureMethod.Equals(TEXT("Manual"), ESearchCase::IgnoreCase))
        {
            Settings.AutoExposureMethod = AEM_Manual;
        }
        else if (ExposureMethod.Equals(TEXT("AutoHistogram"), ESearchCase::IgnoreCase))
        {
            Settings.AutoExposureMethod = AEM_AutoHistogram;
        }
        else if (ExposureMethod.Equals(TEXT("AutoBasic"), ESearchCase::IgnoreCase))
        {
            Settings.AutoExposureMethod = AEM_AutoBasic;
        }
        else
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown exposure method: %s"), *ExposureMethod));
        }
    }

    double ExposureBias = 0.0;
    if (Params->TryGetNumberField(TEXT("exposure_bias"), ExposureBias))
    {
        Settings.AutoExposureBias = static_cast<float>(ExposureBias);
    }

    double BloomIntensity = 0.0;
    if (Params->TryGetNumberField(TEXT("bloom_intensity"), BloomIntensity))
    {
        Settings.BloomIntensity = static_cast<float>(BloomIntensity);
    }

    double BloomThreshold = 0.0;
    if (Params->TryGetNumberField(TEXT("bloom_threshold"), BloomThreshold))
    {
        Settings.BloomThreshold = static_cast<float>(BloomThreshold);
    }

    bool bDofEnabled = false;
    if (Params->TryGetBoolField(TEXT("dof_enabled"), bDofEnabled))
    {
        Settings.bOverride_DepthOfFieldFocalDistance = true;
        Settings.bOverride_DepthOfFieldFstop = true;
        Settings.bOverride_DepthOfFieldDepthBlurRadius = true;
        Settings.bOverride_DepthOfFieldDepthBlurAmount = true;
        Settings.DepthOfFieldFocalDistance = bDofEnabled ? 1000.0f : 0.0f;
        Settings.DepthOfFieldFstop = bDofEnabled ? 2.8f : 0.0f;
    }

    double DofFocalDistance = 0.0;
    if (Params->TryGetNumberField(TEXT("dof_focal_distance"), DofFocalDistance))
    {
        Settings.bOverride_DepthOfFieldFocalDistance = true;
        Settings.DepthOfFieldFocalDistance = static_cast<float>(DofFocalDistance);
    }

    double DofAperture = 0.0;
    if (Params->TryGetNumberField(TEXT("dof_aperture"), DofAperture))
    {
        Settings.bOverride_DepthOfFieldFstop = true;
        Settings.DepthOfFieldFstop = static_cast<float>(DofAperture);
    }

    double ColorTemperature = 0.0;
    if (Params->TryGetNumberField(TEXT("color_temperature"), ColorTemperature))
    {
        Settings.bOverride_WhiteTemp = true;
        Settings.WhiteTemp = static_cast<float>(ColorTemperature);
    }

    double ColorTint = 0.0;
    if (Params->TryGetNumberField(TEXT("color_tint"), ColorTint))
    {
        Settings.bOverride_WhiteTint = true;
        Settings.WhiteTint = static_cast<float>(ColorTint);
    }

    double MotionBlurAmount = 0.0;
    if (Params->TryGetNumberField(TEXT("motion_blur_amount"), MotionBlurAmount))
    {
        Settings.bOverride_MotionBlurAmount = true;
        Settings.MotionBlurAmount = static_cast<float>(MotionBlurAmount);
    }

    double AoIntensity = 0.0;
    if (Params->TryGetNumberField(TEXT("ao_intensity"), AoIntensity))
    {
        Settings.bOverride_AmbientOcclusionIntensity = true;
        Settings.AmbientOcclusionIntensity = static_cast<float>(AoIntensity);
    }

    double AoRadius = 0.0;
    if (Params->TryGetNumberField(TEXT("ao_radius"), AoRadius))
    {
        Settings.bOverride_AmbientOcclusionRadius = true;
        Settings.AmbientOcclusionRadius = static_cast<float>(AoRadius);
    }

    double VignetteIntensity = 0.0;
    if (Params->TryGetNumberField(TEXT("vignette_intensity"), VignetteIntensity))
    {
        Settings.bOverride_VignetteIntensity = true;
        Settings.VignetteIntensity = static_cast<float>(VignetteIntensity);
    }

    double FilmGrainIntensity = 0.0;
    if (Params->TryGetNumberField(TEXT("film_grain_intensity"), FilmGrainIntensity))
    {
        Settings.bOverride_FilmGrainIntensity = true;
        Settings.FilmGrainIntensity = static_cast<float>(FilmGrainIntensity);
    }

    double ChromaticAberration = 0.0;
    if (Params->TryGetNumberField(TEXT("chromatic_aberration"), ChromaticAberration))
    {
        Settings.bOverride_SceneFringeIntensity = true;
        Settings.SceneFringeIntensity = static_cast<float>(ChromaticAberration);
    }

    double LensFlareIntensity = 0.0;
    if (Params->TryGetNumberField(TEXT("lens_flare_intensity"), LensFlareIntensity))
    {
        Settings.bOverride_LensFlareIntensity = true;
        Settings.LensFlareIntensity = static_cast<float>(LensFlareIntensity);
    }

    TargetVolume->MarkDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("volume_name"), VolumeName);
    return Result;
}

// ------------------------------------------------------------------
// Camera Actor
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleSpawnCameraActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName) || ActorName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
    }

    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FString ParamError;
    if (Params->HasField(TEXT("location")))
    {
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("location"), Location, ParamError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'location': %s"), *ParamError));
        }
    }
    if (Params->HasField(TEXT("rotation")))
    {
        if (!FEpicUnrealMCPCommonUtils::TryGetRotatorFromJson(Params, TEXT("rotation"), Rotation, ParamError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'rotation': %s"), *ParamError));
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

    ACameraActor* Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
    if (!Camera)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn CameraActor"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor_name"), Camera->GetName());
    Result->SetStringField(TEXT("actor_class"), Camera->GetClass()->GetName());
    Result->SetStringField(TEXT("actor_type"), TEXT("CameraActor"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleSpawnCineCameraActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName) || ActorName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
    }

    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FString ParamError;
    if (Params->HasField(TEXT("location")))
    {
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("location"), Location, ParamError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'location': %s"), *ParamError));
        }
    }
    if (Params->HasField(TEXT("rotation")))
    {
        if (!FEpicUnrealMCPCommonUtils::TryGetRotatorFromJson(Params, TEXT("rotation"), Rotation, ParamError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'rotation': %s"), *ParamError));
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

    ACineCameraActor* Camera = World->SpawnActor<ACineCameraActor>(ACineCameraActor::StaticClass(), Location, Rotation, SpawnParams);
    if (!Camera)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn CineCameraActor"));
    }

    UCineCameraComponent* CineComp = Camera->GetCineCameraComponent();
    if (CineComp)
    {
        double FocalLength = 0.0;
        if (Params->TryGetNumberField(TEXT("focal_length"), FocalLength) && FocalLength > 0.0)
        {
            CineComp->SetCurrentFocalLength(static_cast<float>(FocalLength));
        }
        double Aperture = 0.0;
        if (Params->TryGetNumberField(TEXT("aperture"), Aperture) && Aperture > 0.0)
        {
            CineComp->SetCurrentAperture(static_cast<float>(Aperture));
        }
        double FocusDistance = 0.0;
        if (Params->TryGetNumberField(TEXT("focus_distance"), FocusDistance) && FocusDistance >= 0.0)
        {
            CineComp->SetFocusDistance(static_cast<float>(FocusDistance));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor_name"), Camera->GetName());
    Result->SetStringField(TEXT("actor_class"), Camera->GetClass()->GetName());
    Result->SetStringField(TEXT("actor_type"), TEXT("CineCameraActor"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPRenderingCommands::HandleSetCameraProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName) || ActorName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
    }

    AActor* TargetActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UCineCameraComponent* CineComp = nullptr;
    if (ACineCameraActor* CineCamera = Cast<ACineCameraActor>(TargetActor))
    {
        CineComp = CineCamera->GetCineCameraComponent();
    }
    else if (ACameraActor* Camera = Cast<ACameraActor>(TargetActor))
    {
        // Regular CameraActor doesn't have CineCameraComponent
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("CameraActor does not support CineCameraComponent properties. Use CineCameraActor instead."));
    }
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Actor is not a CameraActor or CineCameraActor"));
    }

    if (!CineComp)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("CineCameraComponent not found on actor"));
    }

    double FocalLength = 0.0;
    if (Params->TryGetNumberField(TEXT("focal_length"), FocalLength) && FocalLength > 0.0)
    {
        CineComp->SetCurrentFocalLength(static_cast<float>(FocalLength));
    }
    double Aperture = 0.0;
    if (Params->TryGetNumberField(TEXT("aperture"), Aperture) && Aperture > 0.0)
    {
        CineComp->SetCurrentAperture(static_cast<float>(Aperture));
    }
    double FocusDistance = 0.0;
    if (Params->TryGetNumberField(TEXT("focus_distance"), FocusDistance) && FocusDistance >= 0.0)
    {
        CineComp->SetFocusDistance(static_cast<float>(FocusDistance));
    }

    TargetActor->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor_name"), ActorName);
    Result->SetNumberField(TEXT("focal_length"), CineComp->CurrentFocalLength);
    Result->SetNumberField(TEXT("aperture"), CineComp->CurrentAperture);
    Result->SetNumberField(TEXT("focus_distance"), CineComp->CurrentFocusDistance);
    return Result;
}
