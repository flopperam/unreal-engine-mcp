// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class FlopperamUnrealMCPEditorTarget : TargetRules
{
	public FlopperamUnrealMCPEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;

		// Use Latest to automatically match the engine's default build and include order settings
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("FlopperamUnrealMCP");
	}
}
